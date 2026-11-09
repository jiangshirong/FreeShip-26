// Production-action observer. All inputs/files/settings are owned by FreeShip-Tests.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QPushButton>
#include <QTimer>
#include <QLayout>
#include <stdexcept>

void MainWindow::verifyModeling(const QString& request,const QString& output){
    workflow_=true;modelingWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile file(request);if(!file.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read modeling workflow");return;}
    const auto data=QJsonDocument::fromJson(file.readAll()).object();modelingCommands_=data["commands"].toArray();
    if(modelingCommands_.isEmpty()||!QDir().mkpath(workflowOutput_+"/views")){finishWorkflow("Invalid modeling workflow");return;}
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();for(auto* view:views_){view->resize(640,400);view->setAnimationsEnabled(false);}
    modelingDialogTimer_=new QTimer(this);modelingDialogTimer_->setInterval(10);
    connect(modelingDialogTimer_,&QTimer::timeout,this,[this]{
        const auto command=modelingCommands_[workflowStage_].toObject();const auto operation=command["operation"].toString();
        for(auto* widget:QApplication::topLevelWidgets()){
            auto* dialog=qobject_cast<QDialog*>(widget);if(!dialog||!dialog->isVisible())continue;
            const auto dialogOp=dialog->property("modeling_operation").toString();
            const auto nested=command["dialog_operation"].toString();
            if(dialogOp!=operation&&(nested.isEmpty()||dialogOp!=nested)&&dialog->property("advanced_operation").toString()!=operation)continue;
            if(auto* file=qobject_cast<QFileDialog*>(dialog)){
                if(command["cancel"].toBool()){modelingDialogTimer_->stop();file->reject();return;}
                if(auto* focus=file->focusWidget())focus->clearFocus();
                const auto path=QFileInfo(command["file"].toString());file->setDirectory(path.absolutePath());file->selectFile(path.fileName());
                if(auto* buttons=file->findChild<QDialogButtonBox*>())if(auto* button=buttons->button(QDialogButtonBox::Open)){
                    if(button->isEnabled()){modelingDialogTimer_->stop();button->click();}return;
                }
                modelingDialogTimer_->stop();file->reject();finishWorkflow("File open role not found");return;
            }
            if(dialogOp=="point_groups"){
                auto* list=dialog->findChild<QListWidget*>();if(!list){finishWorkflow("Point group list not found");dialog->reject();return;}
                list->setCurrentRow(command["group_row"].toInt());
                for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("group_action").toInt()==command["group_action"].toInt()){
                    if(command["group_action"].toInt()==1)modelingDialogTimer_->stop();b->click();return;
                }
                dialog->reject();finishWorkflow("Point group action not found");return;
            }
            if(dialogOp=="model_check"){
                if(!dialog->grab().save(workflowOutput_+"/views/"+modelingPhase_+"-dialog.png")){dialog->reject();finishWorkflow("Cannot save check evidence");return;}
                const auto checkAction=command["check_action"].toString();
                if(!checkAction.isEmpty()){
                    QList<QPushButton*> matches;QJsonArray discovered;
                    for(auto* button:dialog->findChildren<QPushButton*>()){
                        const auto action=button->property("check_action").toString();
                        if(!action.isEmpty())discovered.append(QJsonObject{{"action",action},{"enabled",button->isEnabled()}});
                        if(action==checkAction)matches.append(button);
                    }
                    workflowArtifacts_[modelingPhase_+"_check_actions"]=discovered;
                    if(matches.size()!=1||!matches[0]->isEnabled()){
                        workflowCases_.append(QJsonObject{{"name",modelingPhase_+".entry"},{"status","not_run"},{"message","Check dialog action is missing, ambiguous or unavailable"}});
                        modelingDialogTimer_->stop();dialog->reject();return;
                    }
                    matches[0]->click();return;
                }
                modelingDialogTimer_->stop();dialog->reject();return;
            }
            const auto values=command["fields"].toObject();QSet<QString> used;QJsonArray discovered;
            for(auto* control:dialog->findChildren<QWidget*>()){
                const auto key=control->property("field").toString();if(key.isEmpty())continue;discovered.append(key);if(!values.contains(key))continue;used.insert(key);
                if(auto* w=qobject_cast<QDoubleSpinBox*>(control))w->setValue(values[key].toDouble());
                else if(auto* w=qobject_cast<QSpinBox*>(control))w->setValue(values[key].toInt());
                else if(auto* w=qobject_cast<QComboBox*>(control))w->setCurrentIndex(values[key].toInt());
                else if(auto* w=qobject_cast<QCheckBox*>(control))w->setChecked(values[key].toBool());
                else if(auto* w=qobject_cast<QLineEdit*>(control))w->setText(values[key].toString());
                else if(auto* w=qobject_cast<QListWidget*>(control)){for(int i=0;i<w->count();++i)w->item(i)->setCheckState(values[key].toArray().contains(w->item(i)->data(Qt::UserRole).toDouble())?Qt::Checked:Qt::Unchecked);}
            }
            for(auto it=values.begin();it!=values.end();++it)if(!used.contains(it.key())){modelingDialogTimer_->stop();dialog->reject();finishWorkflow("Parameter not discovered: "+it.key());return;}
            workflowArtifacts_[modelingPhase_+"_fields"]=discovered;
            if(!dialog->grab().save(workflowOutput_+"/views/"+modelingPhase_+"-dialog.png")){dialog->reject();finishWorkflow("Cannot save dialog evidence");return;}
            auto* buttons=dialog->findChild<QDialogButtonBox*>();const bool accept=!command["cancel"].toBool();auto* button=buttons?buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel):nullptr;
            if(!button){modelingDialogTimer_->stop();dialog->reject();finishWorkflow("Confirmation role not found");return;}
            modelingDialogTimer_->stop();button->click();return;
        }
    });
    openModel(data["model"].toString());
}

