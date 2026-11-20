// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_hull_panel.h"
#include "core/model_exchange.h"
#include "model_session.h"
#include "ui_language.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QPromise>
#include <QSignalBlocker>
#include <QThreadPool>
#include <QVBoxLayout>

namespace {
struct Prepared {
    std::shared_ptr<fs::printing::Result> result;
    std::shared_ptr<QTemporaryDir> stage;
    fs::printing::Problem problem;
    qint64 buildMs=0,stlMs=0;
};
}
PrintHullPanel::PrintHullPanel(ModelSession& session,QWidget* parent):QWidget(parent),session_(session) {
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto* fields=new QFormLayout;
    auto number=[&](const char* caption,const char* key,double low,double high,double value) {
        auto* box=new QDoubleSpinBox;box->setRange(low,high);box->setDecimals(2);box->setValue(value);
        box->setKeyboardTracking(false);box->setProperty("print_value",key);
        fields->addRow(uiText(caption),box);connect(box,&QDoubleSpinBox::valueChanged,this,[this]{invalidate();});
        return box;
    };
    length_=number("成品船长 (mm)","length_mm",1,10000,250);
    thickness_=number("成品壁厚 (mm)","thickness_mm",.05,1000,1.2);thickness_->setSingleStep(.1);
    layers_=new QListWidget;layers_->setProperty("print_layers",true);layers_->setMaximumHeight(82);
    fields->addRow(uiText("船壳图层"),layers_);
    seam_=new QCheckBox(uiText("接合艏艉中心线微缝"));seam_->setProperty("exchange_field","close_stem_seam");seam_->hide();
    fields->addRow(seam_);layout->addLayout(fields);
    message_=new QLabel;message_->setWordWrap(true);layout->addWidget(message_);
    connect(layers_,&QListWidget::itemChanged,this,[this]{invalidate();});
    connect(seam_,&QCheckBox::toggled,this,[this]{invalidate();});
    hide();
}
PrintHullPanel::~PrintHullPanel() {if(job_) job_->Cancel();}
void PrintHullPanel::setActive(bool on) {
    active_=on;setVisible(on);
    if(on) syncSource();
    else {invalidate();source_.reset();}
}
void PrintHullPanel::syncSource() {
    if(!active_) return;
    auto current=session_.model();
    if(current==source_) return;
    source_=current;
    QSignalBlocker block(layers_);layers_->clear();
    if(current) {
        QSet<fs::EntityId> used;
        for(const auto& tri:current->triangles) used.insert(tri.layer);
        for(const auto& layer:current->layers) if(used.contains(layer.id)) {
            auto* item=new QListWidgetItem(layer.name,layers_);item->setData(Qt::UserRole,QVariant::fromValue(layer.id));
            item->setCheckState(layer.visible && layer.useInHydrostatics?Qt::Checked:Qt::Unchecked);
        }
    }
    {QSignalBlocker hold(seam_);seam_->setChecked(false);seam_->hide();}
    invalidate();
}
void PrintHullPanel::invalidate() {
    if(job_) job_->Cancel();
    job_.reset();onReady_={};result_.reset();stage_.reset();error_.clear();
    pending_=false;message_->clear();
    if(changed) changed();
}
void PrintHullPanel::prepare(std::function<void()> onReady) {
    if(!active_||!source_||pending_) return;
    if(ready()){if(onReady)onReady();return;}
    onReady_=std::move(onReady);
    pending_=true;++builds_;error_.clear();
    message_->setText(uiText("正在生成船壳…"));if(changed)changed();
    fs::printing::Options options;options.lengthMm=length_->value();options.thicknessMm=thickness_->value();
    options.closeStemSeam=seam_->isChecked();
    for(int i=0;i<layers_->count();++i) {
        auto* item=layers_->item(i);if(item->checkState()==Qt::Checked) options.layers.insert(item->data(Qt::UserRole).toULongLong());
    }
    auto job=std::make_shared<manifold::ExecutionContext>();job_=job;
    auto promise=std::make_shared<QPromise<Prepared>>();auto* watcher=new QFutureWatcher<Prepared>(this);
    connect(watcher,&QFutureWatcher<Prepared>::finished,this,[this,watcher,job]{
        auto prepared=watcher->result();watcher->deleteLater();
        if(job_!=job||job->Cancelled()) return;
        pending_=false;auto complete=std::move(onReady_);
        buildMs_=prepared.buildMs;stlMs_=prepared.stlMs;
        if(source_!=session_.model()) {syncSource();return;}
        result_=std::move(prepared.result);stage_=std::move(prepared.stage);
        auto p=prepared.problem;
        if(!p.reason.isEmpty()) {
            result_.reset();stage_.reset();
            error_=uiText(p.reason);if(!p.location.isEmpty()) error_+=" "+p.location;
            message_->setText(error_);
        } else if(result_) {
            auto text=QString("%1 × %2 × %3 mm").arg(result_->lengthMm,0,'f',2)
                .arg(result_->beamMm,0,'f',2).arg(result_->heightMm,0,'f',2);
            if(result_->seamMm>0) text+="  ·  "+QString(uiText("已接合 %1 mm 中心线微缝")).arg(result_->seamMm,0,'f',3);
            message_->setText(text);
        }
        seam_->setVisible(p.stemSeam||seam_->isChecked());if(changed) changed();
        if(ready()&&complete)complete();
    });
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise,source=source_,options,job]{
        promise->start();Prepared prepared;QElapsedTimer timer;timer.start();
        try {
            prepared.result=std::make_shared<fs::printing::Result>(fs::printing::build(*source,options,*job));
            prepared.buildMs=timer.restart();
            prepared.stage=std::make_shared<QTemporaryDir>();
            if(!prepared.stage->isValid()) throw fs::printing::Problem{"无法创建导出临时目录。",{}};
            fs::exchange::Options stl;stl.units="mm";stl.fullHull=false;
            fs::exchange::write(prepared.stage->filePath("shell.stl"),"stl",prepared.result->mesh,stl);
            prepared.stlMs=timer.elapsed();
        }catch(const fs::printing::Problem& p){prepared.problem=p;}
        catch(const std::exception& e){prepared.problem.reason=QString::fromUtf8(e.what());}
        promise->addResult(std::move(prepared));promise->finish();
    });
}
bool PrintHullPanel::ready() const {
    return active_ && result_ && stage_ && source_==session_.model() && session_.ready();
}
void PrintHullPanel::save(const QString& path) const {
    if(!ready()) throw std::runtime_error(uiText("模型版本已改变，请重新导出。"));
    ModelSession::atomicCopy(stage_->filePath("shell.stl"),path);
}
QJsonObject PrintHullPanel::evidence() const {
    return {{"builds",builds_},{"build_ms",buildMs_},{"stl_ms",stlMs_},{"ready",ready()},
            {"seam_mm",result_?result_->seamMm:0.},{"retessellated_ends",result_?result_->retessellatedEnds:0},
            {"triangles",result_?int(result_->mesh.triangles.size()):0},
            {"error",error_}};
}
