// Hidden tests of production actions; no desktop input or user files.
#include "main_window.h"
#include "section_dialog.h"
#include "layer_panel.h"
#include "ui_language.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QHeaderView>
#include <QScrollBar>
#include <QShortcut>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>
#include <stdexcept>
namespace {
QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing section evidence");return f.readAll();}
// Feed ordinary key events through Qt's shortcut routing in the isolated editor.
// This observes production bindings; it does not activate QAction/QShortcut directly.
void historyKey(QWidget* window, QWidget* focus, int key, Qt::KeyboardModifiers modifiers=Qt::ControlModifier) {
    if(!window->testAttribute(Qt::WA_DontShowOnScreen))throw std::runtime_error("History probe requires private window");
    QApplication::setActiveWindow(window);focus->setFocus();
    QKeyEvent press(QEvent::KeyPress,key,modifiers);
    QApplication::sendEvent(focus,&press);
    QKeyEvent release(QEvent::KeyRelease,key,modifiers);
    QApplication::sendEvent(focus,&release);
    // End the entire synthetic chord, including its modifier keys. Otherwise
    // Qt's later programmatic row selection can inherit Ctrl+Shift state.
    for(const auto modifier:{Qt::Key_Shift,Qt::Key_Alt,Qt::Key_Control}){
        QKeyEvent up(QEvent::KeyRelease,modifier,Qt::NoModifier);QApplication::sendEvent(focus,&up);
    }
}
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
                if(a->property("section_operation")=="clear"){
                    const auto* yes=buttons?buttons->button(QDialogButtonBox::Yes):nullptr;
                    workflowCase("clear-confirmation-caption",yes&&yes->text()==uiText("确定"));
                    dialog->grab().save(workflowOutput_+"/section-clear-confirmation.png");
                }
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
            auto* definitions=table();
            QDir().mkpath(workflowOutput_+"/icons");QJsonArray iconEvidence;
            const auto observeIcon=[&](const QString& operation,const QIcon& icon){
                for(int size:{18,20})for(qreal ratio:{1.,1.25,2.}){
                    const auto path=workflowOutput_+QString("/icons/%1-%2-%3.png").arg(operation).arg(size).arg(ratio);
                    const auto pixels=icon.pixmap(QSize(size,size),ratio).toImage();pixels.save(path);
                    iconEvidence.append(QJsonObject{{"operation",operation},{"name",icon.name()},{"size",size},{"ratio",ratio},{"image",path}});
                }
            };
            QJsonArray headerActions,rows;
            for(auto* button:definitions->horizontalHeader()->findChildren<QToolButton*>()){
                observeIcon(button->defaultAction()->property("section_operation").toString(),button->icon());
                headerActions.append(QJsonObject{{"operation",button->defaultAction()->property("section_operation").toString()},
                    {"x",button->x()},{"right",button->geometry().right()},{"visible",button->isVisible()},{"icon",button->icon().name()}});
            }
            for(int row=0;row<definitions->rowCount();++row){
                auto* button=definitions->cellWidget(row,0)->findChild<QToolButton*>();
                if(row==0)observeIcon("delete",button->icon());
                rows.append(QJsonObject{{"number",definitions->item(row,0)->text()},{"index",button->property("section_index").toInt()},
                    {"accessible",button->accessibleName()},{"visible",button->isVisible()},{"icon",button->icon().name()},
                    {"visible_number",definitions->cellWidget(row,0)->findChild<QLabel*>("SectionNumber")->text()}});
            }
            workflowArtifacts_["section_icons"]=iconEvidence;
            workflowArtifacts_["section_ui"]=QJsonObject{{"rows",rows},{"header_actions",headerActions},
                {"number_column_width",definitions->columnWidth(0)},{"scroll_maximum",definitions->verticalScrollBar()->maximum()},
                {"width",sections_->width()},{"height",sections_->height()}};
            workflowCase("header-has-three-actions",definitions->horizontalHeader()->findChildren<QToolButton*>().size()==3);
            workflowCase("row-has-delete-button",definitions->rowCount()==0||definitions->cellWidget(0,0)->findChild<QToolButton*>()!=nullptr);
            dialogAction(action("add"),false,{{"start",outside}});
            workflowCase("add_cancel_preserves_revision",session_.model()==model&&!session_.busy());
            const int beforeRows=table()->rowCount();
            dialogAction(action("add_range"),true,{{"start",outside},{"end",outside+2},{"spacing",1}});
            workflowCase("section-list-updates-before-backend",session_.busy()&&table()->rowCount()==beforeRows+3);
            workflowCase("section-actions-stay-enabled",action("add")->isEnabled()&&table()->isEnabled());
            dialogAction(action("add"),true,{{"start",outside+3}});
            workflowCase("queued-add-visible",table()->rowCount()==beforeRows+4);
            auto* last=table()->cellWidget(table()->rowCount()-1,0)->findChild<QToolButton*>();last->click();
            workflowCase("queued-delete-visible",table()->rowCount()==beforeRows+3);
            break;
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
            dialogAction(action("clear"),false);workflowCase("clear_cancel_preserves_revision",session_.model()==model&&!session_.busy());
            // The cell's own undo must win over document undo while editing.
            auto* item=t->item(0,1);t->setCurrentItem(item);t->editItem(item);
            auto* editor=t->findChild<QLineEdit*>();
            if(!editor)throw std::runtime_error("Distance editor unavailable");
            const auto beforeText=editor->text();editor->selectAll();editor->insert(beforeText+"9");
            historyKey(sections_,editor,'Z');
            workflowCase("distance-editor-keeps-local-undo",editor->text()==beforeText&&session_.model()==model&&!session_.busy());
            // Cancel rather than commit the temporary editor.
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(editor,&escape);
            QToolButton* remove=nullptr;
            for(int row=0;row<table()->rowCount();++row)if(float(table()->item(row,1)->text().toDouble())==float(outside+0.25))
                remove=table()->cellWidget(row,0)->findChild<QToolButton*>();
            if(!remove||!remove->isEnabled())throw std::runtime_error("Row delete button unavailable");
            table()->clearSelection();if(table()->rowCount()>1)table()->selectRow(0);
            workflowOriginalDigest_=bytes(session_.scenePath());
            remove->click();break;
        }
        case 3:{
            const bool originalSectionsRemain=std::all_of(editingOriginal_->sections.begin(),editingOriginal_->sections.end(),[&](const auto& before){
                return std::any_of(model->sections.begin(),model->sections.end(),[&](const auto& after){return before.kind==after.kind&&before.plane==after.plane;});
            });
            workflowCase("row-delete-keeps-unrelated-sections",originalSectionsRemain&&std::none_of(model->sections.begin(),model->sections.end(),[&](const auto& s){return s.kind==1&&s.distance==float(outside+0.25);}));
            captureWorkflow("deleted");workflowCase("delete_removes_one_definition",model->sections.size()==editingOriginal_->sections.size()+2);workflowMovedDigest_=bytes(session_.scenePath());
            historyKey(sections_,table(),Qt::Key_Backspace,Qt::AltModifier);
            workflowCase("removed-alt-backspace-does-not-undo",!session_.busy()&&session_.model()==model);
            // Simulate the previous main-window-only binding as a negative control.
            QShortcut* localUndo=nullptr;
            for(auto* shortcut:sections_->findChildren<QShortcut*>())if(shortcut->property("shortcut_action").value<QObject*>()==undo_)localUndo=shortcut;
            if(!localUndo)throw std::runtime_error("Dialog undo binding unavailable");
            localUndo->setEnabled(false);historyKey(sections_,table(),'Z');
            workflowCase("main-window-shortcut-does-not-reach-section-dialog",!session_.busy()&&session_.model()==model);
            localUndo->setEnabled(undo_->isEnabled());historyKey(sections_,table(),'Z');
            workflowCase("section-dialog-ctrl-z-key-event-dispatched",session_.busy());
            if(!session_.busy())throw std::runtime_error("Ctrl+Z did not dispatch undo from section dialog");
            break;
        }
        case 4:{
            captureWorkflow("undo");workflowCase("undo_restores_definition",model->sections.size()==editingOriginal_->sections.size()+3);
            workflowCase("undo-restores-exact-section-scene",bytes(session_.scenePath())==workflowOriginalDigest_);
            workflowCase("undo-restores-deleted-plane",std::any_of(model->sections.begin(),model->sections.end(),[&](const auto& s){return s.kind==1&&s.distance==float(outside+0.25);}));
            workflowCase("undo-refreshes-open-section-list",sections_->isVisible()&&table()->rowCount()==std::count_if(model->sections.begin(),model->sections.end(),[](const auto& s){return s.kind==1;}));
            auto* rowButton=table()->cellWidget(0,0)->findChild<QToolButton*>();
            historyKey(sections_,rowButton,Qt::Key_Y);
            workflowCase("removed-ctrl-y-does-not-redo",!session_.busy()&&session_.model()==model);
            historyKey(sections_,rowButton,Qt::Key_Backspace,Qt::AltModifier|Qt::ShiftModifier);
            workflowCase("removed-alt-shift-backspace-does-not-redo",!session_.busy()&&session_.model()==model);
            historyKey(sections_,rowButton,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
            workflowCase("section-dialog-ctrl-shift-z-key-event-dispatched",session_.busy());
            if(!session_.busy())throw std::runtime_error("Ctrl+Shift+Z did not dispatch redo from section dialog");
            break;
        }
        case 5:{
            captureWorkflow("redo");workflowCase("redo_scene_exact",bytes(session_.scenePath())==workflowMovedDigest_);
            sections_->reject();sectionsAction_->trigger();auto* tabs=sections_->findChild<QTabWidget*>();
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
            int selectedRow=-1;for(int row=0;row<table()->rowCount();++row)if(table()->item(row,0)->data(Qt::UserRole).toInt()==curvedIndex)selectedRow=row;
            // This check concerns the production selection-to-highlight signal,
            // not QTableView::selectRow's modifier-dependent convenience API.
            // Use an explicit row selection so earlier synthetic keyboard
            // routing cannot turn it into a range/toggle operation.
            workflowArtifacts_["highlight_selection_input"]=QJsonObject{{"row",selectedRow},{"rows",table()->rowCount()},
                {"modifiers",int(QApplication::keyboardModifiers())},{"enabled",table()->isEnabled()},{"kind",sections_->kind()}};
            if(selectedRow<0)throw std::runtime_error("Diagonal row unavailable for selection verification");
            table()->selectionModel()->select(table()->model()->index(selectedRow,0),QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
            workflowCase("table_selection_highlights_world_curve",view->image()!=shown);
            workflowArtifacts_["section_highlight"]=QJsonObject{{"index",curvedIndex},{"selected",sections_->selectedIndices().size()},
                {"curve_paths",int(std::find_if(model->sections.begin(),model->sections.end(),[&](const auto& s){return s.kind==4&&s.index==curvedIndex;})->paths.size())}};
            view->image().save(workflowOutput_+"/sections-selected-view.png");
            sections_->grab().save(workflowOutput_+"/section-dialog-edited.png");sections_->reject();
            for(auto* v:views_){v->fit();for(int i=0;i<10;++i){v->panBy({1,2});v->zoomAt({200,100},1.01);v->image();}}
            workflowCase("camera_does_not_mutate_or_request_backend",session_.model()==model&&!session_.busy()&&!session_.modified());
            // Unlike the older check above, press Ctrl+Z in the same event turn
            // as deletion: no backend completion or wait is allowed in between.
            sectionsAction_->trigger();sections_->findChild<QTabWidget*>()->setCurrentIndex(0);
            workflowOriginalDigest_=bytes(session_.scenePath());
            auto* remove=table()->cellWidget(0,0)->findChild<QToolButton*>();remove->click();
            workflowCase("immediate-delete-is-still-computing",session_.editingSections());
            historyKey(sections_,table(),'Z');
            workflowCase("immediate-delete-undo-queued",session_.sectionHistoryPending());
            if(!session_.sectionHistoryPending())throw std::runtime_error("Immediate Ctrl+Z was dropped while delete was pending");
            break;
        }
        case 13:{
            workflowCase("immediate-delete-undo-restores-exact-scene",bytes(session_.scenePath())==workflowOriginalDigest_);
            workflowCase("immediate-delete-undo-refreshes-list",table()->rowCount()==std::count_if(model->sections.begin(),model->sections.end(),[](const auto& s){return s.kind==1;}));
            workflowOriginalDigest_=bytes(session_.scenePath());
            dialogAction(action("clear"),true);historyKey(sections_,table(),'Z');
            workflowCase("immediate-clear-undo-queued",session_.sectionHistoryPending());
            if(!session_.sectionHistoryPending())throw std::runtime_error("Immediate Ctrl+Z was dropped while clear was pending");
            break;
        }
        case 14:
            workflowCase("immediate-clear-undo-restores-exact-scene",bytes(session_.scenePath())==workflowOriginalDigest_);
            workflowCase("history-queue-drains",!session_.sectionHistoryPending()&&!session_.busy());sections_->reject();finishWorkflow();break;
        default:throw std::runtime_error("Unexpected section workflow stage");
        }
    }catch(const std::exception& error){finishWorkflow(QString::fromUtf8(error.what()));}
}
