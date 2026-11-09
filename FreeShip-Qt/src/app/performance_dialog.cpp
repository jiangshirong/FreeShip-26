#include "preferences_store.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "performance_dialog.h"
#include "studio_ui.h"
#include "calculation_pages.h"
#include "calculation_plot.h"
#include "core/water_viscosity.h"
#include <QInputDialog>
#include "grid_preview_panel.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QMenu>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QTextDocument>
#include <QTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QJsonObject parameterPages(const QString& method){
    static const auto pages=QJsonDocument::fromJson(CalculationPages).object();return pages[method].toObject();
}
QString full(double n){return QString::number(n,'g',17);}
QString quote(QString s){return '"'+s.replace('"',"\"\"")+'"';}
class PerformanceTable final:public QAbstractTableModel {
    QJsonArray points_,rowHeaders_;QStringList headers_;
public:
    explicit PerformanceTable(QObject* parent):QAbstractTableModel(parent){}
    void setSeries(const QJsonObject& s){beginResetModel();rowHeaders_={};points_=s["points"].toArray();headers_={s["x_title"].toString(),s["name"].toString()+" · "+s["y_title"].toString()};endResetModel();}
    void setTable(const QJsonObject& t){beginResetModel();points_=t["values"].toArray();rowHeaders_=t["rows"].toArray();headers_.clear();for(auto v:t["columns"].toArray())headers_<<v.toVariant().toString();endResetModel();}
    int rowCount(const QModelIndex& parent={})const override{return parent.isValid()?0:int(points_.size());}
    int columnCount(const QModelIndex& parent={})const override{return parent.isValid()?0:headers_.size();}
    QVariant data(const QModelIndex& i,int role)const override{if(!i.isValid()||i.row()>=points_.size()||i.column()>=headers_.size()||(role!=Qt::DisplayRole&&role!=Qt::ToolTipRole))return {};auto row=points_[i.row()].toArray();if(i.column()>=row.size())return {};auto value=row[i.column()];if(value.isNull())return QString("未定义");return QString::number(value.toDouble(),'g',role==Qt::ToolTipRole?17:10);}
    QVariant headerData(int section,Qt::Orientation orientation,int role)const override{if(role!=Qt::DisplayRole)return {};if(orientation==Qt::Vertical)return section<rowHeaders_.size()?rowHeaders_[section].toVariant():QVariant(section+1);return section<headers_.size()?QVariant(headers_[section]):QVariant{};}
};
}
QString PerformanceDialog::defaultSolverDirectory(){
    if(!preferenceDirectory("solver").isEmpty())return preferenceDirectory("solver");
    auto path=QSettings().value("calculation/solver-directory").toString();if(!path.isEmpty())return path;
    QFile config(QCoreApplication::applicationDirPath()+"/calculation-tools.json");
    if(config.open(QIODevice::ReadOnly)&&config.size()<65536)path=QJsonDocument::fromJson(config.readAll()).object()["directory"].toString();
    return path;
}
PerformanceDialog::PerformanceDialog(ModelSession& session,const QJsonObject& schema,bool hidden,QWidget* parent)
    :QDialog(parent),session_(session),method_(schema["calculator"].toString()),hidden_(hidden){
    if(method_=="acceleration")CalculationPlot::prepareRenderer();
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setWindowTitle(schema["title"].toString());resize(1180,760);setMinimumSize(740,480);
    auto* outer=new QVBoxLayout(this);auto* toolbar=new QHBoxLayout;outer->addLayout(toolbar);toolbar->addStretch();
    pages_=new QTabWidget;pages_->setProperty("calculation_pages",true);outer->addWidget(pages_,1);
    auto* split=new QSplitter;split->setProperty("calculation_page","general");pages_->addTab(split,"常规");
    additional_=new QScrollArea;additional_->setWidgetResizable(true);additional_->setFrameShape(QFrame::NoFrame);
    additional_->setProperty("calculation_page","additional");pages_->addTab(additional_,"附加数据");
    additional_->viewport()->installEventFilter(this);
    auto* left=new QWidget;auto* form=new QVBoxLayout(left);form->setContentsMargins(0,0,5,0);
    auto* label=new QLabel(method_=="acceleration"?"请先计算当前船模的阻力。提取推进参数还需同一版本的推进结果；也可手动填写。":method_=="wind"?"提取总长和水线宽；受风面积、上层建筑及桅杆资料请手动核对。":method_.startsWith("propeller")?"提取参数会读取同一船模当前工况下的阻力结果。":"可从当前船模提取主尺度，也可手动填写。计算方法的适用范围见结果报告。");label->setWordWrap(true);form->addWidget(label);
    parameters_=new QScrollArea;parameters_->setWidgetResizable(true);parameters_->setFrameShape(QFrame::NoFrame);form->addWidget(parameters_,1);
    auto* dir=new QHBoxLayout;solver_=new QLineEdit;solver_->setPlaceholderText("外部计算程序目录（内置方法可留空）");solver_->setProperty("performance_directory",true);
    const auto configured=hidden?QString{}:defaultSolverDirectory();solver_->setText(configured.isEmpty()?schema["solver_directory"].toString():configured);
    dir->addWidget(solver_);auto* browse=new QPushButton("计算程序…");browse->setIcon(studioIcon("OpenFile"));dir->addWidget(browse);
    auto* solverRow=new QWidget;solverRow->setLayout(dir);form->addWidget(solverRow);
    solverRow->setVisible(method_.startsWith("propeller")||method_=="maneuver"||method_=="acceleration"||method_=="addmass");
    connect(browse,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getExistingDirectory(this,"选择原版计算程序所在的 Exec 目录",solver_->text());if(!path.isEmpty())solver_->setText(path);});
    auto button=[&](const QString& title,const QString& operation){auto* b=new QPushButton(title);b->setProperty("performance_action",operation);return b;};
    extract_=button("提取参数","extract");extract_->setIcon(studioIcon("Hydrostatics"));form->insertWidget(1,extract_);
    run_=button("计算","calculate");run_->setIcon(studioIcon("Calculate"));run_->setDefault(true);
    cancel_=button("取消计算","cancel");cancel_->setIcon(studioIcon("Cancel"));toolbar->addWidget(run_);toolbar->addWidget(cancel_);
    save_=button("将参数保存到船模","save_settings");save_->setIcon(studioIcon("SaveFile"));left->setMinimumWidth(300);split->addWidget(left);
    if(method_=="propeller4"){
        databaseButton_=button("数据库查看器…","database_viewer");form->addWidget(databaseButton_);
        connect(databaseButton_,&QPushButton::clicked,this,&PerformanceDialog::openDatabaseViewer);
    }
    auto* presetRow=new QHBoxLayout;auto* presetSave=button("保存参数…","save_parameters");auto* presetLoad=button("载入参数…","load_parameters");
    presetSave->setIcon(studioIcon("SaveAs"));presetLoad->setIcon(studioIcon("OpenFile"));
    presetRow->addWidget(presetSave);presetRow->addWidget(presetLoad);presetRow->addWidget(save_);presetRow->addStretch();outer->addLayout(presetRow);
    auto* persistenceNote=new QLabel("船模格式仅保存部分计算参数；完整工况可另存为参数文件。");persistenceNote->setWordWrap(true);form->addWidget(persistenceNote);
    if(method_=="propeller4"||method_=="maneuver") {
        auto* temperature=button("按水温换算黏度…","water_temperature");form->addWidget(temperature);
        connect(temperature,&QPushButton::clicked,this,[this] {
            if(session_.busy())return;
            QInputDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,hidden_);
            dialog.setWindowTitle("水温换算");dialog.setLabelText("水温（°C，原版水体表 0–30°C）\n换算后仍可手动填写适合当前水体的运动黏度。");
            dialog.setInputMode(QInputDialog::DoubleInput);dialog.setDoubleRange(0,30);dialog.setDoubleDecimals(2);dialog.setDoubleValue(15);
            if(dialog.exec()!=QDialog::Accepted)return;
            const auto key=method_=="maneuver"?"Edit11":"WaterViscosity";
            if(auto* field=qobject_cast<QDoubleSpinBox*>(fields_.value(key))) {
                const double value=fs::waterViscosity(float(dialog.doubleValue()));field->setValue(value);
                field->setProperty("performance_exact_value",value);
                pages_->setCurrentIndex(0);
                parameters_->ensureWidgetVisible(field);
            }
        });
        if(method_=="propeller4")persistenceNote->setText("此方法的水体黏度不写入 FBM；请用“保存参数”保留完整工况，载入参数后可复算。");
    }
    connect(presetSave,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,"保存计算参数",preferenceFileLocation("save"),"计算参数 (*.json)");if(!path.isEmpty()){auto err=saveParameters(path);status_->setText(err.isEmpty()?"参数已保存。":err);}});
    connect(presetLoad,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"载入计算参数",preferenceFileLocation("open"),"计算参数 (*.json)");if(!path.isEmpty()){auto err=loadParameters(path);if(!err.isEmpty())showFailure(err);}});
    auto* right=new QWidget;auto* rightLayout=new QVBoxLayout(right);rightLayout->setContentsMargins(0,0,0,0);series_=new QComboBox;series_->setProperty("performance_series",true);rightLayout->addWidget(series_);
    auto* results=new QWidget;results->setProperty("calculation_page","results");auto* resultsLayout=new QVBoxLayout(results);resultsLayout->setContentsMargins(0,0,0,0);
    resultSeries_=new QComboBox;resultSeries_->setModel(series_->model());resultSeries_->setProperty("performance_result_series",true);resultsLayout->addWidget(resultSeries_);
    tabs_=new QTabWidget;resultsLayout->addWidget(tabs_,1);pages_->addTab(results,"结果");
    table_=new QTableView;table_->setProperty("performance_table",true);table_->setModel(new PerformanceTable(table_));table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(true);tabs_->addTab(table_,"曲线数据");
    plot_=new CalculationPlot;tabs_->addTab(plot_,"曲线");text_=new QPlainTextEdit;text_->setReadOnly(true);tabs_->addTab(text_,"报告与适用范围");
    if(method_=="maneuver"||method_=="addmass") {auto* report=new QPlainTextEdit;report->setReadOnly(true);report->setPlainText("计算完成后显示结果。完整数据与报告见“结果”页。");overview_=report;}
    else {auto* overview=new CalculationPlot;overview_=overview;auto* detail=static_cast<CalculationPlot*>(plot_);
        overview->onDisplayChanged=[detail]{detail->synchronize();};detail->onDisplayChanged=[overview]{overview->synchronize();};}
    rightLayout->addWidget(overview_,1);split->addWidget(right);split->setStretchFactor(1,1);split->setSizes({460,720});
    help_=new QPlainTextEdit;help_->setReadOnly(true);help_->setProperty("calculation_page","help");pages_->addTab(help_,"帮助");
    status_=new QLabel("设置参数后点击“计算”。");status_->setWordWrap(true);outer->addWidget(status_);
    auto* exportButton=new QPushButton("导出报告");exportButton->setIcon(studioIcon("SaveFile"));auto* exportMenu=new QMenu(exportButton);exportButton->setMenu(exportMenu);exports_.append(exportButton);toolbar->insertWidget(0,exportButton);
    for(const auto& format:{QString("csv"),QString("txt"),QString("json"),QString("pdf")}){
        auto* action=exportMenu->addAction(studioIcon(format=="pdf"?"Print":"SaveFile"),format.toUpper()+"…");action->setProperty("performance_export",format);
        connect(action,&QAction::triggered,this,[this,format]{const auto path=QFileDialog::getSaveFileName(this,"保存计算报告",preferenceFileLocation("export"),format.toUpper()+" (*."+format+")");if(path.isEmpty())return;const auto err=exportReport(path,format);status_->setText(err.isEmpty()?"报告已保存："+path:err);});
    }
    auto* close=button("关闭","close");close->setIcon(studioIcon("Cancel"));toolbar->addWidget(close);connect(close,&QPushButton::clicked,this,&PerformanceDialog::reject);
    connect(run_,&QPushButton::clicked,this,&PerformanceDialog::calculate);
    connect(cancel_,&QPushButton::clicked,this,[this]{session_.cancelPerformance();status_->setText("正在取消计算…");});
    connect(extract_,&QPushButton::clicked,this,[this]{auto request=inputs();request["extract"]=true;session_.performanceDefaults(request);});
    connect(save_,&QPushButton::clicked,this,[this]{session_.savePerformanceSettings(inputs());});
    connect(series_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int i){QSignalBlocker guard(resultSeries_);resultSeries_->setCurrentIndex(i);renderSeries();});
    connect(resultSeries_,qOverload<int>(&QComboBox::currentIndexChanged),series_,&QComboBox::setCurrentIndex);
    setParameters(schema);text_->setPlainText("计算完成后显示完整报告。");
    if(method_=="maneuver"){tabs_->setTabVisible(0,false);tabs_->setTabVisible(1,false);tabs_->setCurrentWidget(text_);series_->hide();resultSeries_->hide();}
    if(method_=="addmass"){tabs_->setTabText(0,"计算数据");tabs_->setTabVisible(1,false);save_->hide();persistenceNote->setText("此计算的工况另存为参数文件；不写入船模。");label->setText("从当前船型生成附加质量计算输入。参数使用公制；矩阵、等效椭球和经验系数分别展示。");}
    if(method_=="addmass"){preview_=new GridPreviewPanel(session_,"addmass",[this]{auto values=inputs()["values"].toObject();values["object_type"]=values["object_type"].toInt()+1;values["panel_type"]=values["panel_type"].toInt()+1;return values;});tabs_->addTab(preview_,"型值预览");
        for(auto* field:fields_){if(auto* n=qobject_cast<QDoubleSpinBox*>(field))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),preview_,[this]{preview_->invalidate();});else if(auto* c=qobject_cast<QComboBox*>(field))connect(c,qOverload<int>(&QComboBox::currentIndexChanged),preview_,[this]{preview_->invalidate();});}}
    refreshState();
}
void PerformanceDialog::setParameters(const QJsonObject& schema){
    additionalLayout_=nullptr;
    const int generalScroll=parameters_->verticalScrollBar()->value(),extraScroll=additional_->verticalScrollBar()->value();
    const quint64 generation=++parameterGeneration_;
    schema_=schema;fields_.clear();auto* body=new QWidget;auto* layout=new QVBoxLayout(body);QMap<QString,QGridLayout*> groups;
    auto* extraBody=new QWidget;auto* extraLayout=new QGridLayout(extraBody);additionalGroups_.clear();
    for(int i=0;i<2;++i){auto* column=new QWidget(extraBody);additionalColumnLayouts_[i]=new QVBoxLayout(column);additionalColumnLayouts_[i]->setContentsMargins(0,0,0,0);additionalColumnLayouts_[i]->addStretch();extraLayout->addWidget(column,0,i);}
    const auto pageMap=parameterPages(method_);bool hasAdditional=false;
    for(auto v:schema["fields"].toArray()){
        const auto f=v.toObject();const auto key=f["key"].toString(),group=f["group"].toString();
        const bool extra=pageMap[key]=="additional";const auto groupKey=(extra?"additional/":"general/")+group;
        if(!groups.contains(groupKey)){auto* box=new QGroupBox(group);auto* rows=new QGridLayout(box);rows->setColumnStretch(0,3);rows->setColumnStretch(1,4);groups[groupKey]=rows;
            if(extra){additionalColumnLayouts_[0]->insertWidget(additionalColumnLayouts_[0]->count()-1,box);additionalGroups_.append(box);hasAdditional=true;}else layout->addWidget(box);}
        QWidget* widget=nullptr;const auto kind=f["kind"].toString();
        if(kind=="number"){auto* n=new QDoubleSpinBox;n->setDecimals(std::max(0,std::min(12,f["decimals"].toInt())));n->setRange(f["minimum"].toDouble(),f["maximum"].toDouble());n->setValue(f["value"].toDouble());n->setProperty("performance_exact_value",f["value"].toDouble());n->setKeyboardTracking(false);widget=n;connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,n](double value){n->setProperty("performance_exact_value",double(float(value)));if(!result_.isEmpty())status_->setText("参数已更改，请重新计算；报告仍对应上一次完整计算。");});}
        else if(kind=="boolean"){auto* n=new QCheckBox;n->setChecked(f["value"].toBool());widget=n;connect(n,&QCheckBox::toggled,this,[this]{if(!result_.isEmpty())status_->setText("参数已更改，请重新计算。");});}
        else {auto* n=new QComboBox;n->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);n->setMinimumContentsLength(10);for(auto c:f["choices"].toArray())n->addItem(c.toString());n->setCurrentIndex(f["value"].toInt());widget=n;connect(n,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,key]{if(!result_.isEmpty())status_->setText("参数已更改，请重新计算。");if(method_=="propeller5"&&key=="ComboBox")refreshChoices();});}
        fields_[key]=widget;widget->setProperty("performance_field",key);
        widget->setProperty("calculation_parameter_page",extra?"additional":"general");widget->setProperty("calculation_declared_visible",f["visible"].toBool(true));
        if(preview_){if(auto* n=qobject_cast<QDoubleSpinBox*>(widget))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),preview_,[this]{preview_->invalidate();});else if(auto* c=qobject_cast<QComboBox*>(widget))connect(c,qOverload<int>(&QComboBox::currentIndexChanged),preview_,[this]{preview_->invalidate();});}
        widget->setEnabled(f["enabled"].toBool(true));
        auto title=f["label"].toString();if(!f["unit"].toString().isEmpty())title+=" ("+f["unit"].toString()+")";
        auto* label=new QLabel(title);label->setWordWrap(true);label->setMinimumWidth(100);label->setBuddy(widget);auto* rows=groups[groupKey];const int row=rows->rowCount();
        rows->addWidget(label,row,0,Qt::AlignVCenter);rows->addWidget(widget,row,1,Qt::AlignVCenter);
        if(!f["visible"].toBool(true)){label->hide();widget->hide();}
    }
    layout->addStretch();parameters_->setWidget(body);
    additional_->setWidget(extraBody);additionalLayout_=extraLayout;additionalColumns_=0;arrangeAdditional();
    pages_->setTabVisible(pages_->indexOf(additional_),hasAdditional);
    const auto helpText=schema["help"].toString().isEmpty()?result_["help"].toString():schema["help"].toString();
    help_->setPlainText(helpText.isEmpty()?(result_.isEmpty()?"方法说明会随计算结果更新。":"本次结果未提供独立帮助；计算说明请见结果报告。"):helpText);
    QTimer::singleShot(0,this,[this,generation,generalScroll,extraScroll]{
        if(generation!=parameterGeneration_)return;
        parameters_->verticalScrollBar()->setValue(generalScroll);additional_->verticalScrollBar()->setValue(extraScroll);
    });
    if(preview_)preview_->invalidate();
}
void PerformanceDialog::arrangeAdditional(){
    if(!additionalLayout_)return;
    const int columnWidth=std::max(340,fontMetrics().horizontalAdvance(QString(24,'M')));
    const int columns=additional_->viewport()->width()>=2*columnWidth?2:1;
    if(columns==additionalColumns_)return;
    additionalColumns_=columns;
    int weights[2]{};
    for(auto* group:additionalGroups_){
        for(auto* column:additionalColumnLayouts_)column->removeWidget(group);
        const int column=columns==1||weights[0]<=weights[1]?0:1;auto* layout=additionalColumnLayouts_[column];
        layout->insertWidget(layout->count()-1,group);weights[column]+=group->layout()->count();group->show();
    }
    additionalColumnLayouts_[1]->parentWidget()->setVisible(columns==2);
    additionalLayout_->setColumnStretch(0,1);additionalLayout_->setColumnStretch(1,columns==2?1:0);
}
void PerformanceDialog::resizeEvent(QResizeEvent* event){QDialog::resizeEvent(event);arrangeAdditional();}
void PerformanceDialog::changeEvent(QEvent* event){QDialog::changeEvent(event);if(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)arrangeAdditional();}
bool PerformanceDialog::eventFilter(QObject* object,QEvent* event){
    if(additional_&&object==additional_->viewport()&&event->type()==QEvent::Resize)arrangeAdditional();
    return QDialog::eventFilter(object,event);
}
void PerformanceDialog::refreshChoices(){
    if(choicesPending_)return;
    choicesPending_=true;
    QTimer::singleShot(0,this,[this]{choicesPending_=false;if(!session_.busy())session_.performanceDefaults(inputs());});
}
QJsonObject PerformanceDialog::inputs()const{
    QJsonObject values;for(auto it=fields_.begin();it!=fields_.end();++it){auto* w=it.value();if(auto* n=qobject_cast<QDoubleSpinBox*>(w))values[it.key()]=n->property("performance_exact_value").toDouble();else if(auto* n=qobject_cast<QCheckBox*>(w))values[it.key()]=n->isChecked();else if(auto* n=qobject_cast<QComboBox*>(w))values[it.key()]=n->currentIndex();}
    return {{"calculator",method_},{"values",values},{"solver_directory",solver_->text().trimmed()}};
}
void PerformanceDialog::calculate(){
    if(!hidden_)QSettings().setValue("calculation/solver-directory",solver_->text().trimmed());
    if(session_.calculatePerformance(inputs())){setProperty("performance_finished",false);setProperty("performance_error",QString{});status_->setText("正在计算，可取消。");refreshState();}
}
void PerformanceDialog::refreshState(){
    const bool busy=session_.busy();parameters_->setEnabled(!busy);additional_->setEnabled(!busy);solver_->setEnabled(!busy);run_->setEnabled(!busy);cancel_->setEnabled(busy);extract_->setEnabled(!busy);save_->setEnabled(!busy);
    save_->setEnabled(!busy&&schema_["persistable"].toBool(true));
    if(databaseButton_)databaseButton_->setEnabled(!busy&&!databaseViewer_);
    for(auto* b:exports_)b->setEnabled(!busy&&!result_.isEmpty());
    for(auto* b:findChildren<QPushButton*>())if(b->property("performance_action")=="save_parameters"||b->property("performance_action")=="load_parameters"||b->property("performance_action")=="water_temperature")b->setEnabled(!busy);
    if(closing_&&!busy)QTimer::singleShot(0,this,[this]{QDialog::reject();});
}
void PerformanceDialog::showFailure(const QString& error){status_->setText(error+(result_.isEmpty()?QString{}:"\n保留上一次完整结果。"));setProperty("performance_error",error);setProperty("performance_finished",true);refreshState();}
void PerformanceDialog::settingsSaved(){status_->setText("参数已写入当前船模，可撤销；保存文件后下次打开仍可使用。");refreshState();}
void PerformanceDialog::receive(const QJsonObject& result,bool defaults){
    if(!defaults){auto state=std::make_shared<CalculationChartState>();state->data=CalculationChartData::read(method_,result);state->visible.fill(true,state->data->curves.size());
        static_cast<CalculationPlot*>(plot_)->setState(state);if(method_!="maneuver"&&method_!="addmass")static_cast<CalculationPlot*>(overview_)->setState(state);}
    if(defaults){setParameters(result);status_->setText("参数已更新；请核对后计算。");}
    else{result_=result;setParameters(result["parameters"].toObject());setProperty("performance_finished",true);series_->clear();for(auto s:result[result.contains("tables")?"tables":"series"].toArray())series_->addItem(s.toObject()["name"].toString());renderSeries();text_->setPlainText(reportText());if(auto* summary=qobject_cast<QPlainTextEdit*>(overview_))summary->setPlainText(reportText());status_->setText(QString(method_=="addmass"?"计算完成，%1 组数据，耗时 %2 s。%3":"计算完成，%1 条曲线，耗时 %2 s。%3").arg(series_->count()).arg(result["elapsed_ms"].toDouble()/1000,0,'f',3).arg(result["warnings"].toArray().isEmpty()?QString{}:"请查看报告中的提示。"));}
    refreshState();
}
void PerformanceDialog::reject(){if(databaseViewer_){status_->setText("请先关闭外部数据库查看器，再关闭此窗口。");return;}if(session_.busy()){closing_=true;session_.cancelPerformance();status_->setText("取消计算后关闭…");return;}QDialog::reject();}
void PerformanceDialog::openDatabaseViewer(){
    if(databaseViewer_||session_.busy())return;
    const auto exe=QDir(solver_->text().trimmed()).absoluteFilePath("DBFview.EXE");
    if(solver_->text().trimmed().isEmpty()||!QFileInfo(exe).isFile()){showFailure("找不到 DBFview.EXE。原版数据库查看器是单独的外部程序，请选择包含它的目录。");return;}
    auto work=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/fs-dbfview-XXXXXX");
    if(!work->isValid()){showFailure("无法创建数据库查看器工作目录。");return;}
    auto* process=new QProcess(this);databaseViewer_=process;process->setProgram(exe);process->setWorkingDirectory(work->path());
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    // This is an explicitly requested interactive viewer, not a background solver.
    process->setStandardOutputFile(QProcess::nullDevice());process->setStandardErrorFile(QProcess::nullDevice());
    connect(process,&QProcess::errorOccurred,this,[this,process,work](QProcess::ProcessError error){if(error!=QProcess::FailedToStart)return;databaseViewer_=nullptr;process->deleteLater();showFailure("数据库查看器无法启动："+process->errorString());});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,work](int code,QProcess::ExitStatus state){databaseViewer_=nullptr;process->deleteLater();setProperty("database_viewer_exit",code);refreshState();if(state!=QProcess::NormalExit||code!=0)showFailure("数据库查看器异常退出，原计算结果已保留。");else{status_->setText("数据库查看器已关闭。");calculate();}});
    setProperty("database_viewer_exit",QVariant{});process->start();refreshState();
}
void PerformanceDialog::renderSeries(){
    if(result_.contains("tables")){auto all=result_["tables"].toArray();int i=series_->currentIndex();if(i<0||i>=all.size())return;static_cast<PerformanceTable*>(table_->model())->setTable(all[i].toObject());table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->resizeRowsToContents();return;}
    const auto all=result_["series"].toArray();const auto index=series_->currentIndex();if(index<0||index>=all.size())return;
    const auto s=all[index].toObject();static_cast<PerformanceTable*>(table_->model())->setSeries(s);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    static_cast<CalculationPlot*>(plot_)->setSelected(index);
    if(method_!="maneuver"&&method_!="addmass")static_cast<CalculationPlot*>(overview_)->setSelected(index);
}
QByteArray PerformanceDialog::csv()const{
    if(result_.contains("tables")){QStringList lines{"数据组,行,列,值"};for(auto value:result_["tables"].toArray()){auto t=value.toObject();auto rows=t["rows"].toArray(),cols=t["columns"].toArray(),values=t["values"].toArray();for(qsizetype i=0;i<values.size();++i){auto row=values[i].toArray();for(qsizetype j=0;j<row.size();++j)lines<<QStringList{quote(t["name"].toString()),quote(rows[i].toVariant().toString()),quote(cols[j].toVariant().toString()),row[j].isNull()?QString{}:full(row[j].toDouble())}.join(',');}}return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();}
    QStringList lines;lines<<"曲线,X 量与单位,Y 量与单位,X,Y";
    for(auto v:result_["series"].toArray()){auto s=v.toObject();for(auto p:s["points"].toArray()){auto xy=p.toArray();lines<<QStringList{quote(s["name"].toString()),quote(s["x_title"].toString()),quote(s["y_title"].toString()),full(xy[0].toDouble()),full(xy[1].toDouble())}.join(',');}}
    return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();
}
QString PerformanceDialog::reportText()const{
    QString text="FreeShip_26 — "+windowTitle()+"\n模型版本："+QString::number(result_["revision"].toDouble(),'f',0)+"\n\n";
    for(auto warning:result_["warnings"].toArray())text+="提示："+warning.toString()+"\n";
    text+="本次计算参数\n";
    for(auto v:result_["parameters"].toObject()["fields"].toArray()){
        const auto f=v.toObject();QString value;
        if(f["kind"]=="boolean")value=f["value"].toBool()?"是":"否";
        else if(f["kind"]=="choice"){const auto index=f["value"].toInt();const auto choices=f["choices"].toArray();value=index>=0&&index<choices.size()?choices[index].toString():"未选择";}
        else value=full(f["value"].toDouble());
        text+=f["label"].toString()+"："+value+" "+f["unit"].toString()+"\n";
    }
    text+="\n";
    text+=result_["text"].toString()+"\n"+result_["help"].toString();return text;
}
QString PerformanceDialog::exportReport(const QString& path,const QString& format)const{
    if(result_.isEmpty())return "尚无完整计算结果。";
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return "无法写入报告："+file.errorString();
    if(format=="pdf"){QPdfWriter writer(&file);writer.setPageSize(QPageSize(QPageSize::A4));writer.setPageOrientation(QPageLayout::Landscape);writer.setResolution(120);QTextDocument doc;QFont font("Microsoft YaHei UI",8);doc.setDefaultFont(font);doc.setPlainText(reportText());doc.print(&writer);}
    else{QByteArray bytes;if(format=="csv")bytes=csv();else if(format=="json")bytes=QJsonDocument(result_).toJson();else if(format=="txt")bytes=reportText().toUtf8();else return "未知报告格式。";if(file.write(bytes)!=bytes.size())return "报告写入未完成。";}
    return file.commit()?QString{}:"无法完成报告保存："+file.errorString();
}
QString PerformanceDialog::saveParameters(const QString& path)const{
    if(session_.busy())return "请等待当前操作结束。";
    auto data=inputs();data.remove("solver_directory");data["format"]="FreeShipPerformanceParameters";data["version"]=1;data["units"]=session_.model()->units;
    QSaveFile f(path);const auto bytes=QJsonDocument(data).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())return "无法保存参数文件。";return {};
}
QString PerformanceDialog::loadParameters(const QString& path){
    if(session_.busy())return "请等待当前操作结束。";
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return "无法读取参数文件。";
    QJsonParseError parse;auto data=QJsonDocument::fromJson(f.readAll(),&parse).object();
    if(parse.error!=QJsonParseError::NoError||data["format"]!="FreeShipPerformanceParameters"||data["version"]!=1||data["calculator"]!=method_||data["units"]!=session_.model()->units||!data["values"].isObject())return "参数文件的方法、版本或单位与当前计算不一致。";
    data["solver_directory"]=solver_->text().trimmed();return session_.performanceDefaults(data)?QString{}:"无法载入计算参数。";
}
