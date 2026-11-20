// SPDX-License-Identifier: GPL-2.0-or-later
// Private production-action probe. No desktop control or user settings writes.
#include "main_window.h"
#include "layer_panel.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "render/curve_edit_motion.h"
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include "point_coordinate_popup.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QKeyEvent>
#include <QImage>
#include <QLineF>
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

namespace {
QJsonObject popupRowEvidence(QAbstractItemView* view,const QModelIndex& index){
    const auto image=view->viewport()->grab(view->visualRect(index)).toImage();
    if(image.width()<16||image.height()<6)return {};
    const auto background=image.pixelColor(image.width()*3/4,image.height()/2);
    const auto luminance=[](const QColor& color){
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return .2126*channel(color.redF())+.7152*channel(color.greenF())+.0722*channel(color.blueF());
    };
    const double base=luminance(background);
    int legiblePixels=0;
    QColor foreground=background;
    double best=1;
    for(int y=2;y<image.height()-2;++y)for(int x=3;x<image.width()/2;++x){
        const auto color=image.pixelColor(x,y);
        const double ink=luminance(color);
        const double contrast=(std::max(base,ink)+.05)/(std::min(base,ink)+.05);
        if(contrast>=4.5)++legiblePixels;
        if(contrast>best){best=contrast;foreground=color;}
    }
    return {{"background",background.name()},{"foreground",foreground.name()},
            {"contrast",best},{"legible_pixels",legiblePixels}};
}
}

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
        const auto dragPixel=[](HullView* view,QPointF at){
            const auto img=view->grab().toImage();
            const auto pixel=(at*img.devicePixelRatio()).toPoint();
            return img.rect().contains(pixel)?img.pixelColor(pixel).name():QString{};
        };
        const auto workspace=[this]{QJsonArray result;for(auto* frame:viewFrames_){const auto r=frame->geometry();result.append(QJsonArray{r.x(),r.y(),r.width(),r.height()});}return result;};
        auto layerButton=[this](const char* action,fs::EntityId id){
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            for(int row=0;row<list->count();++row){
                auto* item=list->item(row);
                if(item->data(Qt::UserRole).toULongLong()!=id)continue;
                if(auto* wrapper=list->itemWidget(item))for(auto* button:wrapper->findChildren<QToolButton*>())
                    if(button->property("layer_action")==QLatin1String(action))return button;
            }
            throw std::runtime_error("Layer row control not found");
        };
        auto eyePaint=[this](QToolButton* eye,bool gray){
            auto* viewport=layers_->findChild<QListWidget*>("layer_list")->viewport();
            const QRect area(eye->mapTo(viewport,QPoint()),eye->size());
            const auto painted=viewport->grab(area).toImage();
            eye->hide();const auto underneath=viewport->grab(area).toImage();eye->show();
            if(painted.isNull()||painted.size()!=underneath.size())return false;
            const auto glyph=QStyle::alignedRect(eye->layoutDirection(),Qt::AlignCenter,eye->iconSize(),eye->rect());
            int strokes=0,grayPixels=0;
            for(int y=0;y<painted.height();++y)for(int x=0;x<painted.width();++x){
                const auto color=painted.pixelColor(x,y);
                if(color==underneath.pixelColor(x,y))continue;
                if(!glyph.contains(QPointF(x/painted.devicePixelRatio(),y/painted.devicePixelRatio()).toPoint()))return false;
                ++strokes;
                if(qAbs(color.red()-color.green())<=1&&qAbs(color.green()-color.blue())<=1&&color.value()<160)++grayPixels;
            }
            return strokes>0&&(!gray||grayPixels>0);
        };
        const auto key=[](QWidget* target,Qt::Key code,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
            QKeyEvent press(QEvent::KeyPress,code,modifiers),release(QEvent::KeyRelease,code,modifiers);
            QApplication::sendEvent(target,&press);QApplication::sendEvent(target,&release);
        };
        auto layerHistory=[&](bool redo){
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            QApplication::setActiveWindow(layerDialog_);list->setFocus();
            key(list,Qt::Key_Z,redo?Qt::ControlModifier|Qt::ShiftModifier:Qt::ControlModifier);
            if(!session_.busy())throw std::runtime_error("Layer window history shortcut did not start a revision");
        };
        const auto stage=workflowStage_++;
        if(stage>=100&&stage<=132){
            auto observed=workflowArtifacts_["history_toolbar"].toObject();
            const auto layerId=fs::EntityId(observed["redo_first_layer"].toDouble());
            auto* name=layers_->fieldEditor("alpha_blend");
            if(stage>100){
                const bool redone=(stage-100)%2==0;
                if(!session_.busy()&&layerProperties_->isEnabled()&&name->isEnabled()&&
                   (m->layers.front().id==layerId)==redone)
                    observed["completed"]=observed["completed"].toInt()+1;
            }
            if(stage==132){
                workflowCase("history-shortcuts-apply-32-revisions-and-restore-layer-editing",observed["completed"].toInt()==32);
                workflowCase("layer-properties-opens-readonly-during-history",observed["read_only_opens"].toInt()==32);
                workflowStage_=6;workflowArtifacts_["history_toolbar"]=observed;
                session_.save(workflowOutput_+"/layers.fbm");return;
            }
            auto* view=views_[0];QApplication::setActiveWindow(this);view->setFocus();
            const auto modifiers=(stage%2)?Qt::ControlModifier|Qt::ShiftModifier:Qt::ControlModifier;
            QKeyEvent press(QEvent::KeyPress,Qt::Key_Z,modifiers),release(QEvent::KeyRelease,Qt::Key_Z,modifiers);
            QApplication::sendEvent(view,&press);QApplication::sendEvent(view,&release);
            for(const auto key:{Qt::Key_Shift,Qt::Key_Control}){
                QKeyEvent up(QEvent::KeyRelease,key,Qt::NoModifier);QApplication::sendEvent(view,&up);
            }
            if(!session_.busy())throw std::runtime_error("History shortcut did not start a revision");
            if(layerProperties_->isEnabled())layerProperties_->trigger();
            if(layerDialog_->isVisible()&&!name->isEnabled())observed["read_only_opens"]=observed["read_only_opens"].toInt()+1;
            layerDialog_->close();workflowArtifacts_["history_toolbar"]=observed;return;
        }
        switch(stage){
        case 0:{
            QTimer::singleShot(0,this,[this]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog)return;
                QComboBox* units=nullptr;
                for(auto* combo:dialog->findChildren<QComboBox*>())
                    if(combo->property("field")=="units"){units=combo;break;}
                if(!units){workflowCase("unit-popup-hover-readable",false);dialog->reject();return;}
                units->showPopup();
                auto* view=units->view();
                QJsonArray hover;
                bool readable=view->isVisible()&&units->count()==2;
                for(int row:{1,0}){
                    const auto index=units->model()->index(row,units->modelColumn(),units->rootModelIndex());
                    const auto at=view->visualRect(index).center();
                    QMouseEvent move(QEvent::MouseMove,at,view->viewport()->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(view->viewport(),&move);
                    const bool highlighted=view->currentIndex()==index;
                    auto evidence=popupRowEvidence(view,index);
                    evidence["item"]=units->itemText(row);
                    evidence["highlighted"]=highlighted;
                    hover.append(evidence);
                    readable&=highlighted&&evidence["legible_pixels"].toInt()>4;
                }
                workflowArtifacts_["unit_popup_hover"]=hover;
                view->window()->grab().save(workflowOutput_+"/views/unit-popup-hover.png");
                workflowCase("unit-popup-hover-readable",readable);
                units->hidePopup();
                dialog->reject();
            });
            modelingDialog("new");
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
        case 1:{
            workflowCase("alpha-applied-by-production-control",m->layers.front().alphaBlend==191);captureWorkflow("alpha");
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            auto* footer=list->itemWidget(list->item(list->count()-1));
            auto* add=footer->findChild<QToolButton*>();
            if(!add||!add->isEnabled())throw std::runtime_error("Layer add button unavailable");
            workflowCase("plus-follows-last-layer",list->count()==m->layers.size()+1&&add->geometry().left()==0&&add->icon().name()=="studio-add-one");
            if(m->layers.size()==1)workflowCase("last-layer-not-removable",!layerButton("delete",m->layers.front().id)->isEnabled());
            QApplication::setActiveWindow(layerDialog_);add->setFocus();add->click();return;
        }
        case 2:{
            const auto& added=m->layers.back();auto* name=layers_->findChild<QLineEdit*>("layer_inline_name");
            const auto expected=QStringLiteral("图层 ")+QString::number(added.defaultNameNumber.value_or(-1));
            workflowCase("inline-layer-add-selects-default-name",added.defaultNameNumber&&layers_->selectedLayerId()==added.id&&name&&
                name->selectedText()==expected,name?name->selectedText():QString());
            workflowCase("inline-layer-add-focuses-name",name&&layerDialog_->focusWidget()==name,
                layerDialog_->focusWidget()?layerDialog_->focusWidget()->objectName():QString());
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            workflowCase("trash-targets-individual-layer",layerButton("delete",added.id)->isEnabled()&&
                !layerButton("delete",m->layers.front().id)->isEnabled());
            layerDialog_->grab().save(workflowOutput_+"/layer-inline-controls.png");
            const auto originalLanguage=uiLanguage();
            const auto number=QString::number(added.defaultNameNumber.value_or(-1));
            const QPair<const char*,QString> localizedNames[]={
                {"zh",QStringLiteral("图层")},{"en",QStringLiteral("Layer")},{"ru",QStringLiteral("Слой")},
                {"ja",QStringLiteral("レイヤー")},{"de",QStringLiteral("Ebene")},
                {"fr",QStringLiteral("Calque")},{"es",QStringLiteral("Capa")},
            };
            for(const auto& [locale, caption]:localizedNames){
                const bool switched=setUiLanguage(locale).isEmpty()&&uiLanguage()==QLatin1String(locale);
                const auto expectedName=caption+QLatin1Char(' ')+number;
                const auto rowName=list->item(list->count()-2)->text();
                const auto fieldName=name?name->text():QString();
                workflowCase(QStringLiteral("generated-layer-name-follows-language-")+QLatin1String(locale),
                    switched&&added.defaultNameNumber&&rowName==expectedName&&fieldName==expectedName,
                    QStringLiteral("Row: %1; name field: %2").arg(rowName,fieldName));
            }
            if(!setUiLanguage(originalLanguage).isEmpty())throw std::runtime_error("Could not restore probe language");
            editingLayer_=added.id;
            if(!name)throw std::runtime_error("Inline layer editor unavailable");
            name->insert(QStringLiteral("Uncommitted layer name"));
            key(name,Qt::Key_Z,Qt::ControlModifier);
            workflowCase("inline-name-undo-is-local-to-typing",name->text()==expected&&!session_.busy());
            key(name,Qt::Key_Escape);
            workflowCase("inline-name-escape-leaves-dialog-open-and-name-unchanged",layerDialog_->isVisible()&&!session_.busy()&&
                         !layers_->findChild<QLineEdit*>("layer_inline_name")&&list->currentItem()->text()==expected);
            workflowCase("layer-side-panel-has-no-duplicate-controls",!layers_->fieldEditor("name")&&!layers_->fieldEditor("visible"));
            auto* viewport=list->viewport();const auto click=list->visualItemRect(list->currentItem()).topLeft()+QPoint(64,12);
            auto* target=viewport->childAt(click);if(!target)target=viewport;
            for(auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease,QEvent::MouseButtonDblClick}){
                QMouseEvent event(type,target->mapFrom(viewport,click),viewport->mapToGlobal(click),Qt::LeftButton,
                    type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(target,&event);
            }
            name=layers_->findChild<QLineEdit*>("layer_inline_name");
            workflowCase("double-click-edits-name-in-row",name&&name->isVisible()&&name->parentWidget()==viewport&&name->geometry().contains(click));
            if(!name)throw std::runtime_error("Double-click did not open the layer-name editor");
            qobject_cast<QDoubleSpinBox*>(layers_->fieldEditor("thickness"))->setValue(8.5);
            name->setText(QStringLiteral("Edited test layer"));
            workflowStage_=23;key(name,Qt::Key_Return);
            if(!session_.busy())throw std::runtime_error("Inline rename did not start a revision");
            return;
        }
        case 23:{
            workflowCase("enter-renames-only-the-layer",m->layer(editingLayer_)&&m->layer(editingLayer_)->name=="Edited test layer"&&
                         m->layer(editingLayer_)->thickness==0&&layers_->changes()["thickness"].toDouble()==8.5);
            auto* eye=layerButton("visibility",editingLayer_);
            if(!eye||!eye->isEnabled())throw std::runtime_error("Layer visibility button unavailable");
            workflowCase("visible-layer-has-eye-icon",eye->icon().name()=="studio-menu:LayerVisible"&&eye->isChecked());
            workflowCase("visible-eye-paints-only-icon",eyePaint(eye,false));
            eye->setDown(true);workflowCase("pressed-eye-does-not-paint-a-filled-button",eyePaint(eye,false));eye->setDown(false);
            auto* otherEye=layerButton("visibility",m->layers.front().id);
            const bool otherVisible=otherEye->isChecked();
            workflowStage_=24;eye->click();
            workflowCase("other-eye-remains-interactive-during-update",session_.busy()&&otherEye->isEnabled()&&otherEye->isChecked()==otherVisible);
            otherEye->setDisabled(true);workflowCase("disabled-eye-still-paints-only-gray-icon",eyePaint(otherEye,true));otherEye->setEnabled(true);
            otherEye->click();workflowCase("second-layer-eye-responds-before-first-commit",otherEye->isChecked()!=otherVisible&&!eye->isChecked());
            otherEye->click();workflowArtifacts_["eye_burst_expected_revision"]=double(m->revision+3);
            workflowArtifacts_["other_layer_visibility"]=otherVisible;
            layerDialog_->grab().save(workflowOutput_+"/layer-busy-eyes.png");
            return;
        }
        case 24:{
            workflowCase("rapid-eye-changes-all-committed",m->revision==quint64(workflowArtifacts_["eye_burst_expected_revision"].toDouble())&&m->layers.front().visible==workflowArtifacts_["other_layer_visibility"].toBool());
            auto* eye=layerButton("visibility",editingLayer_);
            workflowCase("eye-click-hides-layer-and-preserves-draft",eye&&!eye->isChecked()&&eye->icon().name()=="studio-menu:LayerHidden"&&
                         !m->layer(editingLayer_)->visible&&m->layer(editingLayer_)->thickness==0&&layers_->changes()["thickness"].toDouble()==8.5);
            if(!eye||!eye->isEnabled())throw std::runtime_error("Hidden layer visibility button unavailable");
            workflowCase("hidden-eye-paints-only-icon",eyePaint(eye,false));
            layerDialog_->grab().save(workflowOutput_+"/layer-hidden-eye.png");
            workflowStage_=25;eye->click();return;
        }
        case 25:{
            auto* eye=layerButton("visibility",editingLayer_);
            workflowCase("second-eye-click-shows-layer",eye&&eye->isChecked()&&eye->icon().name()=="studio-menu:LayerVisible"&&m->layer(editingLayer_)->visible);
            auto* trash=layerButton("delete",editingLayer_);
            if(!trash||!trash->isEnabled())throw std::runtime_error("Layer trash button unavailable");
            workflowArtifacts_["before_layer_delete_revision"]=double(m->revision);workflowStage_=20;
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            auto listed=[&]{for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toULongLong()==editingLayer_)return true;return false;};
            QElapsedTimer timer;timer.start();trash->click();
            workflowArtifacts_["layer_delete_feedback_ms"]=timer.nsecsElapsed()/1e6;
            workflowCase("direct-delete-updates-before-publication",session_.busy()&&!listed()&&list->isEnabled());
            timer.restart();layerHistory(false);
            workflowArtifacts_["layer_undo_feedback_ms"]=timer.nsecsElapsed()/1e6;
            workflowCase("immediate-undo-restores-row-before-publication",session_.busy()&&listed()&&list->isEnabled());
            layerHistory(false);layerHistory(true);
            workflowCase("excess-undo-preserves-layer-redo",session_.busy()&&!listed()&&list->isEnabled());
            return;
        }
        case 20:
            workflowCase("queued-delete-undo-redo-commit-count",m->revision==quint64(workflowArtifacts_["before_layer_delete_revision"].toDouble())+3);
            workflowCase("trash-deletes-only-target-layer",!m->layer(editingLayer_)&&m->layers.size()==editingOriginal_->layers.size());
            layerHistory(false);return;
        case 21:
            workflowCase("undo-restores-deleted-layer",m->layer(editingLayer_)&&m->layers.back().id==editingLayer_&&m->layer(editingLayer_)->name=="Edited test layer");
            layerHistory(true);return;
        case 22:
            workflowCase("redo-removes-layer-again-from-layer-window",!m->layer(editingLayer_));
            workflowStage_=26;layerHistory(false);return;
        case 26:{
            workflowStage_=3;
            auto* list=layers_->findChild<QListWidget*>("layer_list");
            auto start=list->visualItemRect(list->item(0)).center();
            auto end=list->visualItemRect(list->item(list->count()-1)).center();
            auto* viewport=list->viewport();
            auto* target=viewport->childAt(start);
            if(!target)target=viewport;
            auto mouse=[target,viewport](QEvent::Type type,QPoint pos,Qt::MouseButton button,Qt::MouseButtons buttons){
                QMouseEvent event(type,target->mapFrom(viewport,pos),viewport->mapToGlobal(pos),button,buttons,Qt::NoModifier);
                QApplication::sendEvent(target,&event);
            };
            mouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            mouse(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
            if(!session_.busy())throw std::runtime_error("Layer drag did not request a reorder");
            return;
        }
        case 3:
            workflowCase("reorder-preserves-selected-stable-layer",m->layers.back().id==layers_->selectedLayerId());captureWorkflow("reordered");session_.undo();return;
        case 4:captureWorkflow("undo-reorder");session_.redo();return;
        case 5:{
            captureWorkflow("redo-reorder");layerDialog_->close();
            workflowArtifacts_["history_toolbar"]=QJsonObject{{"redo_first_layer",double(m->layers.front().id)}};
            workflowStage_=100;modelUiStep();return;
        }
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
            const auto id=fs::EntityId(workflowArtifacts_["placed_point_id"].toDouble());
            if(!m->point(id))throw std::runtime_error("Missing control point for release-frame check");
            view->finishPresentation();
            selectPoints({id});
            const auto start=view->projectControlPoint(id);
            const auto preview=start+QPointF(-25,-18);
            const auto end=start+QPointF(-32,-22);
            const auto ink=dragPixel(view,start);
            const auto originalModel=view->snapshot();
            const auto mouse=[view](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons down){
                QMouseEvent event(type,point,view->mapToGlobal(point),button,down,Qt::NoModifier);
                QApplication::sendEvent(view,&event);
            };
            mouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            mouse(QEvent::MouseMove,preview,Qt::NoButton,Qt::LeftButton);
            workflowCase("point-drag-painted-at-pointer",!ink.isEmpty()&&dragPixel(view,preview)==ink&&dragPixel(view,start)!=ink);
            mouse(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
            const auto atRelease=dragPixel(view,end),oldAtRelease=dragPixel(view,start);
            workflowCase("point-release-keeps-final-pixels-while-model-is-pending",
                session_.busy()&&view->snapshot()==originalModel&&atRelease==ink&&oldAtRelease!=ink);
            workflowArtifacts_["point_release"]=QJsonObject{{"x",end.x()},{"y",end.y()},{"ink",ink}};
            return;
        }
        case 15:{
            auto* view=views_[0];
            const auto id=fs::EntityId(workflowArtifacts_["placed_point_id"].toDouble());
            const auto release=workflowArtifacts_["point_release"].toObject();
            const QPointF end(release["x"].toDouble(),release["y"].toDouble());
            workflowCase("point-commit-matches-release-frame",
                QLineF(view->projectControlPoint(id),end).length()<.05&&dragPixel(view,end)==release["ink"].toString());
            session_.undo();return;
        }
        case 16:{
            auto* view=views_[0];
            const auto id=fs::EntityId(workflowArtifacts_["placed_point_id"].toDouble());
            const auto release=workflowArtifacts_["point_release"].toObject();
            const QPointF end(release["x"].toDouble(),release["y"].toDouble());
            selectPoints({id});
            const auto ink=release["ink"].toString();
            workflowCase("point-undo-clears-release-preview",
                m->point(id)&&QLineF(view->projectControlPoint(id),end).length()>20&&
                dragPixel(view,view->projectControlPoint(id))==ink&&dragPixel(view,end)!=ink);
            auto* perspective=views_[3];
            const auto orbit=[perspective](QPointF delta){
                const QPointF start=perspective->rect().center();
                const auto send=[perspective](QEvent::Type type,QPointF at,Qt::MouseButton button,Qt::MouseButtons down){
                    QMouseEvent event(type,at,perspective->mapToGlobal(at),button,down,Qt::ControlModifier);
                    QApplication::sendEvent(perspective,&event);
                };
                send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                send(QEvent::MouseMove,start+delta,Qt::NoButton,Qt::LeftButton);
                send(QEvent::MouseButtonRelease,start+delta,Qt::LeftButton,Qt::NoButton);
                return perspective->interactionMotionEvidence();
            };
            const auto initial=perspective->interactionMotionEvidence();
            const auto right=orbit({24,0}),down=orbit({0,20});
            workflowCase("perspective-ctrl-drag-reverses-horizontal-orbit",
                right["camera_yaw"].toDouble()<initial["camera_yaw"].toDouble()&&right["camera_pitch"]==initial["camera_pitch"]);
            workflowCase("perspective-ctrl-drag-down-increases-pitch",
                down["camera_pitch"].toDouble()>right["camera_pitch"].toDouble()&&down["camera_yaw"]==right["camera_yaw"]);
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
                    workflowStage_=200;modelUiStep();
                });
            });return;
        }
        case 200:{
            using namespace fs::curveEdit;
            fs::ModelSnapshot before,after;
            fs::Section oldSection;
            oldSection.kind=3;oldSection.index=2;oldSection.paths={{{0,0,1},{5,1,1},{10,0,1}}};
            before.sections={oldSection};
            auto split=oldSection;split.paths={{{0,0,1},{4,1,1}},{{6,1,1},{10,0,1}}};after.sections={split};
            auto motion=Motion::between(before,after,{},{});
            if(!motion)throw std::runtime_error("Missing section split transition");
            const Key key{3,2};
            const auto splitAt=motion->frameAt(Motion::DurationMs/2).at(key);
            workflowCase("curve-motion-split-fades-without-linking-branches",
                splitAt.size()==3&&splitAt[0].path.size()==3&&splitAt[0].opacity<1&&
                splitAt[1].path.back().x==4&&splitAt[2].path.front().x==6&&
                splitAt[1].opacity>0&&splitAt[1].opacity<1);
            auto shifted=oldSection;
            shifted.paths={{{10,1,1},{7,2,1},{2,2,1},{0,1,1}}};
            after.sections={shifted};
            motion=Motion::between(before,after,{},{});
            if(!motion)throw std::runtime_error("Missing resampled section transition");
            const auto intermediate=motion->frameAt(Motion::DurationMs/2).at(key);
            const auto end=motion->frameAt(Motion::DurationMs).at(key);
            workflowCase("curve-motion-matches-resampled-path-and-direction",
                intermediate.size()==1&&intermediate.front().opacity==1&&
                intermediate.front().path.front().x==10&&intermediate.front().path.front().y>0&&
                intermediate.front().path.front().y<1&&end.front().path.front().y==1&&
                end.front().path.back().x==0&&end.front().path.back().y==1);

            QSet<fs::EntityId> occupied;
            for(const auto& curve:m->controlCurves)for(const auto id:curve.controls)occupied.insert(id);
            for(const auto& edge:m->edges){
                const auto* point=m->point(edge.a);
                if(!point||point->locked||!point->batchMovable||occupied.contains(edge.a)||occupied.contains(edge.b)||
                    point->position.y<.2)continue;
                workflowArtifacts_["curve_motion_point"]=double(point->id);
                if(session_.editAuxiliary("create_control",{{"ids",QJsonArray{double(edge.id)}}}))return;
                break;
            }
            throw std::runtime_error("Cannot create control curve for edit motion workflow");
        }
        case 201:{
            const double height=m->minimum.z+(m->maximum.z-m->minimum.z)*.37;
            if(!session_.editSections("add",3,{{"positions",QJsonArray{height}}}))
                throw std::runtime_error("Cannot add waterline for edit motion workflow");
            return;
        }
        case 202:{
            using namespace fs::curveEdit;
            const auto id=fs::EntityId(workflowArtifacts_["curve_motion_point"].toDouble());
            const auto* point=m->point(id);
            if(!point)throw std::runtime_error("Missing motion control point");
            const auto curve=std::find_if(m->controlCurves.begin(),m->controlCurves.end(),
                [id](const auto& curve){return curve.controls.contains(id);});
            if(curve==m->controlCurves.end())throw std::runtime_error("Missing motion control curve");
            workflowArtifacts_["curve_motion_id"]=double(curve->id);
            const auto original=point->position;
            auto target=original;target.y+=.5;
            const auto base=fs::presentation::splineCubics(curve->spline);
            const auto moved=preview(*curve,*m,PointPositions{{id,target}},base);
            const bool first=curve->controls.front()==id;
            workflowArtifacts_["curve_motion_first_endpoint"]=first;
            const auto beforeEnd=first?base.front().front():base.back().back();
            const auto liveEnd=first?moved.front().front():moved.back().back();
            workflowCase("curve-motion-live-preview-follows-connected-control",
                std::abs(liveEnd.y-beforeEnd.y-.5)<1e-6&&liveEnd.x==beforeEnd.x&&liveEnd.z==beforeEnd.z);
            for(auto* view:views_)view->setAnimationsEnabled(true);
            qApp->setProperty("studio_motion_enabled",true);
            if(!session_.movePoint(id,target))throw std::runtime_error("Cannot move section control point");
            return;
        }
        case 203:{
            using namespace fs::curveEdit;
            const Key key{0,fs::EntityId(workflowArtifacts_["curve_motion_id"].toDouble())};
            const auto motion=curveEditMotion_;
            workflowCase("curve-motion-publishes-real-control-and-waterline-edit",
                motion&&motion->contains(key)&&std::any_of(motion->frameAt(Motion::DurationMs/2).begin(),motion->frameAt(Motion::DurationMs/2).end(),
                    [](const auto& item){return item.first.kind==3;}));
            if(!motion||!motion->contains(key)){finishWorkflow("Missing curve motion on model publication");return;}
            const auto first=motion->frameAt(0).at(key);
            const auto middle=motion->frameAt(Motion::DurationMs/2).at(key);
            const auto last=motion->frameAt(Motion::DurationMs).at(key);
            const bool firstEndpoint=workflowArtifacts_["curve_motion_first_endpoint"].toBool();
            const auto endpoint=[firstEndpoint](const auto& paths){
                const auto& cubics=paths.front().cubics;
                return (firstEndpoint?cubics.front().front():cubics.back().back()).y;
            };
            const auto a=endpoint(first),b=endpoint(middle),c=endpoint(last);
            const auto curve=std::find_if(m->controlCurves.begin(),m->controlCurves.end(),[key](const auto& entry){return entry.id==key.id;});
            const auto cubics=fs::presentation::splineCubics(curve->spline);
            const auto exact=(firstEndpoint?cubics.front().front():cubics.back().back()).y;
            workflowCase("curve-motion-intermediate-and-exact-final-positions",
                first.size()==1&&middle.size()==1&&last.size()==1&&
                ((a<b&&b<c)||(a>b&&b>c))&&std::abs(c-exact)<1e-9);
            workflowArtifacts_["curve_motion_before"]=a;
            session_.undo();return;
        }
        case 204:{
            using namespace fs::curveEdit;
            const Key key{0,fs::EntityId(workflowArtifacts_["curve_motion_id"].toDouble())};
            const auto motion=curveEditMotion_;
            bool restored=false;
            if(motion&&motion->contains(key)){
                const auto& cubics=motion->frameAt(Motion::DurationMs).at(key).front().cubics;
                const bool first=workflowArtifacts_["curve_motion_first_endpoint"].toBool();
                restored=std::abs((first?cubics.front().front():cubics.back().back()).y-
                    workflowArtifacts_["curve_motion_before"].toDouble())<1e-9;
            }
            workflowCase("curve-motion-undo-reaches-original-geometry",restored);
            workflowCases_.append(QJsonObject{{"name","desktop-input-native-dialog-print-visual-acceptance"},{"status","not_run"}});
            auto before=std::make_shared<fs::ModelSnapshot>(*m),after=std::make_shared<fs::ModelSnapshot>(*m);
            before->sections.clear();after->sections.clear();
            const auto low=m->minimum,high=m->maximum;
            const auto mix=[](double a,double b,double t){return a+(b-a)*t;};
            for(int kind=1;kind<=3;++kind)for(int row=0;row<30;++row){
                fs::Section from,to;
                from.kind=kind;from.index=row;from.color=kind==1?0x805f32:kind==2?0x575faf:0x454f29;
                const double f=(row+1)/31.;
                QVector<fs::Vec3> oldPath,newPath;
                for(int i=0;i<=512;++i){
                    const double t=i/512.,rise=std::sin(t*3.141592653589793);
                    fs::Vec3 p,q;
                    const double wav=.05*std::sin(t*15.)+rise*.17;
                    if(kind==1){p={mix(low.x,high.x,f),mix(low.y,high.y,.20+wav),mix(low.z,high.z,t)};q=p;q.y+=(high.y-low.y)*rise*.12;}
                    else if(kind==2){p={mix(low.x,high.x,t),mix(low.y,high.y,f),mix(low.z,high.z,.30+wav)};q=p;q.z+=(high.z-low.z)*rise*.12;}
                    else {p={mix(low.x,high.x,t),mix(low.y,high.y,.30+wav),mix(low.z,high.z,f)};q=p;q.y+=(high.y-low.y)*rise*.12;}
                    oldPath.append(p);newPath.append(q);
                }
                to=from;from.paths={oldPath};to.paths={newPath};before->sections.append(from);after->sections.append(to);
            }
            resize(1440,900);
            for(auto* view:views_){view->setSnapshot(after);view->fit();view->finishPresentation();view->setCurveEditMotion({});}
            QTimer::singleShot(120,this,[this,before,after]{
                QElapsedTimer plan;plan.start();
                const auto motion=fs::curveEdit::Motion::between(*before,*after,{},{});
                const auto ready=plan.nsecsElapsed()/1.e6;
                if(!motion){finishWorkflow("Missing dense curve performance fixture");return;}
                auto clock=std::make_shared<QElapsedTimer>();
                auto frames=std::make_shared<std::array<QJsonArray,4>>();
                for(size_t i=0;i<views_.size();++i){
                    auto* view=views_[i];
                    view->onHullFrame=[view,frames,clock,i]{
                        const double elapsed=clock->nsecsElapsed()/1.e6;
                        if(elapsed<fs::curveEdit::Motion::DurationMs)
                            (*frames)[i].append(QJsonArray{elapsed,view->lastPaintMs});
                    };
                    view->setCurveEditMotion(motion);
                }
                clock->start();motion->start();
                QTimer::singleShot(fs::curveEdit::Motion::DurationMs+60,this,[this,frames,ready]{
                    QJsonArray views;
                    for(size_t i=0;i<views_.size();++i){
                        auto* view=views_[i];view->onHullFrame={};
                        views.append(QJsonObject{{"projection",int(view->projection())},{"frames",(*frames)[i]},
                            {"width",view->width()},{"height",view->height()}});
                    }
                    workflowArtifacts_["curve_motion_paint_benchmark"]=QJsonObject{
                        {"duration_ms",fs::curveEdit::Motion::DurationMs},{"sections",90},{"points_per_section",513},
                        {"plan_ms",ready},{"views",views}};
                    finishWorkflow();
                });
            });
            return;
        }
        }
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
