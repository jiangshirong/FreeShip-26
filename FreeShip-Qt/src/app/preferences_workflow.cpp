// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden observation adapter for the production preferences action and dialog.
#include "main_window.h"
#include "preferences_dialog.h"
#include "studio_ui.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <stdexcept>
namespace {
QByteArray bytes(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
}
void MainWindow::verifyPreferences(const QString& request,const QString& output){
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    workflow_=true;preferencesWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();QDir().mkpath(workflowOutput_);
    QJsonParseError error;const auto config=QJsonDocument::fromJson(bytes(request),&error);
    if(error.error!=QJsonParseError::NoError||!config.isObject()){finishWorkflow("Invalid preferences probe request");return;}
    preferencesRequest_=config.object();loadPreferences(preferencesRequest_.value("settings_file").toString());
    for(auto* view:views_){view->resize(640,400);view->setAnimationsEnabled(false);}
    QTimer::singleShot(120000,this,[this]{if(!workflowFinished_){if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->reject();}finishWorkflow("Preferences probe timed out");}});
    preferencesStep("begin");
}
void MainWindow::preferencesStep(const QString& operation){
    if(workflowFinished_)return;
    try{
        int next=preferencesRequest_.value("next").toInt();auto observations=preferencesRequest_.value("observations").toArray();
        if(operation!="begin"){
            auto item=preferencesRequest_.value("dialog_observation").toObject();preferencesRequest_.remove("dialog_observation");
            item["operation"]=operation;item["active"]=preferences_;item["persisted"]=QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object();
            item["appearance"]=preferenceAppearance(preferences_);
            item["modified"]=session_.modified();item["undo"]=session_.canUndo();item["redo"]=session_.canRedo();item["toolbar_height"]=studioBar_->height();
            item["export_start"]=preferenceFileLocation("export","example.dxf");item["open_start"]=preferenceFileLocation("open");
            if(session_.model()){
                const auto model=session_.model();item["revision"]=double(model->revision);item["point_size"]=model->overlay.pointSize;item["font_size"]=model->overlay.fontSize;
                item["selected"]=views_[0]->selectedPoints().contains(workflowPoint_);item["coordinate_draft"]=coordinates_[0]->value();item["coordinate_changed"]=coordinatesChanged_[0];
                const auto at=views_[0]->project({0,0,0});item["camera_origin"]=QJsonArray{at.x(),at.y()};
                const auto folder=workflowOutput_+'/'+QString::number(next);QDir().mkpath(folder);
                ModelSession::atomicCopy(session_.modelPath(),folder+"/model.json");ModelSession::atomicCopy(session_.dxfPath(),folder+"/lines.dxf");
                item["workspace_model"]=session_.modelPath();
                views_[0]->image().save(folder+"/view.png");item["model"]=folder+"/model.json";item["dxf"]=folder+"/lines.dxf";item["image"]=folder+"/view.png";
            }
            observations.append(item);preferencesRequest_["observations"]=observations;
        }
        const auto commands=preferencesRequest_.value("commands").toArray();
        if(next>=commands.size()){
            QFile f(workflowOutput_+"/observations.json");if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write preference observations");f.write(QJsonDocument(observations).toJson());
            workflowArtifacts_["observations"]=f.fileName();workflowCase("preference_commands_observed",true);
            workflowCases_.append(QJsonObject{{"name","desktop_acceptance"},{"status","not_run"}});finishWorkflow();return;
        }
        const auto command=commands[next].toObject();preferencesRequest_["next"]=next+1;
        QTimer::singleShot(0,this,[this,command]{try{
            const auto kind=command.value("kind").toString();
            if(kind=="load"){session_.load(command.value("path").toString());return;}
            if(kind=="save"){session_.save(command.value("path").toString());return;}
            if(kind=="undo"){session_.undo();return;}
            if(kind=="redo"){session_.redo();return;}
            if(kind=="move"){
                for(const auto& point:session_.model()->points)if(!point.locked&&point.batchMovable){workflowPoint_=point.id;auto p=point.position;p.x+=0.125;session_.movePoint(point.id,p);return;}
                throw std::runtime_error("Fixture has no movable point");
            }
            if(kind=="draft"){
                if(!workflowPoint_)workflowPoint_=session_.model()->points.front().id;
                selectPoint(workflowPoint_);coordinates_[0]->setValue(command.value("value").toDouble());views_[0]->panBy({23,17});views_[0]->zoomAt({180,150},1.1);preferencesStep("draft");return;
            }
            if(kind!="preferences")throw std::runtime_error("Unknown preferences probe command");
            QList<QAction*> actions;for(auto* action:findChildren<QAction*>())if(action->property("operation")=="preferences"&&action->isEnabled())actions.append(action);
            if(actions.size()!=1){workflowCases_.append(QJsonObject{{"name","preferences_action_unavailable"},{"status","not_run"}});finishWorkflow();return;}
            QTimer::singleShot(0,this,[this,command]{try{
                if(!preferencesDialog_)throw std::runtime_error("Preferences callback did not open a dialog");
                auto* dialog=preferencesDialog_;auto fields=command.value("fields").toObject();QJsonArray discovered;
                QJsonArray imports;
                for(const auto& file:command.value("legacy_imports").toArray()){
                    QList<QPushButton*> buttons;for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("preference_action")=="import_legacy")buttons.append(b);
                    if(buttons.size()!=1){workflowCases_.append(QJsonObject{{"name","preferences_import_unavailable"},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                    bool handled=false;
                    QTimer::singleShot(0,dialog,[dialog,file,&handled]{
                        for(auto* picker:dialog->findChildren<QFileDialog*>())if(picker->property("preference_file_action")=="import_legacy"){
                            handled=true;
                            if(file.isNull())picker->reject();
                            else {picker->selectFile(file.toString());QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);}
                            return;
                        }
                        dialog->reject();
                    });
                    buttons.front()->click();
                    if(!handled)throw std::runtime_error("Import callback did not open the reviewed file dialog");
                    QString message;for(auto* label:dialog->findChildren<QLabel*>())if(label->property("preference_error").toBool())message=label->text();
                    imports.append(QJsonObject{{"file",file},{"draft",dialog->values()},{"report",dialog->property("preference_import_report").toJsonObject()},{"message",message},{"persisted",QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object()}});
                }
                for(auto* object:dialog->findChildren<QWidget*>())if(object->property("preference_field").isValid()){
                    QJsonObject field{{"field",object->property("preference_field").toString()},{"type",object->metaObject()->className()}};
                    if(object->property("color").isValid())field["color"]=object->property("color").value<QColor>().name();
                    if(auto* spin=qobject_cast<QSpinBox*>(object))field["value"]=spin->value();
                    if(auto* edit=qobject_cast<QLineEdit*>(object))field["value"]=edit->text();
                    if(auto* combo=qobject_cast<QComboBox*>(object))field["value"]=combo->currentText();
                    discovered.append(field);
                }
                for(auto it=fields.begin();it!=fields.end();++it){
                    QList<QWidget*> found;for(auto* w:dialog->findChildren<QWidget*>())if(w->property("preference_field").toString()==it.key())found.append(w);
                    if(found.size()!=1){workflowCases_.append(QJsonObject{{"name","preference_field_unavailable:"+it.key()},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                    auto* w=found.front();if(auto* s=qobject_cast<QSpinBox*>(w))s->setValue(it.value().toInt());
                    else if(auto* e=qobject_cast<QLineEdit*>(w))e->setText(it.value().toString());
                    else if(auto* c=qobject_cast<QComboBox*>(w)){
                        c->setCurrentText(it.value().toString());
                        if(it.key()=="theme")QMetaObject::invokeMethod(c,"activated",Qt::DirectConnection,Q_ARG(int,c->currentIndex()));
                    }
                    else throw std::runtime_error("Unreviewed preference field adapter");
                }
                dialog->grab().save(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+".png");
                for(auto* tabs:dialog->findChildren<QTabWidget*>()){
                    const int current=tabs->currentIndex();
                    for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);dialog->grab().save(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+"-tab-"+QString::number(i)+".png");}
                    tabs->setCurrentIndex(current);
                }
                if(command.value("block_next_request").toBool()){
                    QDir current(QFileInfo(session_.modelPath()).absolutePath());const auto name=current.dirName();bool ok=false;int n=name.mid(8).toInt(&ok);
                    if(!name.startsWith("request-")||!ok||!current.cdUp())throw std::runtime_error("No private request directory for failure injection");
                    QFile blocked(current.filePath("request-"+QString::number(n+1)));
                    if(!blocked.open(QIODevice::WriteOnly|QIODevice::NewOnly))throw std::runtime_error("Cannot inject request directory failure");
                    blocked.write("blocked");
                }
                if(command.contains("concurrent_write")){QFile f(preferencesStore_->filename());if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot inject concurrent preference write");f.write(QJsonDocument(command.value("concurrent_write").toObject()).toJson());}
                const auto action=command.value("action").toString("apply");QList<QPushButton*> buttons;
                for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("preference_action")==action)buttons.append(b);
                if(buttons.size()!=1){workflowCases_.append(QJsonObject{{"name","preference_button_unavailable"},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                buttons[0]->click();QString message;for(auto* l:dialog->findChildren<QLabel*>())if(l->property("preference_error").toBool())message=l->text();
                preferencesRequest_["dialog_observation"]=QJsonObject{{"fields",discovered},{"message",message},{"draft",dialog->values()},{"imports",imports}};
                QFile evidence(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+".json");
                if(evidence.open(QIODevice::WriteOnly))evidence.write(QJsonDocument(preferencesRequest_.value("dialog_observation").toObject()).toJson());
                if(action=="apply"&&!command.value("reject_after_apply").toBool()&&!preferencesPending_&&dialog->result()!=QDialog::Accepted){workflowCase("preferences_apply_completed",false,message);dialog->reject();}
                if(command.value("reject_after_apply").toBool()||action=="reset_graphics")dialog->reject();
            }catch(const std::exception& e){if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->reject();}finishWorkflow(QString::fromUtf8(e.what()));}});
            actions[0]->trigger();
        }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}});
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
