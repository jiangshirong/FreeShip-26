// GPL-2.0-or-later. Actual application callbacks in hidden Windows widgets.
#include "main_window.h"
#include "exchange_dialog.h"
#include "grid_preview_panel.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>

void MainWindow::openExchange(bool importing,const QString& format){
    if(session_.busy()||(!importing&&!session_.ready()))return;
    ExchangeDialog dialog(session_,importing,format,this);exchangeDialog_=&dialog;
    if(exchangeWorkflow_){auto command=exchangeRequest_["commands"].toArray()[exchangeIndex_].toObject();if(command.contains("confirm_overwrite"))dialog.confirmOverwrite=[command](const QString& path){return QFileInfo(path).absoluteFilePath()==QFileInfo(command["path"].toString()).absoluteFilePath()&&command["confirm_overwrite"].toBool();};}
    if(!importing&&format=="part"&&selectionKind_==HullEntity::Face)dialog.faceIds=selection_.values();
    dialog.beforeImport=[this](std::function<void()> next){if(exchangeWorkflow_)next();else withSavedChanges(std::move(next));};
    dialog.exec();exchangeDialog_=nullptr;
    if(exchangeWorkflow_){
        const auto command=exchangeRequest_["commands"].toArray()[exchangeIndex_].toObject();const auto key=command["name"].toString();bool expectFailure=command["expect_failure"].toBool();
        if(command["cancel"].toBool())workflowCase(key+".cancelled",!dialog.succeeded()&&dialog.failure().isEmpty(),dialog.failure());
        else workflowCase(key+".completed",expectFailure?!dialog.failure().isEmpty():dialog.succeeded(),dialog.failure());
        if(session_.ready())captureWorkflow(key);
        workflowArtifacts_[key+"_error"]=dialog.failure();
        if(importing&&!expectFailure&&format!="part"&&format!="waves")workflowCase(key+".new-unsaved-document",session_.modified()&&!session_.canUndo()&&session_.sourcePath().isEmpty());
        ++exchangeIndex_;QTimer::singleShot(0,this,[this]{exchangeStep("advance");});
    }
}
void MainWindow::verifyExchange(const QString& request,const QString& output){
    workflow_=true;exchangeWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read exchange request");return;}
    exchangeRequest_=QJsonDocument::fromJson(f.readAll()).object();QDir().mkpath(workflowOutput_+"/views");
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();
    QTimer::singleShot(300000,this,[this]{if(!workflowFinished_)finishWorkflow("Exchange workflow timed out");});
    if(exchangeRequest_["model"].toString().isEmpty())exchangeStep("start");else session_.load(exchangeRequest_["model"].toString());
}
void MainWindow::exchangeStep(const QString& method){
    if(workflowFinished_||method=="import_model"||method=="append_model"||method=="cad_iges")return;
    if(method=="load"&&exchangeIndex_==0&&!workflowArtifacts_.contains("original_model"))captureWorkflow("original");
    else if(method=="save"||method=="load"||method=="undo"||method=="redo"){
        const auto c=exchangeRequest_["commands"].toArray()[exchangeIndex_].toObject();captureWorkflow(c["name"].toString());++exchangeIndex_;
    }
    auto commands=exchangeRequest_["commands"].toArray();if(exchangeIndex_>=commands.size()){finishWorkflow();return;}
    auto c=commands[exchangeIndex_].toObject();auto action=c["method"].toString();auto key=c["name"].toString();
    if(action=="save"){session_.save(c["path"].toString());return;}
    if(action=="reopen"){session_.load(c["path"].toString());return;}
    if(action=="undo"){session_.undo();return;}if(action=="redo"){session_.redo();return;}
    const auto operation=action+'_'+c["format"].toString();QList<QAction*> entries;for(auto*a:findChildren<QAction*>())if(a->property("operation")==operation)entries.append(a);
    if(entries.size()!=1||!entries[0]->isEnabled()){workflowCases_.append(QJsonObject{{"name",key+".entry"},{"status","not_run"}});++exchangeIndex_;QTimer::singleShot(0,this,[this]{exchangeStep("advance");});return;}
    auto* timer=new QTimer(this);timer->setInterval(10);auto submitted=std::make_shared<bool>(false);
    auto previewStage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,c,key,timer,submitted,previewStage]{
        if(!exchangeDialog_)return;
        if(*submitted&&c["cancel"].toBool()&&exchangeDialog_->busy()){
            workflowCase(key+".responsive-during-export",true);timer->stop();timer->deleteLater();exchangeDialog_->reject();return;
        }
        if(exchangeDialog_->busy())return;
        if(*submitted){timer->stop();timer->deleteLater();exchangeDialog_->reject();return;}
        auto* d=exchangeDialog_;const auto options=c["options"].toObject();
        if(*previewStage==1||*previewStage==2){auto* preview=d->gridPreview();if(preview->pending())return;
            if(*previewStage==1&&c["preview_changes"].isObject()){
                workflowArtifacts_[key+"_preview_before"]=preview->evidence();workflowCase(key+".first-preview-current",preview->current());
                auto changes=c["preview_changes"].toObject();for(auto* w:d->findChildren<QLineEdit*>()){auto field=w->property("exchange_number").toString();if(changes.contains(field))w->setText(QString::number(changes[field].toDouble(),'g',17));}
                workflowCase(key+".changed-preview-stale",!preview->current());preview->requestNow();*previewStage=2;return;
            }
            workflowArtifacts_[key+"_preview"]=preview->evidence();workflowCase(key+".preview-current",(c["preview_cancel"].toBool()||c["expect_failure"].toBool())?!preview->current():preview->current(),preview->evidence()["error"].toString());
            d->grab().save(workflowOutput_+"/views/"+key+"-preview.png");*previewStage=3;
        }
        if(*previewStage==0){
        connect(d,&QDialog::finished,timer,[timer]{timer->stop();timer->deleteLater();});
        for(auto*w:d->findChildren<QLineEdit*>())if(w->property("exchange_field")=="path")w->setText(c["path"].toString());
        for(auto*w:d->findChildren<QLineEdit*>()){auto key=w->property("exchange_number").toString();auto refs=options["references"].toObject();if(refs.contains(key))w->setText(refs[key].isString()?refs[key].toString():QString::number(refs[key].toDouble(),'g',17));}
        for(auto*w:d->findChildren<QCheckBox*>()){auto field=w->property("exchange_field").toString();if(options.contains(field))w->setChecked(options[field].toBool());}
        for(auto*w:d->findChildren<QComboBox*>()){auto field=w->property("exchange_field").toString();if(options.contains(field))w->setCurrentIndex(w->findData(options[field].toString()));}
        for(auto*w:d->findChildren<QComboBox*>()){auto key=w->property("exchange_parameter").toString();auto refs=options["references"].toObject();if(refs.contains(key))w->setCurrentIndex(w->findData(refs[key].toVariant()));}
        for(auto*w:d->findChildren<QSpinBox*>()){auto field=w->property("exchange_count").toString();if(options.contains(field))w->setValue(options[field].toInt());}
        }
        if(c["preview"].toBool()&&*previewStage==0){auto* preview=d->gridPreview();if(!preview){workflowCases_.append(QJsonObject{{"name",key+".preview-entry"},{"status","not_run"}});timer->stop();timer->deleteLater();d->reject();return;}
            for(auto* tabs:d->findChildren<QTabWidget*>())if(tabs->indexOf(preview)>=0)tabs->setCurrentWidget(preview);
            preview->requestNow();if(c["preview_cancel"].toBool())preview->cancel();*previewStage=1;return;
        }
        if(c["method"]=="import"&&c["format"]=="fef")for(auto*w:d->findChildren<QComboBox*>())if(w->property("exchange_field")=="units")workflowCase(key+".file-owned-units",!w->isEnabled()&&!w->currentData().isValid());
        QList<QPushButton*> buttons;for(auto*b:d->findChildren<QPushButton*>())if(b->property("exchange_action")==c["method"].toString())buttons.append(b);
        if(buttons.size()!=1){workflowCases_.append(QJsonObject{{"name",key+".submit-entry"},{"status","not_run"}});timer->stop();timer->deleteLater();d->reject();return;}
        *submitted=true;d->grab().save(workflowOutput_+"/views/"+key+".png");
        for(auto* tabs:d->findChildren<QTabWidget*>())if(tabs->property("exchange_tabs").toBool()){int original=tabs->currentIndex();for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);d->grab().save(workflowOutput_+"/views/"+key+"-tab-"+QString::number(i)+".png");}tabs->setCurrentIndex(original);}
        const QDir workspace(QFileInfo(QFileInfo(session_.modelPath()).absolutePath()).absolutePath());
        const auto previous=workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot);
        buttons[0]->click();
        if(c["late_cancel"].toBool()) {
            // Diagnostic only: delay UI dispatch until the production worker
            // commits a result, then cancel before receiveCad can write it.
            QElapsedTimer deadline;deadline.start();bool produced=false;
            while(!produced && deadline.elapsed()<3000) {
                for(const auto &name:workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot))
                    if(!previous.contains(name) && QFileInfo::exists(workspace.filePath(name+"/cad.json")))produced=true;
                if(!produced)QThread::msleep(1);
            }
            workflowCase(key+".result-pending-before-cancel",produced && session_.busy());d->reject();
        }
    });timer->start();entries[0]->trigger();
}
