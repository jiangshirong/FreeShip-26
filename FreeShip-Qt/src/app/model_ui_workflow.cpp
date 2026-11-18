// SPDX-License-Identifier: GPL-2.0-or-later
// Private production-action probe. No desktop control or user settings writes.
#include "main_window.h"
#include "layer_panel.h"
#include "point_coordinate_popup.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <stdexcept>
#include <algorithm>
#include <cmath>
void MainWindow::verifyModelUi(const QString& model,const QString& output){
    modelUiWorkflow_=true;QApplication::setQuitOnLastWindowClosed(false);
    // An inline overlay is visible only when its canvas ancestors are shown.
    // Exercise that real hierarchy on the private desktop, without displaying
    // a window on the user's desktop or relying on grab() to initialize it.
    setAttribute(Qt::WA_DontShowOnScreen);ensurePolished();show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    verifyWorkflow(model,output);
}
void MainWindow::modelUiStep(){
    if(workflowFinished_)return;
    try{
        auto action=[this](const QString& operation)->QAction*{
            QList<QAction*> matches;QJsonArray observed;for(auto* a:findChildren<QAction*>())if(a->property("operation")==operation){matches.append(a);observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});}
            workflowArtifacts_["action_"+operation]=observed;
            if(matches.size()==1&&matches[0]->isEnabled())return matches[0];
            workflowCases_.append(QJsonObject{{"name","unavailable_"+operation},{"status","not_run"}});finishWorkflow();return nullptr;
        };
        auto* m=session_.model().get();
        const auto workspace=[this]{QJsonArray result;for(auto* frame:viewFrames_){const auto r=frame->geometry();result.append(QJsonArray{r.x(),r.y(),r.width(),r.height()});}return result;};
        switch(workflowStage_++){
        case 0:{
            editingOriginal_=session_.model();captureWorkflow("original");if(auto* a=action("layer_properties"))a->trigger();else return;
            if(!layerDialog_||!layers_){workflowCases_.append(QJsonObject{{"name","layer_dialog_missing"},{"status","not_run"}});finishWorkflow();return;}
            QApplication::processEvents();layerDialog_->grab().save(workflowOutput_+"/layer-dialog.png");
            const auto id=m->layers.front().id;layers_->setSelectedLayerId(id);
            auto* alpha=qobject_cast<QSpinBox*>(layers_->fieldEditor("alpha_blend"));auto* thickness=qobject_cast<QDoubleSpinBox*>(layers_->fieldEditor("thickness"));auto* density=qobject_cast<QDoubleSpinBox*>(layers_->fieldEditor("material_density"));
            if(!alpha||!thickness||!density){workflowCases_.append(QJsonObject{{"name","layer_fields_unavailable"},{"status","not_run"}});finishWorkflow();return;}
            auto revision=m->revision;auto* control=qobject_cast<QCheckBox*>(layers_->fieldEditor("control_net_visible"));
            if(control){control->setChecked(false);workflowCase("layer-control-net-is-display-only",m->revision==revision&&hiddenControlLayers_.contains(id));control->setChecked(true);}
            alpha->setValue(25);thickness->setValue(10);density->setValue(2);workflowArtifacts_["measurements"]=layers_->property("layer_measurements").toJsonObject();
            layerDialog_->grab().save(workflowOutput_+"/layer-draft.png");QList<QPushButton*> apply;for(auto* b:layers_->findChildren<QPushButton*>())if(b->property("layer_action")=="apply"&&b->isEnabled())apply.append(b);
            if(apply.size()!=1){workflowCases_.append(QJsonObject{{"name","layer_apply_unavailable"},{"status","not_run"}});finishWorkflow();return;}apply[0]->click();return;
        }
        case 1:
            workflowCase("alpha-applied-by-production-control",m->layers.front().alphaBlend==191);captureWorkflow("alpha");
            session_.manageLayers("create",{{"changes",QJsonObject{{"name","UI fixture second layer"}}}});return;
        case 2:
            layers_->setSelectedLayerId(editingOriginal_->layers.front().id);if(auto* a=action("layer_down"))a->trigger();return;
        case 3:
            workflowCase("reorder-preserves-selected-stable-layer",m->layers.back().id==layers_->selectedLayerId());captureWorkflow("reordered");session_.undo();return;
        case 4:captureWorkflow("undo-reorder");session_.redo();return;
        case 5:captureWorkflow("redo-reorder");session_.save(workflowOutput_+"/layers.fbm");return;
        case 6:session_.load(workflowOutput_+"/layers.fbm");return;
        case 7:{
            captureWorkflow("reopened");layerDialog_->close();
            if(auto* a=action("select_edge"))a->trigger();else return;
            if(auto* a=action("place_point"))a->trigger();else return;
            workflowCase("point-placement-switches-from-edge-selection",selectionKind_==HullEntity::Point&&placePoint_->isChecked());
            if(auto* a=action("select_point"))workflowCase("selection-menu-matches-point-placement",a->isChecked());else return;
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);workflowArtifacts_["point_workspace_before"]=workspace();
            HullView* body=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Body)body=v;if(!body)throw std::runtime_error("Missing body view");
            workflowOriginalPoint_={m->maximum.x,.75,1.25};workflowPoint_=m->points.size();auto pixel=body->project(workflowOriginalPoint_);
            QMouseEvent press(QEvent::MouseButtonPress,pixel,pixel,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(body,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,pixel,pixel,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(body,&release);return;
        }
        case 8:{
            workflowCase("view-click-adds-one-control-point",m->points.size()==qsizetype(workflowPoint_+1));
            workflowCase("placed-point-selected-in-all-views",selection_.size()==1&&std::all_of(views_.begin(),views_.end(),[this](auto* v){return v->selectedPoints()==selection_;}));
            workflowCase("placement-remains-active-after-click",placePoint_->isChecked()&&std::all_of(views_.begin(),views_.end(),[](auto* v){return v->pointPlacement()==(v->projection()!=HullProjection::Perspective);}));
            if(selection_.size()==1){const auto* p=m->point(*selection_.begin());workflowCase("body-click-uses-model-max-x",p&&std::abs(p->position.x-workflowOriginalPoint_.x)<1e-6&&std::abs(p->position.y-.75)<1e-5&&std::abs(p->position.z-1.25)<1e-5);}
            workflowCase("creation-does-not-open-full-sidebar",inspector_->isHidden());
            workflowCase("creation-keeps-two-splitter-panes",qobject_cast<QSplitter*>(centralWidget())->count()==2);
            workflowCase("point-popup-preserves-four-view-layout",workspace()==workflowArtifacts_["point_workspace_before"].toArray());
            workflowCase("point-popup-represents-new-selected-point",pointPopup_&&pointPopup_->isVisible()&&selection_.contains(pointPopup_->pointId()));
            if(!pointPopup_||!pointPopup_->isVisible())throw std::runtime_error("Point popup was not presented");
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            workflowCase("point-popup-avoids-created-point",pointPopup_->property("avoids_anchor").toBool());
            bool inputsReachable=true;
            for(auto* field:pointPopup_->findChildren<QDoubleSpinBox*>()){
                auto* hit=pointPopup_->parentWidget()->childAt(field->mapTo(pointPopup_->parentWidget(),field->rect().center()));
                inputsReachable&=hit&&(hit==field||field->isAncestorOf(hit));
            }
            workflowCase("point-popup-inputs-above-view-dividers",inputsReachable);
            workflowArtifacts_["placed_point_id"]=double(pointPopup_->pointId());
            grab().save(workflowOutput_+"/point-popup.png");captureWorkflow("point");
            for(auto* field:pointPopup_->findChildren<QDoubleSpinBox*>())if(field->property("point_popup_axis")==1)field->setValue(1.5);
            for(auto* button:pointPopup_->findChildren<QPushButton*>())if(button->property("point_popup_action")=="confirm"){workflowCase("point-popup-confirm-enabled",button->isEnabled());button->click();return;}
            throw std::runtime_error("Point popup confirmation unavailable");
        }
        case 9:{
            const auto id=fs::EntityId(workflowArtifacts_["placed_point_id"].toDouble());const auto* point=m->point(id);
            workflowCase("popup-confirm-updates-the-new-point",point&&std::abs(point->position.y-1.5)<1e-6);
            workflowCase("popup-confirm-keeps-selection-and-placement",selection_.contains(id)&&placePoint_->isChecked()&&inspector_->isHidden()&&!pointPopup_->isVisible());
            workflowCase("popup-confirm-preserves-workspace",workspace()==workflowArtifacts_["point_workspace_before"].toArray());
            captureWorkflow("edited-point");
            auto* body=views_[1];const auto pixel=body->project({m->maximum.x,2,2});
            QMouseEvent press(QEvent::MouseButtonPress,pixel,pixel,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(body,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,pixel,pixel,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(body,&release);return;
        }
        case 10:{
            workflowCase("continuous-placement-adds-a-second-point",m->points.size()==qsizetype(workflowPoint_+2)&&pointPopup_->isVisible()&&selection_.contains(pointPopup_->pointId()));
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(pointPopup_,&escape);
            workflowCase("popup-escape-ends-placement",!pointPopup_->isVisible()&&!placePoint_->isChecked()&&std::none_of(views_.begin(),views_.end(),[](auto* view){return view->pointPlacement();}));
            session_.undo();return;
        }
        case 11:session_.undo();return; // Undo the first point's coordinate edit.
        case 12:session_.undo();return; // Undo creation of the first point.
        case 13:captureWorkflow("undo-point");session_.redo();return;
        case 14:{
            captureWorkflow("redo-point");auto* view=views_[0];view->setAnimationsEnabled(false);
            QContextMenuEvent menuEvent(QContextMenuEvent::Mouse,{50,50},{50,50});QApplication::sendEvent(view,&menuEvent);
            QJsonArray menuItems;QList<QAction*> saves;for(auto* a:view->findChildren<QAction*>())if(a->property("view_operation").isValid()){menuItems.append(QJsonObject{{"operation",a->property("view_operation").toString()},{"text",a->text()},{"enabled",a->isEnabled()}});if(a->property("view_operation")=="save_image")saves.append(a);}
            workflowArtifacts_["view_menu"]=menuItems;for(auto* menu:view->findChildren<QMenu*>())if(menu->isVisible())menu->grab().save(workflowOutput_+"/view-menu.png");
            if(saves.size()!=1){workflowCases_.append(QJsonObject{{"name","view_export_unavailable"},{"status","not_run"}});finishWorkflow();return;}
            const auto before=view->image();const auto path=workflowOutput_+"/current-view.png";
            // Modal view actions are deliberately queued outside QMenu's
            // release handler. Observe completion after that callback returns.
            saves[0]->trigger();
            QTimer::singleShot(0,this,[this,path,before,view]{
                auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                if(!dialog||dialog->property("view_operation")!="save_image_file"){finishWorkflow("Missing safe view file dialog");return;}
                dialog->selectFile(path);QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);
                QTimer::singleShot(50,this,[this,path,before,view]{
                    workflowCase("context-export-is-exact-current-view",QImage(path)==before);
                    for(auto* menu:view->findChildren<QMenu*>())menu->hide();
                    workflowCases_.append(QJsonObject{{"name","desktop-input-native-dialog-print-visual-acceptance"},{"status","not_run"}});finishWorkflow();
                });
            });return;
        }
        }
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
