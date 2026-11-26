// SPDX-License-Identifier: GPL-2.0-or-later
#include "comparison_parameters.h"
#include "document/hydrostatics.h"
#include "ui_language.h"
#include "studio_ui.h"
#include "studio_table.h"
#include "solve_status_table.h"
#include "table_number.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QPromise>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QVBoxLayout>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace {
enum Column { StatusColumn, ModelColumn, FirstMetricColumn };
struct Metric {const char* key;const char* label;const char* unit;};
constexpr Metric metrics[]={
    {"length_m","船体总长","m"},{"beam_m","最大宽度","m"},
    {"draft_m","吃水","m"},{"trim_m","纵倾差","m"},{"heel_deg","横倾角 (°)",""},
    {"draft_aft_m","艉垂线吃水","m"},{"draft_forward_m","艏垂线吃水","m"},
    {"mass_kg","排水质量","kg"},{"volume_m3","排水体积","m³"},
    {"lwl_m","水线长","m"},{"bwl_m","水线宽","m"},
    {"wetted_area_m2","湿表面积","m²"},{"cp","棱形系数",""},{"lcb_m","浮心 X","m"}
};
}

struct ComparisonParameters::Solvers {
    struct Hull {
        // Only one request at a time may use this hull's numerical caches.
        std::mutex mutex;
        std::unique_ptr<fs::document::Hydrostatics> solver;
    };
    explicit Solvers(size_t count):hulls(count){}
    std::vector<Hull> hulls;
};

