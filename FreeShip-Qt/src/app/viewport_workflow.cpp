// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production menus, modeless light controls and current-view export.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFileDialog>
#include <QMenu>
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
        auto* root=menu();auto* fill=action(root,"view_operation","fill");if(!fill)return;fill->trigger();root->hide();
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
        root=menu();auto* toggle=action(root,"view_operation","lighting_enabled");if(!toggle)return;toggle->trigger();root->hide();workflowCase("context-toggle-syncs-open-dialog",!view->light().enabled&&!enabled->isChecked());enabled->setChecked(true);
        QList<QPushButton*> close;for(auto* button:dialog->findChildren<QPushButton*>())if(button->property("light_action")=="close")close.append(button);
        if(close.size()!=1){missing("light-close-unavailable");return;}close[0]->click();workflowCase("closing-keeps-live-light",view->light().ambient==51&&view->light().enabled);
        root=menu();open=action(root,"view_operation","light_settings");if(!open)return;open->trigger();root->hide();workflowCase("reopening-restores-view-values",dialog->isVisible()&&fields["ambient"]->value()==51&&fields["x"]->value()==0);close[0]->click();
        int index=0;bool local=true;for(auto* other:views_)if(other!=view)local&=other->image()==otherImages[index++];workflowCase("camera-and-light-are-view-local",local);
        root=menu();auto* save=action(root,"view_operation","save_image");if(!save)return;
        root->grab().save(workflowOutput_+"/view-menu.png");const auto path=workflowOutput_+"/export.png";const auto expected=view->image();
        QTimer::singleShot(0,this,[this,path]{auto* file=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!file||file->property("view_operation")!="save_image_file"){finishWorkflow("Missing safe viewport file dialog");return;}file->selectFile(path);QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);});save->trigger();root->hide();
        workflowCase("image-export-includes-selected-lens-and-light",QImage(path)==expected);
        workflowCase("view-settings-do-not-edit-document",session_.model()==model&&session_.modified()==modified&&session_.canUndo()==undo&&session_.canRedo()==redo);
        workflowArtifacts_["view_settings"]=view->viewSettingsEvidence();captureWorkflow("after");session_.save(workflowOutput_+"/unchanged.fbm");
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
