// Reproducible hidden exercise of the real presentation actions, followed by
// the existing move/undo/redo/save/reopen workflow. No desktop input injection.
#include "main_window.h"
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <algorithm>
#include <stdexcept>
namespace {QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing artifact");return f.readAll();}}
void MainWindow::verifyOverlays(const QString& model,const QString& output){overlayWorkflow_=true;verifyWorkflow(model,output);}
void MainWindow::overlayStep(const QString& method){
    if(workflowFinished_)return;
    try{
        if(workflowStage_==0){
            auto model=session_.model();const auto scene=read(session_.scenePath()),dxf=read(session_.dxfPath());
            QJsonArray discoveries;
            auto action=[this,&discoveries](const QString& operation,int axis=-1){
                QList<QAction*> matches;QJsonArray candidates;
                for(auto* a:findChildren<QAction*>())if(a->property("operation")==operation&&(axis<0||a->property("coordinate_axis").toInt()==axis)){
                    matches.append(a);candidates.append(QJsonObject{{"text",a->text()},{"object_name",a->objectName()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});
                }
                discoveries.append(QJsonObject{{"operation",operation},{"axis",axis},{"candidates",candidates}});
                if(matches.size()!=1||!matches.front()->isEnabled()){
                    workflowCases_.append(QJsonObject{{"name",operation+"_discovery_"+QString::number(axis)},{"status","not_run"},{"message","Action missing, ambiguous or disabled; see overlay-actions.json."}});
                    return static_cast<QAction*>(nullptr);
                }
                return matches.front();
            };
            for(auto pair:{std::pair<QString,bool>{"overlay_grid",false},{"overlay_hydro",true}}){
                auto* a=action(pair.first);
                if(!a)continue;
                a->setChecked(false);workflowCase(pair.first+"_off",std::all_of(views_.begin(),views_.end(),[&](auto* v){return !(pair.second?v->hydroVisible():v->gridVisible());}));
                a->trigger();workflowCase(pair.first+"_on",std::all_of(views_.begin(),views_.end(),[&](auto* v){return pair.second?v->hydroVisible():v->gridVisible();}));
            }
            for(int axis:{1,2,3,0})if(auto* a=action("coordinate_axis",axis)){a->trigger();workflowCase("coordinate_axis_"+QString::number(axis),std::all_of(views_.begin(),views_.end(),[&](auto* v){return v->coordinateAxis()==axis;}));}
            QFile discoveryFile(workflowOutput_+"/overlay-actions.json");if(discoveryFile.open(QIODevice::WriteOnly))discoveryFile.write(QJsonDocument(discoveries).toJson());
            QJsonArray evidence;int index=0;
            for(auto* view:views_){
                view->finishPresentation();const auto complete=view->image();view->restartPresentation();
                view->presentationImageAt(0);view->presentationImageAt(550);view->presentationImageAt(1100).save(workflowOutput_+"/views/overlay-moving-"+QString::number(index)+".png");
                workflowCase("export_during_animation_"+QString::number(index),view->image()==complete);
                view->panBy({8,4});workflowCase("navigation_interrupts_"+QString::number(index),!view->presentationActive());view->fit();
                view->image().save(workflowOutput_+"/views/overlay-complete-"+QString::number(index)+".png");evidence.append(view->overlayEvidence());++index;
            }
            QFile info(workflowOutput_+"/overlay-evidence.json");if(info.open(QIODevice::WriteOnly))info.write(QJsonDocument(evidence).toJson());
            workflowCase("presentation_does_not_edit_document",session_.model()==model&&!session_.modified()&&!session_.busy()&&!session_.canUndo());
            workflowCase("presentation_preserves_scene_and_dxf",read(session_.scenePath())==scene&&read(session_.dxfPath())==dxf);
            workflowCases_.append(QJsonObject{{"name","original-ui-visual-parity"},{"status","review"},{"message","Saved four-view images and animation frames; full desktop visual acceptance remains separate."}});
        }
        workflowStep(method);
    }catch(const std::exception& ex){finishWorkflow(QString::fromUtf8(ex.what()));}
}