ComparisonParameters::ComparisonParameters(QString currentName,double designDraftM,QWidget* parent):QWidget(parent){
    auto* layout=new QVBoxLayout(this);auto* fields=new QGridLayout;layout->addLayout(fields);
    mode_=new QComboBox;mode_->setObjectName("ComparisonMode");
    addUiItem(mode_,"固定吃水");addUiItem(mode_,"固定装载");
    fields->addWidget(mode_,0,0,1,2);
    auto field=[&](const char* key,const char* label,const char* unit,int row,int col,double min,double max,double value){
        auto* title=new QLabel(uiText(label));auto* box=new QDoubleSpinBox;
        box->setObjectName(key);box->setDecimals(5);box->setRange(min,max);box->setValue(value);
        box->setKeyboardTracking(false);box->setSuffix(QStringLiteral(" ")+QString::fromUtf8(unit));
        fields->addWidget(title,row,col);fields->addWidget(box,row,col+1);
        if(row>0){
            const bool load=row>=2;
            auto visibility=[=](int index){box->setVisible((index==1)==load);title->setVisible((index==1)==load);};
            visibility(0);connect(mode_,&QComboBox::currentIndexChanged,box,visibility);
        }
        connect(box,&QDoubleSpinBox::valueChanged,this,[this]{invalidate();});return box;
    };
    density_=field("ComparisonDensity","水密度","kg/m³",0,2,.00001,1e5,1025);
    draft_=field("ComparisonDraft","吃水","m",1,0,0,1e5,designDraftM);
    trim_=field("ComparisonTrim","纵倾差","m",1,2,-1e5,1e5,0);
    mass_=field("ComparisonMass","总质量","kg",2,0,.00001,1e12,1);
    lcg_=field("ComparisonLCG","重心 X","m",2,2,-1e5,1e5,0);
    tcg_=field("ComparisonTCG","重心 Y","m",3,2,-1e5,1e5,0);
    kg_=field("ComparisonKG","重心 Z","m",3,4,-1e5,1e5,0);
    connect(mass_,&QDoubleSpinBox::valueChanged,this,[this]{seedLoad_=false;});
    connect(lcg_,&QDoubleSpinBox::valueChanged,this,[this]{seedLoad_=false;});
    calculate_=new QPushButton(uiText("计算"));calculate_->setObjectName("ComparisonCalculate");fields->addWidget(calculate_,0,5);
    connect(calculate_,&QPushButton::clicked,this,&ComparisonParameters::calculate);
    connect(mode_,&QComboBox::currentIndexChanged,this,[this]{invalidate();});
    table_=new SolveStatusTable;table_->setColumnCount(FirstMetricColumn+int(std::size(metrics)));
    table_->setObjectName("ComparisonParameters");
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    applyStudioScrollbars(table_);
    const auto refreshHeader=[this]{
        table_->setHorizontalHeaderItem(StatusColumn,new QTableWidgetItem(uiText("求解状态")));
        table_->setHorizontalHeaderItem(ModelColumn,new QTableWidgetItem(uiText("船模")));
        int aftColumn=-1;
        for(int col=FirstMetricColumn;col<table_->columnCount();++col){
            const auto& metric=metrics[col-FirstMetricColumn];QString title=uiText(metric.label);
            if(qstrcmp(metric.key,"draft_aft_m")==0)aftColumn=col;
            if(*metric.unit)title+=QString(" (%1)").arg(QString::fromUtf8(metric.unit));
            auto* item=new QTableWidgetItem(title);item->setData(Qt::UserRole,metric.key);table_->setHorizontalHeaderItem(col,item);
        }
        if(aftColumn>=0)setStudioHeaderGroups(table_,{
            {QString(uiText("垂线吃水 (%1)")).arg("m"),aftColumn,{uiText("艉"),uiText("艏")}}
        });
    };
    refreshHeader();bindUiRefresh(table_,refreshHeader);
    configureStudioDataTable(table_,true);
    layout->addWidget(table_,1);status_=new QLabel;status_->setObjectName("ComparisonProgress");
    status_->setWordWrap(true);layout->addWidget(status_);
    setModels({{std::move(currentName),{},{}}},true);
}
ComparisonParameters::~ComparisonParameters(){
    if(job_)QFile(job_->filePath("cancel")).open(QIODevice::WriteOnly);
}
void ComparisonParameters::invalidate(){
    if(job_){QFile(job_->filePath("cancel")).open(QIODevice::WriteOnly);job_.reset();}
    table_->setProperty("ready",false);status_->clear();status_->hide();
    for(int row=0;row<table_->rowCount();++row){
        delete table_->takeItem(row,StatusColumn);
        for(int col=FirstMetricColumn;col<table_->columnCount();++col)delete table_->takeItem(row,col);
    }
    calculate_->setEnabled(!pending_ && hulls_.size()>1 && hulls_.first().model);
}
void ComparisonParameters::setModels(QVector<ComparisonHull> hulls,bool pending){
    hulls_=std::move(hulls);pending_=pending;
    solvers_=std::make_shared<Solvers>(size_t(hulls_.size()));
    table_->setRowCount(hulls_.size());invalidate();
    for(int row=0;row<hulls_.size();++row){
        const auto& label=hulls_[row].label;
        auto* item=new QTableWidgetItem(label);
        if(!row){
            QFont font=table_->font();font.setBold(true);item->setFont(font);
            item->setToolTip(uiText("当前船模"));
        }
        table_->setItem(row,ModelColumn,item);
    }
    if(pending_){status_->setText(uiText("正在读取横剖线…"));status_->show();}
    else if(calculate_->isEnabled())calculate();
    else for(int row=0;row<hulls_.size();++row)if(!hulls_[row].error.isEmpty()){
        const auto error=uiText(hulls_[row].error);
        table_->setSolveStatus(row,false,error,error);
    }
}
void ComparisonParameters::calculate(){
    invalidate();if(!calculate_->isEnabled())return;
    auto dir=std::make_shared<QTemporaryDir>();
    if(!dir->isValid()){status_->setText(uiText("无法创建计算快照目录。"));status_->show();return;}
    job_=dir;calculate_->setEnabled(false);status_->setText(uiText("正在计算…"));status_->show();
    fs::document::ComparisonConditions input;input.equilibrium=mode_->currentIndex()==1;
    input.draftM=draft_->value();input.trimM=trim_->value();input.densityKgM3=density_->value();
    input.massKg=mass_->value();input.gravityM={lcg_->value(),tcg_->value(),kg_->value()};
    using Result=QVector<QJsonObject>;
    auto promise=std::make_shared<QPromise<Result>>();auto* watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,dir,input]{
        watcher->deleteLater();if(dir!=job_)return;
        const auto result=watcher->result();job_.reset();
        const auto current=result.first()["values"].toObject();
        for(int row=0;row<table_->rowCount();++row){
            const auto error=uiText(result[row]["error"].toString());
            table_->setSolveStatus(row,error.isEmpty(),error.isEmpty()?uiText("已求解"):error,error);
            const auto values=result[row]["values"].toObject();
            for(int col=FirstMetricColumn;col<table_->columnCount();++col){
                const auto& m=metrics[col-FirstMetricColumn];const auto v=values[m.key],baseline=current[m.key];
                auto* cell=new QTableWidgetItem;
                if(v.isDouble() && std::isfinite(v.toDouble())){
                    cell->setText(tableNumber::display(v.toDouble()));cell->setData(Qt::UserRole,v.toDouble());
                    if(row>0&&baseline.isDouble()){
                        const double difference=baseline.toDouble()-v.toDouble();
                        cell->setToolTip(QString::fromUtf8(uiText("差值（当前 − 参考）"))+QStringLiteral("  ")+
                            tableNumber::display(difference)+(*m.unit?QStringLiteral(" ")+QString::fromUtf8(m.unit):QString{}));
                    }
                }else cell->setText(QStringLiteral("—"));
                cell->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);table_->setItem(row,col,cell);
            }
        }
        if(!input.equilibrium && seedLoad_ && current["mass_kg"].toDouble()>0 && current["lcb_m"].isDouble()){
            QSignalBlocker a(mass_),b(lcg_);mass_->setValue(current["mass_kg"].toDouble());lcg_->setValue(current["lcb_m"].toDouble());
        }
        status_->clear();status_->hide();table_->setProperty("ready",true);
        calculate_->setEnabled(true);
    });
    watcher->setFuture(promise->future());
    auto* pool=QThreadPool::globalInstance();
    const int workers=std::clamp(pool->maxThreadCount(),1,2);
    auto remaining=std::make_shared<std::atomic_int>(workers);
    auto rows=std::make_shared<std::vector<QJsonObject>>(size_t(hulls_.size()));
    const auto cancel=dir->filePath("cancel");
    promise->start();
    for(int worker=0;worker<workers;++worker){
        pool->start([promise,dir,input,hulls=hulls_,solvers=solvers_,rows,remaining,workers,worker,cancel]{
            for(qsizetype row=worker;row<hulls.size();row+=workers){
                auto& state=solvers->hulls[size_t(row)];
                const std::lock_guard lock(state.mutex);
                if(QFile::exists(cancel))break;
                const auto& hull=hulls[row];
                auto& result=(*rows)[size_t(row)];
                if(!hull.model){
                    result={{"error",hull.error}};
                    continue;
                }
                try {
                    if(!state.solver)state.solver=std::make_unique<fs::document::Hydrostatics>(*hull.model);
                    result=state.solver->comparison(input,cancel);
                }catch(const std::exception& e){
                    result={{"error",QString::fromUtf8(e.what())}};
                }
            }
            if(remaining->fetch_sub(1)==1){
                Result result;
                result.reserve(qsizetype(rows->size()));
                for(auto& row:*rows)result.append(std::move(row));
                promise->addResult(std::move(result));promise->finish();
            }
        });
    }
}
