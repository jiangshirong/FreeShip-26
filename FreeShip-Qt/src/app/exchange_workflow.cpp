// GPL-2.0-or-later. Actual application callbacks in hidden Windows widgets.
#include "main_window.h"
#include "ui_language.h"
#include "exchange_dialog.h"
#include "exchange_save_probe.h"
#include <QApplication>
#include "grid_preview_panel.h"
#include "print_hull_panel.h"
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QSlider>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QToolButton>

void MainWindow::openExchange(bool importing,const QString& format){
    if(session_.busy()||(!importing&&!session_.ready()))return;
    ExchangeDialog dialog(session_,importing,format,this);exchangeDialog_=&dialog;
    if(exchangeWorkflow_){auto command=exchangeRequest_["commands"].toArray()[exchangeIndex_].toObject();if(command.contains("confirm_overwrite"))dialog.confirmOverwrite=[command](const QString& path){return QFileInfo(path).absoluteFilePath()==QFileInfo(command["path"].toString()).absoluteFilePath()&&command["confirm_overwrite"].toBool();};}
    if(!importing&&format=="part"&&selectionKind_==HullEntity::Face)dialog.faceIds=selection_.values();
    dialog.beforeImport=[this](std::function<void()> next){if(exchangeWorkflow_)next();else withSavedChanges(std::move(next));};
    dialog.exec();exchangeDialog_=nullptr;
    if(exchangeWorkflow_){
        const auto command=exchangeRequest_["commands"].toArray()[exchangeIndex_].toObject();const auto key=command["name"].toString();bool expectFailure=command["expect_failure"].toBool();
        const bool declined=(command.contains("confirm_zero_faces")&&!command["confirm_zero_faces"].toBool())||
                            (command.contains("confirm_obj_fit")&&!command["confirm_obj_fit"].toBool());
        const bool keptFile=command.contains("confirm_overwrite")&&!command["confirm_overwrite"].toBool();
        if(command["cancel"].toBool()||command["cancel_save"].toBool()||declined||keptFile)workflowCase(key+".cancelled",!dialog.succeeded()&&dialog.failure().isEmpty(),dialog.failure());
        else workflowCase(key+".completed",expectFailure?!dialog.failure().isEmpty():dialog.succeeded(),dialog.failure());
        if(dialog.printHull())workflowArtifacts_[key+"_shell_final"]=dialog.printHull()->evidence();
        if(!dialog.objFitEvidence().isEmpty())workflowArtifacts_[key+"_obj_fit"]=dialog.objFitEvidence();
        if(session_.ready())captureWorkflow(key);
        workflowArtifacts_[key+"_error"]=dialog.failure();
        if(importing&&!expectFailure&&!declined&&format!="part"&&format!="waves")workflowCase(key+".new-unsaved-document",session_.modified()&&!session_.canUndo()&&session_.sourcePath().isEmpty());
        ++exchangeIndex_;QTimer::singleShot(0,this,[this]{exchangeStep("advance");});
    }
}
void MainWindow::verifyExchange(const QString& request,const QString& output){
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    workflow_=true;exchangeWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read exchange request");return;}
    exchangeRequest_=QJsonDocument::fromJson(f.readAll()).object();QDir().mkpath(workflowOutput_+"/views");
    if(exchangeRequest_["language"].isString()){
        const auto error=setUiLanguage(exchangeRequest_["language"].toString());
        if(!error.isEmpty()){finishWorkflow(error);return;}
    }
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
    if(action=="save"){
        if(!session_.save(c["path"].toString()))finishWorkflow(key+": save request did not start");
        return;
    }
    if(action=="reopen"){
        if(!session_.load(c["path"].toString()))finishWorkflow(key+": reopen request did not start");
        return;
    }
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
        auto* d=exchangeDialog_;
        if(*submitted && c["accept_stem_repair"].toBool() && d->printHull() && !d->failure().isEmpty()){
            for(auto* box:d->printHull()->findChildren<QCheckBox*>()){
                if(box->property("exchange_field")=="close_stem_seam" && box->isVisible() && !box->isChecked()){
                    workflowArtifacts_[key+"_before_stitch"]=d->printHull()->evidence();
                    box->click();
                    *submitted=false;
                    break;
                }
            }
        }
        if(*submitted){timer->stop();timer->deleteLater();exchangeDialog_->reject();return;}
        const auto options=c["options"].toObject();
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
        bool hasPath=false;
        for(auto*w:d->findChildren<QLineEdit*>())if(w->property("exchange_field")=="path"){hasPath=true;w->setText(c["path"].toString());}
        workflowCase(key+".path-field-only-on-import",hasPath==(c["method"]=="import"));
        for(auto*w:d->findChildren<QLineEdit*>()){auto key=w->property("exchange_number").toString();auto refs=options["references"].toObject();if(refs.contains(key))w->setText(refs[key].isString()?refs[key].toString():QString::number(refs[key].toDouble(),'g',17));}
        for(auto*w:d->findChildren<QCheckBox*>()){auto field=w->property("exchange_field").toString();if(options.contains(field))w->setChecked(options[field].toBool());}
        for(auto*w:d->findChildren<QComboBox*>()){auto field=w->property("exchange_field").toString();if(options.contains(field))w->setCurrentIndex(w->findData(options[field].toString()));}
        if(options["obj_import_mode"]=="fit"){
            QToolButton *relative=nullptr,*manual=nullptr;
            QLineEdit* value=nullptr;
            for(auto* button:d->findChildren<QToolButton*>()){
                const auto mode=button->property("obj_tolerance_mode").toString();
                if(mode=="relative")relative=button;
                if(mode=="manual")manual=button;
            }
            for(auto* edit:d->findChildren<QLineEdit*>())
                if(edit->property("obj_tolerance_value").toBool())value=edit;
            if(relative&&manual&&value){
                const auto tolerance=options["references"].toObject()["obj_tolerance_mm"];
                if(!tolerance.isUndefined())
                    value->setText(tolerance.isString()?tolerance.toString():QString::number(tolerance.toDouble(),'g',17));
                if(c["check_obj_tolerance_toggle"].toBool()){
                    workflowCase(key+".relative-default",relative->isChecked()&&!manual->isChecked()&&
                                 !value->isVisible()&&!value->isEnabled());
                    const int slotWidth=manual->width();
                    manual->click();
                    workflowCase(key+".manual-enables-input",manual->isChecked()&&!relative->isChecked()&&
                                 value->isVisible()&&value->isEnabled()&&!value->isReadOnly());
                    workflowCase(key+".manual-fills-slot",value->width()>=96&&value->width()==slotWidth);
                    relative->click();
                    workflowCase(key+".relative-disables-input",relative->isChecked()&&!manual->isChecked()&&
                                 !value->isVisible()&&!value->isEnabled());
                }
                const auto fallback=options["references"].toObject().contains("obj_tolerance_mm")?"manual":"relative";
                if(options["obj_tolerance_mode"].toString(fallback)=="manual")manual->click();
            }else if(c["check_obj_tolerance_toggle"].toBool()){
                workflowCases_.append(QJsonObject{{"name",key+".tolerance-toggle"},{"status","not_run"}});
            }
        }
        if(options["control_net"].toBool()&&!options.contains("obj_import_mode")){
            for(auto* w:d->findChildren<QComboBox*>())
                if(w->property("exchange_field")=="obj_import_mode")w->setCurrentIndex(w->findData("control"));
        }
        for(auto*w:d->findChildren<QComboBox*>()){auto key=w->property("exchange_parameter").toString();auto refs=options["references"].toObject();if(refs.contains(key))w->setCurrentIndex(w->findData(refs[key].toVariant()));}
        for(auto*w:d->findChildren<QSpinBox*>()){auto field=w->property("exchange_count").toString();if(options.contains(field))w->setValue(options[field].toInt());}
        if(options["print_shell"].toBool() && d->printHull()){
            for(auto*w:d->findChildren<QDoubleSpinBox*>()){auto field=w->property("print_value").toString();if(options.contains(field))w->setValue(options[field].toDouble());}
            for(auto*w:d->findChildren<QListWidget*>())if(w->property("print_layers").toBool()&&options.contains("layers")){
                const auto ids=options["layers"].toArray();for(int i=0;i<w->count();++i){auto* item=w->item(i);
                    item->setCheckState(ids.contains(item->data(Qt::UserRole).toInt())?Qt::Checked:Qt::Unchecked);}
            }
            workflowCase(key+".no-auto-generation",!d->printHull()->pending() && d->printHull()->evidence()["builds"].toInt()==0);
            workflowCase(key+".no-section-preview-controls",d->printHull()->findChildren<QSlider*>().isEmpty()
                         && d->printHull()->findChildren<QPushButton*>().isEmpty());
            *previewStage=3;
        }
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
        if(c.contains("confirm_zero_faces")||c.contains("confirm_obj_fit")){
            // Separate timer: the submission timer is still dispatching while
            // the production confirmation dialog runs its nested event loop.
            auto* confirmation=new QTimer(d);confirmation->setInterval(10);
            connect(confirmation,&QTimer::timeout,d,[this,d,c,key,confirmation]{
                for(auto* message:d->findChildren<QMessageBox*>())if(message->isVisible()){
                    confirmation->stop();confirmation->deleteLater();
                    const bool fit=c.contains("confirm_obj_fit");
                    workflowCase(key+(fit?".fit-review-default-cancel":".zero-face-warning-default-cancel"),
                                 message->defaultButton()==message->button(QMessageBox::No));
                    workflowArtifacts_[key+(fit?"_fit_confirmation":"_zero_face_warning")]=message->text();
                    message->button(c[fit?"confirm_obj_fit":"confirm_zero_faces"].toBool()?QMessageBox::Yes:QMessageBox::No)->click();return;
                }
            });confirmation->start();
        }
        if(c["method"]=="export")answerExchangeSaveDialog(d,c["path"].toString(),c["cancel_save"].toBool(),
            [this,key](const QString& suggested){workflowArtifacts_[key+"_suggested_file"]=suggested;});
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
