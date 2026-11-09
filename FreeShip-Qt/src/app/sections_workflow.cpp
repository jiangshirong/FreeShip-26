// Hidden tests of production actions; no desktop input or user files.
#include "main_window.h"
#include "section_dialog.h"
#include "layer_panel.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>
#include <stdexcept>
namespace {
QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing section evidence");return f.readAll();}
}
void MainWindow::verifySections(const QString& model,const QString& output){sectionsWorkflow_=true;verifyWorkflow(model,output);}
void MainWindow::sectionsStep(const QString& method){
    if(workflowFinished_)return;
    try{
        auto model=session_.model();
        auto action=[this](const QString& operation){
            QAction* found=nullptr;for(auto* a:sections_->findChildren<QAction*>())if(a->property("section_operation")==operation){if(found)throw std::runtime_error("Ambiguous section action");found=a;}
            if(!found)throw std::runtime_error("Missing section action");
            return found;
        };
        auto dialogAction=[this](QAction* a,bool accept,QJsonObject values=QJsonObject{}){
            if(!a->isEnabled())throw std::runtime_error("Disabled production action");
            bool handled=false;QString error;QTimer timer;timer.setSingleShot(true);
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog){error="No production dialog";return;}
                if(!dialog->testAttribute(Qt::WA_DontShowOnScreen)){error="Dialog is not private";dialog->reject();return;}
                for(auto* field:dialog->findChildren<QDoubleSpinBox*>()){
                    auto key=field->property("section_field").toString();if(values.contains(key))field->setValue(values[key].toDouble());
                }
                if(values.contains("name"))for(auto* field:dialog->findChildren<QLineEdit*>())if(!qobject_cast<QDoubleSpinBox*>(field->parent()))field->setText(values["name"].toString());
                if(values.contains("layer"))for(auto* field:dialog->findChildren<QComboBox*>())field->setCurrentIndex(field->findData(QVariant::fromValue(quint64(values["layer"].toDouble()))));
                auto* buttons=dialog->findChild<QDialogButtonBox*>();auto* button=buttons?buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel):nullptr;
                if(!button&&buttons)button=buttons->button(accept?QDialogButtonBox::Yes:QDialogButtonBox::No);
                if(!button){error="Missing button role";dialog->reject();return;}handled=true;button->click();
            });
            timer.start(0);a->trigger();if(!handled||!error.isEmpty())throw std::runtime_error(error.isEmpty()?"Dialog was not handled":error.toStdString());
        };
        auto table=[this]{return sections_->findChild<QTableWidget*>("SectionDefinitions");};
        const auto outside=double(float(editingOriginal_?editingOriginal_->maximum.x+10:model->maximum.x+10));
        switch(workflowStage_++){
        case 0:{
            if(method!="load")throw std::runtime_error("Expected load");
            editingOriginal_=model;captureWorkflow("original");
            workflowOriginalDigest_=bytes(session_.scenePath());sectionsAction_->trigger();
            workflowCase("section_action_opens_production_dialog",sections_&&sections_->isVisible());
            sections_->grab().save(workflowOutput_+"/section-dialog.png");
            dialogAction(action("add"),false,{{"start",outside}});
            workflowCase("add_cancel_preserves_revision",session_.model()==model&&!session_.busy());
            dialogAction(action("add_range"),true,{{"start",outside},{"end",outside+2},{"spacing",1}});break;
        }
        case 1:{
            captureWorkflow("added");workflowCase("range_add_three_definitions",model->sections.size()==editingOriginal_->sections.size()+3);
            int index=-1;for(const auto& s:model->sections)if(s.kind==1&&float(s.distance)==float(outside))index=s.index;
            auto* t=table();int row=-1;for(int i=0;i<t->rowCount();++i)if(t->item(i,0)->data(Qt::UserRole).toInt()==index)row=i;
            if(row<0)throw std::runtime_error("Added section missing from table");
            workflowCase("display_distance_roundtrips_single",float(t->item(row,1)->text().toDouble())==float(outside));
            t->selectRow(row);workflowCase("selection_uses_revision_local_index",sections_->selectedIndices().size()==1);
            t->item(row,1)->setText(QString::number(outside+0.25,'g',12));break;
        }
        case 2:{
            captureWorkflow("edited");workflowCase("distance_editor_updates_definition",std::any_of(model->sections.begin(),model->sections.end(),[&](const auto& s){return s.kind==1&&s.distance==float(outside+0.25);}));
            workflowCase("new_revision_clears_section_selection",sections_->selectedIndices().isEmpty());
            auto* t=table();for(int row=0;row<t->rowCount();++row)if(float(t->item(row,1)->text().toDouble())==float(outside+0.25))t->selectRow(row);
            dialogAction(action("delete"),false);workflowCase("delete_cancel_preserves_revision",session_.model()==model&&!session_.busy());
            dialogAction(action("delete"),true);break;
        }
        case 3:
            captureWorkflow("deleted");workflowCase("delete_removes_one_definition",model->sections.size()==editingOriginal_->sections.size()+2);workflowMovedDigest_=bytes(session_.scenePath());
            sections_->reject();undo_->trigger();break;
        case 4:
            captureWorkflow("undo");workflowCase("undo_restores_definition",model->sections.size()==editingOriginal_->sections.size()+3);redo_->trigger();break;
        case 5:{
            captureWorkflow("redo");workflowCase("redo_scene_exact",bytes(session_.scenePath())==workflowMovedDigest_);
            sectionsAction_->trigger();auto* tabs=sections_->findChild<QTabWidget*>();
            for(int k=1;k<=4;++k){tabs->setCurrentIndex(k-1);int count=0;for(const auto& s:model->sections)if(s.kind==k)++count;workflowCase("tab_definitions_"+QString::number(k),table()->rowCount()==count);}
            double distance=double(float((model->minimum.y+model->maximum.y+model->minimum.z+model->maximum.z)/2));
            while(std::any_of(model->sections.begin(),model->sections.end(),[&](const auto& s){return s.kind==4&&std::abs(s.distance-distance)<1e-5;}))distance=double(float(distance+0.017));
            dialogAction(action("add"),true,{{"start",distance}});break;
        }
        case 6:
            workflowCase("diagonal_added",model->sections.size()==editingOriginal_->sections.size()+3);sections_->reject();
            dialogAction(newLayer_,false,{{"name","Cancelled"}});workflowCase("layer_cancel_preserves_model",session_.model()==model&&!session_.busy());
            dialogAction(newLayer_,true,{{"name","Section workflow"}});break;
        case 7:
            captureWorkflow("layer");workflowCase("new_layer_added",model->layers.size()==editingOriginal_->layers.size()+1);
            editingLayer_=model->layers.back().id;
            workflowCase("new_layer_selected_for_property_editing",layers_->selectedLayerId()==editingLayer_);
            dialogAction(deleteLayer_,false);workflowCase("layer_delete_cancel_preserves_model",session_.model()==model&&!session_.busy());
            dialogAction(deleteLayer_,true);break;
        case 8:
            workflowCase("empty_layer_deleted",model->layer(editingLayer_)==nullptr);undo_->trigger();break;
        case 9:
            workflowCase("undo_restores_layer_identity",model->layer(editingLayer_)&&model->layer(editingLayer_)->name=="Section workflow");
            setSelectionKind(HullEntity::Face);selectEntities(HullEntity::Face,{model->faces.front().id});dialogAction(assignLayer_,true,{{"layer",double(editingLayer_)}});break;
        case 10:
            workflowCase("existing_face_moves_to_new_layer",model->faces.front().layer==editingLayer_);
            captureWorkflow("complete");editingChanged_=model;workflowMovedDigest_=bytes(session_.scenePath());
            session_.save(workflowOutput_+"/sections-saved.fbm");break;
        case 11:
            workflowCase("save_clears_modified",method=="save"&&!session_.modified());session_.load(workflowOutput_+"/sections-saved.fbm");break;
        case 12:{
            captureWorkflow("reopened");workflowCase("reopen_scene_exact",bytes(session_.scenePath())==workflowMovedDigest_);
            workflowCase("reopen_sections_and_layers",model->sections.size()==editingChanged_->sections.size()&&model->layers.size()==editingChanged_->layers.size());
            bool same=true;for(int i=0;i<model->sections.size();++i)same&=model->sections[i].plane==editingChanged_->sections[i].plane;
            workflowCase("reopen_section_planes_exact",same);
            auto* view=views_[1];view->fit();view->setControlNetVisible(false);view->setFillVisible(false);
            QVector<QAction*> visibility;
            for(auto* a:findChildren<QAction*>())if(a->property("operation")=="section_visibility")visibility.append(a);
            workflowCase("section_visibility_actions_discovered",visibility.size()==4);
            for(auto* a:visibility)a->trigger();
            auto hidden=view->image();
            for(auto* a:visibility)a->trigger();
            auto shown=view->image();
            bool any=false;for(const auto& s:model->sections)any|=!s.paths.isEmpty();
            workflowCase("curves_visible_in_four_view",any&&shown!=hidden);shown.save(workflowOutput_+"/sections-body-view.png");
            sectionsAction_->trigger();sections_->findChild<QTabWidget*>()->setCurrentIndex(3);
            int curvedIndex=-1;for(const auto& s:model->sections)if(s.kind==4&&!s.paths.isEmpty()){curvedIndex=s.index;break;}
            if(curvedIndex<0)throw std::runtime_error("No visible diagonal for selection verification");
            for(int row=0;row<table()->rowCount();++row)if(table()->item(row,0)->data(Qt::UserRole).toInt()==curvedIndex)table()->selectRow(row);
            workflowCase("table_selection_highlights_world_curve",view->image()!=shown);
            sections_->grab().save(workflowOutput_+"/section-dialog-edited.png");sections_->reject();
            for(auto* v:views_){v->fit();for(int i=0;i<10;++i){v->panBy({1,2});v->zoomAt({200,100},1.01);v->image();}}
            workflowCase("camera_does_not_mutate_or_request_backend",session_.model()==model&&!session_.busy()&&!session_.modified());
            finishWorkflow();break;
        }
        default:throw std::runtime_error("Unexpected section workflow stage");
        }
    }catch(const std::exception& error){finishWorkflow(QString::fromUtf8(error.what()));}
}
