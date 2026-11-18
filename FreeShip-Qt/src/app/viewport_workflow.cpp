// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production menus, modeless light controls and current-view export.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <stdexcept>

void MainWindow::verifyViewport(const QString& path,const QString& output){viewportWorkflow_=true;QApplication::setQuitOnLastWindowClosed(false);verifyWorkflow(path,output);}
void MainWindow::viewportStep(){
    if(workflowFinished_)return;
    try{
        if(workflowStage_++){
            workflowArtifacts_["saved_file"]=workflowOutput_+"/unchanged.fbm";
            workflowCases_.append(QJsonObject{{"name","desktop-native-dialog-print-acceptance"},{"status","not_run"}});finishWorkflow();return;
        }
        for(auto* v:views_){v->setAttribute(Qt::WA_DontShowOnScreen);v->setAnimationsEnabled(false);}
        if(!captureStudioMenus(true)){finishWorkflow("Cannot write menu observations");return;}
        captureWorkflow("before");const auto model=session_.model();const auto modified=session_.modified(),undo=session_.canUndo(),redo=session_.canRedo();
        HullView* view=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Perspective)view=v;
        if(!view){workflowCases_.append(QJsonObject{{"name","perspective-view-unavailable"},{"status","not_run"}});finishWorkflow();return;}
        auto missing=[&](const QString& scope){workflowCases_.append(QJsonObject{{"name",scope},{"status","not_run"},{"message","Missing, ambiguous or disabled production entry."}});finishWorkflow();};
        auto menu=[&]()->QMenu*{
            for(auto* old:view->findChildren<QMenu*>())old->hide();QContextMenuEvent event(QContextMenuEvent::Mouse,{100,100},{100,100});QApplication::sendEvent(view,&event);QApplication::processEvents();
            QList<QMenu*> found;for(auto* m:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(m->isVisible())found.append(m);
            if(found.size()!=1){missing("context-menu-unavailable");return nullptr;}return found[0];
        };
        auto action=[&](QMenu* root,const char* property,const QVariant& value)->QAction*{
            if(!root)return nullptr;QList<QAction*> found;QJsonArray observed;
            for(auto* a:root->findChildren<QAction*>())if(a->property(property)==value){found.append(a);observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});}
            workflowArtifacts_[QString(property)+':'+value.toString()]=observed;
            if(found.size()!=1||!found[0]->isEnabled()){missing("action-unavailable:"+value.toString());return nullptr;}return found[0];
        };
        auto* root=menu();auto* axis=action(root,"coordinate_axis",2);if(!axis)return;axis->trigger();root->hide();
        const auto axisIsLocal=[&](int selected){
            for(auto* candidate:views_)if(candidate->coordinateAxis()!=(candidate==view?selected:0))return false;
            return true;
        };
        workflowCase("coordinate-menu-affects-only-invoked-view",axisIsLocal(2));
        root=menu();auto* off=action(root,"coordinate_axis",0);if(!off)return;off->trigger();root->hide();
        workflowCase("coordinate-menu-disables-only-invoked-view",axisIsLocal(0));
        if(!model->points.empty()){
            selectPoint(model->points.front().id);
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(view,&escape);
            workflowCase("escape-clears-selection",selection_.isEmpty());
        }
        // Production picking receives local mouse events on this private view.
        // Check both point and non-point paths; Shift no longer means additive.
        HullView* picking=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Body)picking=v;
        if(picking){
            picking->setControlNetVisible(true);picking->setFillVisible(false);picking->finishPresentation();picking->fit();
            const auto click=[&](QPointF p,Qt::KeyboardModifiers mods){
                QMouseEvent press(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,mods);QApplication::sendEvent(picking,&press);
                QMouseEvent release(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,mods);QApplication::sendEvent(picking,&release);
                QKeyEvent up(QEvent::KeyRelease,mods.testFlag(Qt::ShiftModifier)?Qt::Key_Shift:Qt::Key_Control,Qt::NoModifier);QApplication::sendEvent(picking,&up);
            };
            for(auto kind:{HullEntity::Point,HullEntity::Edge}){
                setSelectionKind(kind);QList<QPair<fs::EntityId,QPointF>> hits;
                const auto collect=[&](fs::Vec3 p){
                    const auto pixel=picking->project(p);if(!picking->rect().adjusted(8,8,-8,-8).contains(pixel.toPoint()))return;
                    const auto id=picking->entityAt(pixel);if(!id)return;
                    for(const auto& hit:hits)if(hit.first==id||QLineF(hit.second,pixel).length()<24)return;
                    hits.append({id,pixel});
                };
                if(kind==HullEntity::Point){for(const auto& point:model->points){collect(point.position);if(hits.size()==2)break;}}
                else for(const auto& edge:model->edges){const auto* a=model->point(edge.a);const auto* b=model->point(edge.b);if(a&&b)collect({(a->position.x+b->position.x)/2,(a->position.y+b->position.y)/2,(a->position.z+b->position.z)/2});if(hits.size()==2)break;}
                const QString prefix=kind==HullEntity::Point?"point":"edge";
                if(hits.size()!=2){workflowCases_.append(QJsonObject{{"name",prefix+"-modifier-picking"},{"status","not_run"},{"message","Fixture has no two distinct pickable targets"}});continue;}
                const auto selected=[&]{return kind==HullEntity::Point?selection_:entitySelection_;};
                const auto selectFirst=[&]{if(kind==HullEntity::Point)selectPoints({hits[0].first});else selectEntities(kind,{hits[0].first});};
                selectFirst();click(hits[1].second,Qt::ControlModifier);
                workflowCase(prefix+"-ctrl-click-adds",selected()==QSet<fs::EntityId>{hits[0].first,hits[1].first});
                click(hits[1].second,Qt::ControlModifier);
                workflowCase(prefix+"-ctrl-click-removes",selected()==QSet<fs::EntityId>{hits[0].first});
                selectFirst();click(hits[1].second,Qt::ShiftModifier);
                workflowCase(prefix+"-shift-click-is-not-additive",selected()==QSet<fs::EntityId>{hits[1].first});
                click(hits[1].second,Qt::ShiftModifier);
                workflowCase(prefix+"-shift-click-does-not-toggle",selected()==QSet<fs::EntityId>{hits[1].first});
            }
            setSelectionKind(HullEntity::Point);selectPoints({});
        }
        root=menu();auto* fill=action(root,"view_operation","fill");if(!fill)return;fill->trigger();root->hide();
        QList<QImage> otherImages;for(auto* other:views_)if(other!=view)otherImages.append(other->image());
        const auto lightBefore=view->light();root=menu();auto* wide=action(root,"focal_mm",28);if(!wide)return;wide->trigger();root->hide();
        workflowCase("wide-menu-selects-28-not-legacy-20",view->focalLength()==28);
        for(int focal:{50,90,130,200}){root=menu();auto* a=action(root,"focal_mm",focal);if(!a)return;a->trigger();root->hide();workflowCase("lens-action-"+QString::number(focal),view->focalLength()==focal);}
        view->image().save(workflowOutput_+"/before-light.png");
        root=menu();auto* open=action(root,"view_operation","light_settings");if(!open)return;open->trigger();root->hide();QApplication::processEvents();
        QList<QDialog*> candidates;for(auto* dialog:findChildren<QDialog*>())if(dialog->property("view_operation")=="light_dialog"&&dialog->property("light_projection").toInt()==int(HullProjection::Perspective))candidates.append(dialog);
        if(candidates.size()!=1){missing("light-dialog-unavailable");return;}auto* dialog=candidates[0];
        auto field=[&](const QString& key)->QWidget*{QList<QWidget*> found;for(auto* w:dialog->findChildren<QWidget*>())if(w->property("light_field")==key)found.append(w);if(found.size()!=1){missing("light-field-unavailable:"+key);return nullptr;}return found[0];};
        auto* enabled=qobject_cast<QCheckBox*>(field("enabled"));QHash<QString,QSpinBox*> fields;
        for(const auto& key:QStringList{"x","y","z","ambient","intensity"}){auto* number=qobject_cast<QSpinBox*>(field(key));if(!number){if(!workflowFinished_)missing("light-number-unavailable:"+key);return;}fields[key]=number;}
        if(!enabled){if(!workflowFinished_)missing("light-checkbox-unavailable");return;}
        workflowCase("opening-light-does-not-change-view",view->light()==lightBefore);
        workflowCase("light-fields-show-actual-values",fields["x"]->value()==lightBefore.direction.x&&fields["y"]->value()==lightBefore.direction.y&&fields["z"]->value()==lightBefore.direction.z&&fields["ambient"]->value()==lightBefore.ambient&&fields["intensity"]->value()==lightBefore.intensity&&enabled->isChecked()==lightBefore.enabled);
        dialog->grab().save(workflowOutput_+"/light-dialog.png");const auto imageBefore=view->image();
        enabled->setChecked(true);fields["x"]->setValue(0);fields["y"]->setValue(0);fields["z"]->setValue(0);fields["ambient"]->setValue(51);fields["intensity"]->setValue(153);
        auto current=view->light();workflowCase("light-fields-update-current-view",current.enabled&&current.direction.x==0&&current.direction.y==0&&current.direction.z==0&&current.ambient==51&&current.intensity==153);
        workflowCase("light-changes-rendered-image",view->image()!=imageBefore);view->image().save(workflowOutput_+"/after-light.png");dialog->grab().save(workflowOutput_+"/light-edited.png");
        const auto lit=view->image();enabled->setChecked(false);
        workflowCase("light-dialog-checkbox-disables-light",!view->light().enabled&&view->image()!=lit);enabled->setChecked(true);
        workflowCase("light-title-close-available",bool(dialog->windowFlags()&Qt::WindowCloseButtonHint));
        dialog->close();workflowCase("closing-keeps-live-light",!dialog->isVisible()&&view->light().ambient==51&&view->light().enabled);
        root=menu();open=action(root,"view_operation","light_settings");if(!open)return;open->trigger();root->hide();QApplication::processEvents();workflowCase("reopening-restores-view-values",dialog->isVisible()&&fields["ambient"]->value()==51&&fields["x"]->value()==0);dialog->close();
        int index=0;bool local=true;for(auto* other:views_)if(other!=view)local&=other->image()==otherImages[index++];workflowCase("camera-and-light-are-view-local",local);
        root=menu();auto* save=action(root,"view_operation","save_image");if(!save)return;
        root->grab().save(workflowOutput_+"/view-menu.png");const auto path=workflowOutput_+"/export.png";const auto expected=view->image();
        bool handled=false;QTimer picker;picker.setInterval(10);QElapsedTimer deadline;deadline.start();
        connect(&picker,&QTimer::timeout,this,[&]{
            auto* file=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            if(!file||file->property("view_operation")!="save_image_file")return;
            picker.stop();handled=true;file->selectFile(path);QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);
        });picker.start();save->trigger();root->hide();
        while(!handled&&deadline.elapsed()<10000)QApplication::processEvents(QEventLoop::AllEvents,20);
        if(!handled){finishWorkflow("Missing safe viewport file dialog");return;}
        workflowCase("image-export-includes-selected-lens-and-light",QImage(path)==expected);
        workflowCase("view-settings-do-not-edit-document",session_.model()==model&&session_.modified()==modified&&session_.canUndo()==undo&&session_.canRedo()==redo);
        workflowArtifacts_["view_settings"]=view->viewSettingsEvidence();captureWorkflow("after");session_.save(workflowOutput_+"/unchanged.fbm");
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