void MainWindow::modelingStep(const QString& method){
    if(method=="inspect_modeling")return;
    modelingDialogTimer_->stop();
    try{
        if(!modelingPhase_.isEmpty()){
            captureWorkflow(modelingPhase_);const auto command=modelingCommands_[workflowStage_].toObject();
            QJsonArray selected;for(auto id:selectionKind_==HullEntity::Point?selection_:entitySelection_)selected.append(double(id));workflowArtifacts_[modelingPhase_+"_selection"]=selected;
            if(command["cancel"].toBool())workflowCase(modelingPhase_+".cancel-preserves-revision",double(session_.model()->revision)==modelingOriginal_["revision"].toDouble());
            ++workflowStage_;
        }else captureWorkflow("original");
        if(workflowStage_>=modelingCommands_.size()){modelingDialogTimer_->stop();finishWorkflow();return;}
        const auto command=modelingCommands_[workflowStage_].toObject();modelingPhase_=command["phase"].toString();if(modelingPhase_.isEmpty())throw std::runtime_error("Missing phase name");
        const auto m=session_.model();const auto selected=command["select"].toObject();
        if(!selected.isEmpty()){
            const auto name=selected["kind"].toString();const auto kind=name=="point"?HullEntity::Point:name=="edge"?HullEntity::Edge:name=="face"?HullEntity::Face:HullEntity::Curve;
            QVector<fs::EntityId> ordered;for(auto id:selected["ids"].toArray())ordered.append(fs::EntityId(id.toDouble()));
            if(selected["all"].toBool()){
                if(kind==HullEntity::Point)for(const auto& p:m->points)ordered.append(p.id);
                else if(kind==HullEntity::Face)for(const auto& f:m->faces)ordered.append(f.id);
                else if(kind==HullEntity::Edge)for(const auto& e:m->edges)ordered.append(e.id);
            }
            for(auto coord:selected["positions"].toArray()){const auto p=coord.toArray();bool found=false;for(const auto& point:m->points)if(point.position.x==p[0].toDouble()&&point.position.y==p[1].toDouble()&&point.position.z==p[2].toDouble()){if(found)throw std::runtime_error("Ambiguous coordinate selector");ordered.append(point.id);found=true;}if(!found)throw std::runtime_error("Coordinate selector found no point");}
            setSelectionKind(kind);
            if(kind==HullEntity::Point){
                // Feed the same incremental selection callback as ordered clicks.
                // One unordered set cannot express first/last constraint endpoints.
                selectPoints({});for(auto id:ordered){auto ids=selection_;ids.insert(id);selectPoints(ids);}
            }else selectEntities(kind,QSet<fs::EntityId>(ordered.begin(),ordered.end()));
        }
        const auto op=command["operation"].toString();modelingOriginal_={{"revision",double(m->revision)}};
        if(op=="save"){session_.save(workflowOutput_+"/workflow-saved.fbm");return;}
        if(op=="reopen"){session_.load(workflowOutput_+"/workflow-saved.fbm");return;}
        if(op=="undo"){session_.undo();return;}if(op=="redo"){session_.redo();return;}
        QList<QAction*> matches;for(auto* action:findChildren<QAction*>())if(action->property("operation").toString()==op)matches.append(action);
        QJsonArray candidates;for(auto* action:matches)candidates.append(QJsonObject{{"text",action->text()},{"enabled",action->isEnabled()}});workflowArtifacts_[modelingPhase_+"_candidates"]=candidates;
        if(matches.size()!=1||!matches[0]->isEnabled()){
            workflowCases_.append(QJsonObject{{"name",modelingPhase_+".entry"},{"status","not_run"},{"message","Missing, ambiguous or unavailable production action"}});
            QTimer::singleShot(0,this,[this]{modelingStep("skipped");});return;
        }
        modelingDialogTimer_->start();matches[0]->trigger();
        if(!session_.busy()){modelingDialogTimer_->stop();QTimer::singleShot(0,this,[this]{modelingStep("local");});}
    }catch(const std::exception& ex){modelingDialogTimer_->stop();finishWorkflow(QString::fromUtf8(ex.what()));}
}
