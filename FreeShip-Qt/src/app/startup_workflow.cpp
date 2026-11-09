// Retained hidden-process observation of the production startup/open/save paths.
#include "main_window.h"
#include "project_dialog.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
// Only the explicitly requested hidden startup probe can kill its own service.
// This is an actual QProcess exit, not a simulated successful recovery reply.
struct BackendExitProbe {static void terminate(ModelSession& s){s.process_.kill();}};
void MainWindow::verifyStartup(const QString& request,const QString& output){
    startupProbe_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QDir().mkpath(workflowOutput_);QFile file(request);
    if(!file.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read startup request");return;}
    const auto config=QJsonDocument::fromJson(file.readAll()).object();
    fileHistory_=std::make_unique<FileHistory>(config["history_file"].toString());
    if(config.contains("recovery_root"))session_.setRecoveryRoot(config["recovery_root"].toString());
    if(config.contains("lines_cache_root"))session_.setLinesCacheRoot(config["lines_cache_root"].toString());
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{if(startupClosing_&&!workflowFinished_)startupStep("close");});
    startupCommands_=config["commands"].toArray();startupCommand_=0;refreshRecentFiles();
    QTimer::singleShot(120000,this,[this]{if(!workflowFinished_)finishWorkflow("Startup workflow timed out");});
    startupStep("begin");
}
void MainWindow::startupStep(const QString& operation,const QString& error){
    if(workflowFinished_)return;
    if(operation!="begin"){
        QJsonArray recent;for(const auto& path:fileHistory_->read().paths)recent.append(path);
        QJsonArray actions;for(auto* action:recentMenu_->actions())actions.append(QJsonObject{{"text",action->text()},{"path",action->data().toString()},{"enabled",action->isEnabled()},{"operation",action->property("file_operation").toString()}});
        QJsonObject item{{"operation",operation},{"error",error},{"source",session_.sourcePath()},{"ready",session_.ready()},{"modified",session_.modified()},
            {"last",fileHistory_->read().last},{"recent",recent},{"actions",actions},{"status_text",statusBar()->currentMessage()}};
        item["recovery_file"]=session_.recoveryFile();item["recovery_error"]=session_.recoveryError();item["recovery_origin"]=session_.recoveryOrigin();
        item["backup_file"]=session_.lastBackupPath();
        item["can_resume_current"]=session_.canResumeCurrent();
        item["can_undo"]=session_.canUndo();item["can_redo"]=session_.canRedo();
        item["save_preview_ms"]=session_.lastSavePreviewMs();
        if(!session_.lastBackupPath().isEmpty()&&QFileInfo(session_.lastBackupPath()).isFile()){
            const auto saved=workflowOutput_+"/backup-"+QString::number(startupCommand_)+".fbm";
            ModelSession::atomicCopy(session_.lastBackupPath(),saved);item["backup_snapshot"]=saved;
        }
        if(session_.model()){
            QFile native(QFileInfo(session_.modelPath()).dir().filePath("native-report.json"));
            if(native.open(QIODevice::ReadOnly))item["native_report"]=QJsonDocument::fromJson(native.readAll()).object();
            const auto target=workflowOutput_+"/model-"+QString::number(startupCommand_)+".json";
            ModelSession::atomicCopy(session_.modelPath(),target);item["model"]=target;
            const auto published=QFileInfo(session_.modelPath()).dir();
            for(const auto& artifact:{qMakePair(QString("scene"),session_.scenePath()),qMakePair(QString("dxf"),session_.dxfPath()),qMakePair(QString("surface"),published.filePath("surface.json")),qMakePair(QString("sections"),published.filePath("sections.json"))}){
                const auto saved=workflowOutput_+'/'+artifact.first+'-'+QString::number(startupCommand_)+'.'+artifact.first;
                ModelSession::atomicCopy(artifact.second,saved);item[artifact.first]=saved;
            }
        }
        startupObservations_.append(item);
    }
    if(startupCommand_>=startupCommands_.size()){
        QFile file(workflowOutput_+"/observations.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(startupObservations_).toJson());
        workflowArtifacts_["observations"]=file.fileName();workflowCase("startup_commands_observed",true);
        workflowCases_.append(QJsonObject{{"name","desktop_acceptance"},{"status","not_run"}});finishWorkflow();return;
    }
    const auto command=startupCommands_[startupCommand_++].toObject();
    QTimer::singleShot(0,this,[this,command]{
        const auto observed=startupObservations_.size();
        const auto kind=command["kind"].toString(),path=command["path"].toString();
        if(command.contains("recovery_decision"))QTimer::singleShot(0,this,[this,command]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog){finishWorkflow("Recovery dialog did not open");return;}
            const auto id=command["recovery_id"].toString();
            if(!id.isEmpty())for(auto* list:dialog->findChildren<QListWidget*>())for(int i=0;i<list->count();++i)
                if(list->item(i)->data(Qt::UserRole).toString()==id)list->setCurrentRow(i);
            QDir().mkpath(workflowOutput_+"/views");dialog->grab().save(workflowOutput_+"/views/recovery-"+QString::number(startupCommand_)+".png");
            QList<QPushButton*> found;for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("recovery_action")==command["recovery_decision"].toString())found.append(b);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recovery_action_unavailable"},{"status","not_run"}});dialog->reject();return;
            }
            if(command["recovery_decision"]=="delete")QTimer::singleShot(0,this,[dialog,command]{
                if(auto* question=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
                    auto* button=question->button(command["confirm_delete"].toBool()?QMessageBox::Yes:QMessageBox::No);
                    if(button)button->click();
                }
                QTimer::singleShot(0,dialog,&QDialog::reject);
            });
            found[0]->click();
        });
        if(kind=="startup")startSession(path);
        else if(kind=="open")openModel(path);
        else if(kind=="save")session_.save(path);
        else if(kind=="new_hull")session_.newHull(command["fields"].toObject());
        else if(kind=="update_project")session_.updateProject(command["changes"].toObject());
        else if(kind=="project_action"){
            QList<QAction*> actions;for(auto* action:findChildren<QAction*>())if(action->property("document_operation")=="project_settings")actions.append(action);
            if(actions.size()!=1||!actions[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","project_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }
            QTimer::singleShot(0,this,[this,command]{
                auto* dialog=dynamic_cast<ProjectDialog*>(QApplication::activeModalWidget());
                if(!dialog){finishWorkflow("Project dialog did not open");return;}
                const auto changes=command["changes"].toObject();
                for(auto it=changes.begin();it!=changes.end();++it){
                    QList<QWidget*> found;for(auto* field:dialog->findChildren<QWidget*>())if(field->property("modelField")==it.key())found.append(field);
                    if(found.size()!=1){workflowCases_.append(QJsonObject{{"name","project_field_unavailable"},{"status","not_run"}});dialog->reject();return;}
                    if(auto* box=qobject_cast<QCheckBox*>(found[0]);box&&it.value().isBool())box->setChecked(it.value().toBool());
                    else if(auto* box=qobject_cast<QLineEdit*>(found[0]);box&&it.value().isString())box->setText(it.value().toString());
                    else if(auto* box=qobject_cast<QComboBox*>(found[0]);box&&it.value().isDouble()&&it.value().toDouble()==it.value().toInt()&&it.value().toInt()>=0&&it.value().toInt()<box->count())box->setCurrentIndex(it.value().toInt());
                    else if(auto* box=qobject_cast<QSpinBox*>(found[0]);box&&it.value().isDouble()&&it.value().toDouble()==it.value().toInt()&&it.value().toInt()>=box->minimum()&&it.value().toInt()<=box->maximum())box->setValue(it.value().toInt());
                    else {dialog->reject();finishWorkflow("Project field is outside the safe probe adapter");return;}
                }
                if(command["cancel"].toBool())dialog->reject();else dialog->accept();
            });
            actions[0]->trigger();
        }
        else if(kind=="save_action"){
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="save")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","save_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="move"){
            if(!session_.model()){finishWorkflow("No model to edit");return;}
            const fs::ControlPoint* chosen=nullptr;
            for(const auto& point:session_.model()->points)if(!point.locked&&point.batchMovable){chosen=&point;break;}
            if(!chosen){finishWorkflow("No movable point");return;}
            auto position=chosen->position;position.x+=command["dx"].toDouble(.125);session_.movePoint(chosen->id,position);
        }
        else if(kind=="undo")session_.undo();
        else if(kind=="redo")session_.redo();
        else if(kind=="discard_recovery")session_.discardRecovery();
        else if(kind=="backend_exit"){
            if(!session_.ready()||session_.busy()){finishWorkflow("Backend exit probe requires its own idle live document");return;}
            BackendExitProbe::terminate(session_);return; // Advance on the actual process error.
        }
        else if(kind=="resume_current"){
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="resume_current")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","resume_current_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="close"){
            startupClosing_=true;
            QTimer::singleShot(0,this,[this,command]{
                auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if(!dialog){finishWorkflow("Unsaved-change confirmation did not open");return;}
                const auto choice=command["discard"].toBool()?QMessageBox::Discard:QMessageBox::Cancel;
                if(auto* button=dialog->button(choice))button->click();
            });
            close();
        }
        else if(kind=="recover"){
            QList<QAction*> found;for(auto* action:findChildren<QAction*>())if(action->property("file_operation")=="recover")found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recover_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }
        else if(kind=="recent"){
            QList<QAction*> found;for(auto* action:recentMenu_->actions())if(action->property("file_operation")=="recent"&&action->data().toString()==path)found.append(action);
            if(found.size()!=1||!found[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","recent_action_unavailable"},{"status","not_run"}});finishWorkflow();return;
            }found[0]->trigger();
        }else{finishWorkflow("Unknown startup observation command");return;}
        // A first launch, or an absent file, intentionally starts no backend.
        if(startupObservations_.size()==observed&&!session_.busy()&&!workflowFinished_)startupStep("idle");
    });
}
