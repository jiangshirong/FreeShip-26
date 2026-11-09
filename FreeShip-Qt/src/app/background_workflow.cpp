// Retained hidden production workflow. No desktop input injection.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QPointer>
#include <QSpinBox>
#include <QTimer>
#include <stdexcept>
namespace {
QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing background workflow artifact");return f.readAll();}
void mouse(HullView* view,QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons held){QMouseEvent e(type,p,p,button,held,Qt::NoModifier);QApplication::sendEvent(view,&e);}
}
void MainWindow::verifyBackgrounds(const QString& model,const QString& output){
    backgroundWorkflow_=true;verifyWorkflow(model,output);
    QImage pattern(128,96,QImage::Format_RGB32);pattern.fill(Qt::white);
    for(int y=0;y<96;++y)for(int x=0;x<128;++x){if(x%16==0)pattern.setPixelColor(x,y,Qt::blue);if(y%16==0)pattern.setPixelColor(x,y,Qt::red);}
    if(!pattern.save(output+"/background.jpg","JPEG",100))finishWorkflow("Cannot create retained image fixture");
}
void MainWindow::backgroundStep(const QString& method){
    if(workflowFinished_)return;
    try{
        auto* view=views_[0];const auto model=session_.model();const auto* b=model->background(1);
        auto action=[&](const QString& operation,bool allowDisabled=false)->QAction*{
            for(auto* menu:view->findChildren<QMenu*>(QString(),Qt::FindDirectChildrenOnly))delete menu;
            QContextMenuEvent e(QContextMenuEvent::Keyboard,{20,40},{20,40});QApplication::sendEvent(view,&e);
            QList<QAction*> found;QJsonArray candidates;
            for(auto* a:view->findChildren<QAction*>())if(a->property("background_operation")==operation){found.append(a);candidates.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});}
            QFile evidence(workflowOutput_+"/actions.jsonl");if(evidence.open(QIODevice::Append))evidence.write(QJsonDocument(QJsonObject{{"stage",workflowStage_},{"operation",operation},{"candidates",candidates}}).toJson(QJsonDocument::Compact)+'\n');
            for(auto* menu:view->findChildren<QMenu*>())menu->hide();
            if(found.size()!=1||(!allowDisabled&&!found.front()->isEnabled())){
                workflowCases_.append(QJsonObject{{"name","background_workflow_unavailable"},{"status","not_run"},{"message","Missing, ambiguous or disabled action; dependent workflow not executed. See actions.jsonl."}});
                finishWorkflow();return nullptr;
            }return found.front();
        };
        auto dialogs=[&](std::function<void()> trigger,bool accept,QJsonObject values=QJsonObject{}){
            int handled=0;QString failure;QTimer timer;timer.setInterval(10);QPointer<QDialog> visited;QElapsedTimer elapsed;elapsed.start();
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog)for(auto* candidate:findChildren<QDialog*>())if(candidate->isVisible()&&candidate->isModal()){dialog=candidate;break;}
                if(!dialog){
                    if(elapsed.elapsed()>10000){failure="No safely discoverable active dialog";for(auto* candidate:findChildren<QDialog*>())candidate->reject();}
                    return;
                }
                if(elapsed.elapsed()>10000){failure="Dialog interaction timed out";dialog->reject();return;}
                const bool first=visited!=dialog;
                if(first){
                    visited=dialog;++handled;
                    QFile evidence(workflowOutput_+"/dialogs.jsonl");if(evidence.open(QIODevice::Append))evidence.write(QJsonDocument(QJsonObject{{"stage",workflowStage_},{"class",dialog->metaObject()->className()},{"operation",dialog->property("background_operation").toString()},{"accept",accept}}).toJson(QJsonDocument::Compact)+'\n');
                }
                if(auto* file=qobject_cast<QFileDialog*>(dialog)){
                    if(!accept){file->reject();return;}
                    if(first){
                        auto path=QFileInfo(values["file"].toString());file->setDirectory(path.absolutePath());
                        // QFileDialog::selectFile deliberately leaves a focused filename editor
                        // untouched while visible. Select through its public API after releasing focus.
                        if(auto* focused=file->focusWidget())focused->clearFocus();
                        file->selectFile(path.absoluteFilePath());return;
                    }
                    auto* box=file->findChild<QDialogButtonBox*>();if(box)for(auto* button:box->buttons())if(box->buttonRole(button)==QDialogButtonBox::AcceptRole&&button->isEnabled()){button->click();return;}
                    return;
                }
                if(!first)return;
                if(auto* message=qobject_cast<QMessageBox*>(dialog)){message->button(accept?QMessageBox::Yes:QMessageBox::No)->click();return;}
                for(auto* field:dialog->findChildren<QDoubleSpinBox*>()){const auto key=field->property("background_field").toString();if(values.contains(key))field->setValue(values[key].toDouble());}
                for(auto* field:dialog->findChildren<QSpinBox*>()){const auto key=field->property("background_field").toString();if(values.contains(key))field->setValue(values[key].toInt());}
                auto* box=dialog->findChild<QDialogButtonBox*>();auto* button=box?box->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel):nullptr;
                if(!button){failure="Unrecognized dialog";dialog->reject();return;}button->click();
            });
            timer.start();trigger();if(!handled||!failure.isEmpty())throw std::runtime_error(("Production background dialog was not exercised: "+failure).toStdString());
        };
        auto capture=[&](const QString& phase){captureWorkflow(phase);workflowCase(phase+"_linesplan_unchanged",bytes(session_.scenePath())==workflowOriginalDigest_);};
        switch(workflowStage_++){
        case 0:{
            if(method!="load")throw std::runtime_error("Expected background workflow load");
            workflowOriginalDigest_=bytes(session_.scenePath());capture("original");
            auto* a=action("import");if(!a)return;
            dialogs([&]{a->trigger();},false);workflowCase("cancel_import_preserves_model",session_.model()==model&&!session_.busy());
            dialogs([&]{a->trigger();},true,{{"file",workflowOutput_+"/background.jpg"}});break;
        }
        case 1:{
            capture("imported");workflowCase("import_preserves_jpeg_bytes",b&&b->jpeg==bytes(workflowOutput_+"/background.jpg"));
            workflowCase("import_does_not_create_other_views",model->backgrounds.size()==1&&model->background(0)==nullptr&&model->background(2)==nullptr);
            editingOriginal_=model;view->image().save(workflowOutput_+"/views/background-imported.png");
            auto* a=action("visible");if(a)a->trigger();break;
        }
        case 2:{
            workflowCase("hidden_image_has_no_frame_mode",b&&!b->visible&&!view->setBackgroundMode(BackgroundMode::Frame));
            auto* a=action("visible");if(a)a->trigger();break;
        }
        case 3:{
            auto* a=action("origin");if(!a)return;a->trigger();const auto start=view->backgroundRect().center();
            mouse(view,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            mouse(view,QEvent::MouseMove,start+QPointF(24,-16),Qt::NoButton,Qt::LeftButton);
            workflowCase("drag_preview_does_not_commit",session_.model()==model&&!session_.busy());
            mouse(view,QEvent::MouseButtonRelease,start+QPointF(24,-16),Qt::LeftButton,Qt::NoButton);break;
        }
        case 4:capture("moved");workflowCase("origin_drag_committed",b&&b->origin!=editingOriginal_->background(1)->origin);undo_->trigger();break;
        case 5:capture("undo");workflowCase("undo_restores_image_origin",b&&b->metadata()==editingOriginal_->background(1)->metadata());redo_->trigger();break;
        case 6:{
            capture("redo");editingChanged_=model;
            const auto point=view->backgroundRect().topLeft()+QPointF(view->backgroundRect().width()*.8,view->backgroundRect().height()*.2);
            const auto reference=view->backgroundReference(point);auto* a=action("scale");if(!a)return;a->trigger();
            dialogs([&]{mouse(view,QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);},true,{{"coordinate_x",reference.x()*2},{"coordinate_y",reference.y()*2}});
            mouse(view,QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);break;
        }
        case 7:{
            capture("calibrated");workflowCase("reference_coordinate_doubles_scale",b&&b->scale==double(float(editingChanged_->background(1)->scale*2)));
            auto* a=action("alpha");if(!a)return;dialogs([&]{a->trigger();},false,{{"alpha",96}});
            workflowCase("cancel_blending_preserves_model",session_.model()==model&&!session_.busy());
            dialogs([&]{a->trigger();},true,{{"alpha",96}});break;
        }
        case 8:{
            capture("appearance");editingChanged_=model;workflowCase("blending_applied",b&&b->alpha==96);
            view->image().save(workflowOutput_+"/views/background-calibrated.png");
            auto* a=action("export");if(!a)return;dialogs([&]{a->trigger();},true,{{"file",workflowOutput_+"/exported.jpg"}});
            workflowCase("raw_jpeg_export_exact",bytes(workflowOutput_+"/exported.jpg")==b->jpeg);
            a=action("remove");if(!a)return;dialogs([&]{a->trigger();},false);workflowCase("cancel_remove_preserves_model",session_.model()==model&&!session_.busy());
            dialogs([&]{a->trigger();},true);break;
        }
        case 9:{
            capture("removed");workflowCase("remove_exits_background_mode",!b&&view->backgroundMode()==BackgroundMode::None);
            auto* a=action("frame",true);if(!a)return;workflowCase("no_image_disables_frame_action",!a->isEnabled());undo_->trigger();break;
        }
        case 10:{
            capture("restored");workflowCase("undo_delete_restores_bytes_and_calibration",b&&b->jpeg==editingChanged_->background(1)->jpeg&&b->metadata()==editingChanged_->background(1)->metadata());
            auto path=workflowOutput_+"/saved.fbm";workflowArtifacts_["saved_file"]=path;session_.save(path);break;
        }
        case 11:workflowCase("save_clean",method=="save"&&!session_.modified());session_.load(workflowArtifacts_["saved_file"].toString());break;
        case 12:
            capture("reopened");workflowCase("reopen_retains_exact_image",b&&b->jpeg==editingChanged_->background(1)->jpeg&&b->metadata()==editingChanged_->background(1)->metadata());
            workflowCases_.append(QJsonObject{{"name","background_visual_parity"},{"status","review"},{"message","Inspect saved images; Qt blending/filtering can differ from legacy raster rounding."}});
            workflowCases_.append(QJsonObject{{"name","desktop_acceptance"},{"status","not_run"},{"message","Hidden production actions only; no desktop input."}});finishWorkflow();break;
        default:throw std::runtime_error("Unexpected background workflow stage");
        }
    }catch(const std::exception& ex){finishWorkflow(QString::fromUtf8(ex.what()));}
}
