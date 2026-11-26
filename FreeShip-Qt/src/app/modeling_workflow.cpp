// Production-action observer. All inputs/files/settings are owned by FreeShip-Tests.
#include "main_window.h"
#include "point_coordinate_popup.h"
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
#include <QLabel>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHelpEvent>
#include <QDir>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPushButton>
#include <QTimer>
#include <QToolTip>
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
            if(dialogOp=="repair_normals"){
                auto* apply=dialog->findChild<QPushButton*>("FaceOrientationApply");
                auto* preview=dynamic_cast<HullView*>(dialog->findChild<QWidget*>("FaceOrientationPreview"));
                auto* summary=dialog->findChild<QLabel*>("FaceOrientationSummary");
                if(!apply||!preview||!summary){modelingDialogTimer_->stop();dialog->reject();finishWorkflow("Face orientation preview is incomplete");return;}
                const bool repaired=dialog->property("face_orientation_repaired").toBool();
                if(!repaired&&!apply->isEnabled())return;
                const auto suffix=repaired?QStringLiteral("after"):QStringLiteral("before");
                const auto name=modelingPhase_+"_"+suffix;
                const auto path=workflowOutput_+"/views/"+name+".png";
                const auto canvas=workflowOutput_+"/views/"+name+"_canvas.png";
                if(!dialog->grab().save(path)||!preview->image().save(canvas)){
                    modelingDialogTimer_->stop();dialog->reject();finishWorkflow("Cannot capture orientation preview");return;
                }
                workflowArtifacts_[name]=QJsonObject{{"image",path},{"canvas",canvas},{"summary",summary->text()},
                    {"reference_a",QJsonArray{preview->project({0,0,0}).x(),preview->project({0,0,0}).y()}},
                    {"reference_b",QJsonArray{preview->project({1,1,1}).x(),preview->project({1,1,1}).y()}}};
                if(repaired||command["cancel"].toBool()){
                    modelingDialogTimer_->stop();dialog->reject();
                    QTimer::singleShot(0,this,[this]{modelingStep("local");});
                }else apply->click();
                return;
            }
            if(dialogOp=="delete"){
                auto* buttons=dialog->findChild<QDialogButtonBox*>();
                auto* button=buttons?buttons->button(command["cancel"].toBool()?QDialogButtonBox::No:QDialogButtonBox::Yes):nullptr;
                modelingDialogTimer_->stop();
                if(!button){dialog->reject();finishWorkflow("Delete confirmation role not found");return;}
                button->click();
                return;
            }
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
                workflowArtifacts_[modelingPhase_+"_title"]=dialog->windowTitle();
                auto* list=dialog->findChild<QListWidget*>();
                if(list&&command.contains("check_row"))list->setCurrentRow(command["check_row"].toInt());
                const auto path=workflowOutput_+"/views/"+modelingPhase_+"-dialog.png";
                if(!dialog->grab().save(path)){dialog->reject();finishWorkflow("Cannot save check evidence");return;}
                workflowArtifacts_[modelingPhase_+"_dialog_image"]=path;
                QJsonArray issues;
                if(list)for(int row=0;row<list->count();++row)issues.append(list->item(row)->text());
                workflowArtifacts_[modelingPhase_+"_issues"]=issues;
                const auto checkAction=command["check_action"].toString();
                QList<QPushButton*> matches;QJsonArray discovered;
                for(auto* button:dialog->findChildren<QPushButton*>()){
                    const auto action=button->property("check_action").toString();
                    if(!action.isEmpty())discovered.append(QJsonObject{{"action",action},{"visible",button->isVisible()},{"enabled",button->isEnabled()}});
                    if(action==checkAction&&button->isVisible())matches.append(button);
                }
                workflowArtifacts_[modelingPhase_+"_check_actions"]=discovered;
                if(!checkAction.isEmpty()){
                    if(matches.size()!=1||!matches[0]->isEnabled()){
                        workflowCases_.append(QJsonObject{{"name",modelingPhase_+".entry"},{"status","not_run"},{"message","Check dialog action is missing, ambiguous or unavailable"}});
                        modelingDialogTimer_->stop();dialog->reject();return;
                    }
                    if(checkAction=="locate"){
                        modelingDialogTimer_->stop();
                        matches[0]->click();
                        QJsonArray counts;
                        for(auto* view:views_)counts.append(selectionKind_==HullEntity::Point?view->selectedPoints().size():view->selectedEntities().size());
                        workflowArtifacts_[modelingPhase_+"_view_selection_counts"]=counts;
                        views_[3]->image().save(workflowOutput_+"/views/"+modelingPhase_+"-selection.png");
                        if(dialog->isVisible())dialog->reject();
                        return;
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
        if(op=="clear_selection"){
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(views_.front(),&escape);
            QTimer::singleShot(0,this,[this]{modelingStep("local");});
            return;
        }
        if(op=="inspect_faces"){
            auto* view=views_[3];
            const auto previous=view->displayModeKey();
            if(!view->selectDisplayMode(op))throw std::runtime_error("Could not inspect the fixture's control faces");
            view->fit();
            const auto canvas=workflowOutput_+"/views/"+modelingPhase_+"_canvas.png";
            if(!view->image().save(canvas))throw std::runtime_error("Could not capture control faces");
            workflowArtifacts_[modelingPhase_+"_canvas"]=canvas;
            view->selectDisplayMode(previous);
            QTimer::singleShot(0,this,[this]{modelingStep("local");});
            return;
        }
        if(op=="point_properties"){
            if(!selected_||!views_[3]->onRepeatPointClick)throw std::runtime_error("No selected point for inline editor");
            // Selection was applied above. Exercise its inline form; actual mouse
            // press/release and hit testing are covered by verify-point-transform.
            views_[3]->onRepeatPointClick(selected_);
            auto* name=pointPopup_?pointPopup_->findChild<QLineEdit*>("PointName"):nullptr;
            auto* type=pointPopup_?pointPopup_->findChild<QComboBox*>():nullptr;
            if(!name||name->isHidden()||!type||!pointPropertiesPage_)throw std::runtime_error("Missing point property fields");
            auto fields=command["fields"].toObject();if(fields.contains("name"))name->setText(fields["name"].toString());
            if(fields.contains("vertex_type"))type->setCurrentIndex(fields["vertex_type"].toInt());
            if(command["cancel"].toBool())pointPopup_->hide();
            else if(auto* apply=pointPopup_->findChild<QPushButton*>())apply->click();
            if(!session_.busy())QTimer::singleShot(0,this,[this]{modelingStep("local");});return;
        }
        QList<QAction*> matches;for(auto* action:findChildren<QAction*>())if(action->property("operation").toString()==op)matches.append(action);
        if(matches.size()==1&&matches[0]->property("model_check_targets").toBool()){
            auto* action=matches[0];
            for(auto* object:action->associatedObjects())if(auto* menu=qobject_cast<QMenu*>(object)){
                if(command["observe_tooltip"].toBool()){
                    menu->setAttribute(Qt::WA_DontShowOnScreen);
                    menu->popup({60,60});
                    const auto row=menu->actionGeometry(action);
                    QHelpEvent hover(QEvent::ToolTip,row.center(),menu->mapToGlobal(row.center()));
                    QApplication::sendEvent(menu,&hover);
                    workflowArtifacts_[modelingPhase_+"_tooltip"]=QJsonObject{{"visible",QToolTip::isVisible()},
                        {"text",QToolTip::isVisible()?QToolTip::text():QString{}},{"hit",menu->actionAt(row.center())==action}};
                    QToolTip::hideText();
                    menu->hide();
                }else QMetaObject::invokeMethod(menu,"aboutToShow");
            }
        }
        QJsonArray candidates;for(auto* action:matches)candidates.append(QJsonObject{{"text",action->text()},{"enabled",action->isEnabled()}});workflowArtifacts_[modelingPhase_+"_candidates"]=candidates;
        if(command["observe_only"].toBool()){QTimer::singleShot(0,this,[this]{modelingStep("local");});return;}
        if(matches.size()!=1||!matches[0]->isEnabled()){
            workflowCases_.append(QJsonObject{{"name",modelingPhase_+".entry"},{"status","not_run"},{"message","Missing, ambiguous or unavailable production action"}});
            QTimer::singleShot(0,this,[this]{modelingStep("skipped");});return;
        }
        if(op=="delete")workflowArtifacts_[modelingPhase_+"_has_delete_shortcut"]=matches[0]->shortcuts().contains(QKeySequence::Delete);
        modelingDialogTimer_->start();matches[0]->trigger();
        if(op=="repair_normals"&&faceOrientationWindow_)return;
        if(!session_.busy()){modelingDialogTimer_->stop();QTimer::singleShot(0,this,[this]{modelingStep("local");});}
    }catch(const std::exception& ex){modelingDialogTimer_->stop();finishWorkflow(QString::fromUtf8(ex.what()));}
}
