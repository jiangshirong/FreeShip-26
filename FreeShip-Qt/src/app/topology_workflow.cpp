#include "main_window.h"
#include "studio_ui.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMouseEvent>
#include <QTabWidget>
#include <QToolButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QComboBox>
#include <stdexcept>

namespace {
QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing topology artifact");return f.readAll();}
}
void MainWindow::verifyTopology(const QString& model,const QString& output){
    topologyWorkflow_=true;verifyWorkflow(model,output);
}
void MainWindow::topologyStep(const QString& method){
    if(workflowFinished_)return;
    try {
        auto model=session_.model();
        auto dialogAction=[this](QAction* action,bool accept,fs::Vec3 position=fs::Vec3{}) {
            if(!action->isEnabled())throw std::runtime_error("Production dialog action is disabled");
            bool handled=false;QString error;QTimer timer;timer.setSingleShot(true);
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog){error="Production modal dialog not found";return;}
                if(!dialog->testAttribute(Qt::WA_DontShowOnScreen)){error="Verification dialog is not private";dialog->reject();return;}
                const auto coordinates=dialog->findChildren<QDoubleSpinBox*>();
                if(coordinates.size()==3){coordinates[0]->setValue(position.x);coordinates[1]->setValue(position.y);coordinates[2]->setValue(position.z);}
                auto* buttons=dialog->findChild<QDialogButtonBox*>();
                auto* button=buttons?buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel):nullptr;
                if(!button && buttons)button=buttons->button(accept?QDialogButtonBox::Yes:QDialogButtonBox::No);
                if(button){handled=true;button->click();}else {error="Missing dialog button role";dialog->reject();}
            });
            timer.start(0);action->trigger();timer.stop();
            if(!handled)throw std::runtime_error(error.isEmpty()?"Production dialog was not invoked":error.toStdString());
        };
        auto newPosition=[&](int index){return fs::Vec3{editingOriginal_->maximum.x+4+(index==1?2:0),index==2?3.0:1.0,editingOriginal_->maximum.z+2};};
        auto rememberPoint=[&]{
            const fs::ControlPoint* created=nullptr;
            for(const auto& p:model->points)if(!editingChanged_->point(p.id)){if(created)throw std::runtime_error("Unexpected extra point");created=&p;}
            if(!created)throw std::runtime_error("New point missing");
            editingMoves_.append({created->id,created->position});editingChanged_=model;
        };
        switch(workflowStage_++) {
        case 0: {
            if(method!="load")throw std::runtime_error("Expected topology load");
            captureWorkflow("original");editingOriginal_=model;workflowOriginalDigest_=read(session_.scenePath());
            // QWidget::grab activates the complete hidden-window layout. Do
            // not inspect child geometry left over from the probe's initial sizes.
            const auto initialImage=grab();
            workflowCase("studio_toolbar_present",findChild<QWidget*>("StudioToolbar")!=nullptr);
            workflowCase("studio_uses_original_icons",!studioIcon("Point").isNull()&&!studioIcon("ViewBody").isNull());
            workflowCase("original_four_view_order",viewFrames_[1]->geometry().left()<viewFrames_[0]->geometry().left()&&viewFrames_[1]->geometry().top()==viewFrames_[0]->geometry().top());
            workflowCase("inspector_initially_collapsed",inspector_->isHidden());
            viewActions_[1]->trigger();workflowCase("view_toggle_hides_only_own_view",viewFrames_[1]->isHidden()&&!viewFrames_[0]->isHidden());
            viewActions_[1]->trigger();QApplication::processEvents();
            workflowCase("view_toggle_restores_view",!viewFrames_[1]->isHidden());
            auto* toolbar=findChild<QWidget*>("StudioToolbar");
            toolbar->grab().save(workflowOutput_+"/studio-toolbar.png");
            initialImage.save(workflowOutput_+"/studio-main-window.png");
            const auto originalSize=size();resize(860,640);QApplication::processEvents();
            grab().save(workflowOutput_+"/studio-narrow.png");
            bool overflow=false;
            for(auto* button:toolbar->findChildren<QToolButton*>())if(button->text()=="更多"&&button->isVisibleTo(toolbar)&&button->menu())
                overflow=!button->menu()->actions().isEmpty();
            workflowCase("narrow_toolbar_keeps_overflow_entries",overflow);resize(originalSize);QApplication::processEvents();
            setSelectionKind(HullEntity::Edge);
            const fs::ControlEdge* chosen=nullptr;
            for(const auto& edge:model->edges){auto* a=model->point(edge.a);auto* b=model->point(edge.b);if(a&&b&&a->batchMovable&&b->batchMovable){chosen=&edge;break;}}
            if(!chosen)throw std::runtime_error("No editable edge");
            workflowPoint_=chosen->id;
            selectEntities(HullEntity::Edge,{chosen->id});
            workflowCase("split_action_available",splitEdge_->isEnabled());splitEdge_->trigger();break;
        }
        case 1: {
            if(method!="topology")throw std::runtime_error("Expected edge split");
            captureWorkflow("split");editingChanged_=model;workflowMovedDigest_=read(session_.scenePath());
            workflowCase("split_creates_one_point_and_edge",model->points.size()==editingOriginal_->points.size()+1&&model->edges.size()==editingOriginal_->edges.size()+1&&model->faces.size()==editingOriginal_->faces.size());
            bool unchanged=true;
            for(const auto& point:editingOriginal_->points){auto* p=model->point(point.id);unchanged=unchanged&&p&&p->position.x==point.position.x&&p->position.y==point.position.y&&p->position.z==point.position.z;}
            workflowCase("split_keeps_existing_point_ids_and_coordinates",unchanged);
            undo_->trigger();break;
        }
        case 2:
            captureWorkflow("undo");workflowCase("undo_restores_scene",read(session_.scenePath())==workflowOriginalDigest_);
            workflowCase("undo_restores_point_count",model->points.size()==editingOriginal_->points.size());redo_->trigger();break;
        case 3:
            captureWorkflow("redo");workflowCase("redo_restores_scene",read(session_.scenePath())==workflowMovedDigest_);
            workflowCase("redo_restores_split_point_ids",model->points.size()==editingChanged_->points.size()&&model->points.back().id==editingChanged_->points.back().id);
            if(!session_.save(workflowOutput_+"/topology-saved.fbm"))throw std::runtime_error("Save not submitted");
            break;
        case 4:
            if(method!="save")throw std::runtime_error("Expected topology save");
            workflowCase("save_clears_modified",!session_.modified());
            if(!session_.load(workflowOutput_+"/topology-saved.fbm"))throw std::runtime_error("Reopen not submitted");
            break;
        case 5:
            captureWorkflow("reopened");workflowCase("reopened_topology_counts",model->points.size()==editingChanged_->points.size()&&model->edges.size()==editingChanged_->edges.size()&&model->faces.size()==editingChanged_->faces.size());
            workflowCase("reopened_scene_exact",read(session_.scenePath())==workflowMovedDigest_);
            editingChanged_=model;editingMoves_.clear();
            dialogAction(addPoint_,false,newPosition(0));
            workflowCase("new_point_cancel_preserves_document",session_.model()==model&&!session_.busy()&&!session_.modified());
            dialogAction(addPoint_,true,newPosition(0));break;
        case 6:
            rememberPoint();workflowCase("new_point_dialog_exact_coordinates",editingMoves_.back().position.x==float(newPosition(0).x)&&editingMoves_.back().position.y==float(newPosition(0).y)&&editingMoves_.back().position.z==float(newPosition(0).z));
            dialogAction(addPoint_,true,newPosition(1));break;
        case 7:
            rememberPoint();dialogAction(addPoint_,true,newPosition(2));break;
        case 8: {
            rememberPoint();setSelectionKind(HullEntity::Point);QSet<fs::EntityId> ordered;
            for(const auto& p:editingMoves_){ordered.insert(p.id);selectPoints(ordered);}
            dialogAction(addFace_,true);break;
        }
        case 9: {
            const fs::ControlFace* added=nullptr;
            for(const auto& face:model->faces)if(std::none_of(editingChanged_->faces.begin(),editingChanged_->faces.end(),[&](const auto& old){return old.id==face.id;}))added=&face;
            if(!added)throw std::runtime_error("New face missing");
            QVector<fs::EntityId> ordered;for(const auto& p:editingMoves_)ordered.append(p.id);
            workflowCase("new_face_keeps_click_order",added->points==ordered);
            editingLayer_=added->id;editingChanged_=model;captureWorkflow("created-face");
            setSelectionKind(HullEntity::Face);selectEntities(HullEntity::Face,{editingLayer_});
            dialogAction(assignLayer_,true);break;
        }
        case 10:
            workflowCase("assign_same_layer_is_no_op",model->revision==editingChanged_->revision);
            dialogAction(delete_,false);workflowCase("delete_cancel_preserves_model_and_history",session_.model()==model&&!session_.busy());
            dialogAction(delete_,true);break;
        case 11:
            workflowCase("delete_action_removes_selected_face",std::none_of(model->faces.begin(),model->faces.end(),[this](const auto& face){return face.id==editingLayer_;}));
            captureWorkflow("deleted-face");undo_->trigger();break;
        case 12:
            workflowCase("undo_restores_deleted_face_identity",std::any_of(model->faces.begin(),model->faces.end(),[this](const auto& face){return face.id==editingLayer_;}));
            setSelectionKind(HullEntity::Point);selectPoints({editingMoves_.front().id});lock_->trigger();break;
        case 13:
            workflowCase("lock_action_updates_model",model->point(editingMoves_.front().id)&&model->point(editingMoves_.front().id)->locked);unlock_->trigger();break;
        case 14:
            workflowCase("unlock_action_updates_model",model->point(editingMoves_.front().id)&&!model->point(editingMoves_.front().id)->locked);
            captureWorkflow("complete");editingChanged_=model;workflowMovedDigest_=read(session_.scenePath());
            if(!session_.save(workflowOutput_+"/complete-topology-saved.fbm"))throw std::runtime_error("Complete save not submitted");
            break;
        case 15:
            if(!session_.load(workflowOutput_+"/complete-topology-saved.fbm"))throw std::runtime_error("Complete reopen not submitted");
            break;
        case 16:
            captureWorkflow("complete-reopened");workflowCase("complete_roundtrip_scene_exact",read(session_.scenePath())==workflowMovedDigest_);
            workflowCase("complete_roundtrip_counts",model->points.size()==editingChanged_->points.size()&&model->faces.size()==editingChanged_->faces.size());
            finishWorkflow();break;
        default:throw std::runtime_error("Unexpected topology workflow stage");
        }
    }catch(const std::exception& error){finishWorkflow(QString::fromUtf8(error.what()));}
}
