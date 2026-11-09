// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production actions. No native desktop input or user settings.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QTimer>
#include <algorithm>
#include <iterator>

void MainWindow::verifyShading(const QString& path,const QString& output){shadingWorkflow_=true;QApplication::setQuitOnLastWindowClosed(false);verifyWorkflow(path,output);}
void MainWindow::shadingStep(){
    if(workflowFinished_)return;
    try{
        const auto hash=[&]{QJsonArray values;const auto a=analyzeSurface(*session_.model());for(int i=0;i<a->angleDefect.size();++i)values.append(QJsonArray{a->valid[i],a->angleDefect[i]});return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(values).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());};
        const int stage=workflowStage_++;
        if(stage==1){
            for(auto* view:views_)view->image();workflowArtifacts_["edited_analysis"]=hash();workflowCase("edit-rebuilds-analysis",workflowArtifacts_["edited_analysis"]!=workflowArtifacts_["original_analysis"]);captureWorkflow("edited");session_.undo();return;
        }
        if(stage==2){for(auto* view:views_)view->image();workflowCase("undo-restores-analysis",hash()==workflowArtifacts_["original_analysis"].toString());captureWorkflow("restored");session_.save(workflowOutput_+"/unchanged.fbm");return;}
        if(stage==3){workflowArtifacts_["saved_file"]=workflowOutput_+"/unchanged.fbm";workflowCases_.append(QJsonObject{{"name","desktop-print-visual-acceptance"},{"status","not_run"}});finishWorkflow();return;}
        for(auto* view:views_){view->setAttribute(Qt::WA_DontShowOnScreen);view->setAnimationsEnabled(false);}
        captureWorkflow("before");const auto original=session_.model();const bool modified=session_.modified(),undo=session_.canUndo(),redo=session_.canRedo();
        workflowCase("analysis-is-lazy-on-open",std::all_of(std::begin(views_),std::end(views_),[](auto* view){return !view->viewSettingsEvidence().value("analysis_built").toBool();}));
        auto missing=[&](QString entry){workflowCases_.append(QJsonObject{{"name",entry},{"status","not_run"},{"message","Production action missing, ambiguous or disabled."}});finishWorkflow();};
        auto menu=[&](HullView* view)->QMenu*{for(auto* old:view->findChildren<QMenu*>())old->hide();QContextMenuEvent e(QContextMenuEvent::Mouse,{90,90},{90,90});QApplication::sendEvent(view,&e);QList<QMenu*> found;
            for(auto* m:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))if(m->isVisible())found.append(m);if(found.size()!=1){missing("view-menu");return nullptr;}return found[0];};
        auto discover=[&](QMenu* root,const char* property,QVariant value)->QAction*{if(!root)return nullptr;QList<QAction*> found;QJsonArray evidence;
            for(auto* a:root->findChildren<QAction*>())if(a->property(property)==value){found.append(a);evidence.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});}
            workflowArtifacts_[QString(property)+":"+value.toString()]=evidence;if(found.size()!=1||!found[0]->isEnabled()){missing("action:"+value.toString());return nullptr;}return found[0];};
        HullView* perspective=nullptr;for(auto* view:views_)if(view->projection()==HullProjection::Perspective)perspective=view;
        if(!perspective){missing("perspective-view");return;}
        for(int mode:{1,2,3}){
            for(auto* view:views_){auto* root=menu(view);auto* action=discover(root,"shade_mode",mode);if(!action)return;action->trigger();root->hide();workflowCase(QString("mode-%1-projection-%2").arg(mode).arg(int(view->projection())),view->shadeMode()==HullShade(mode));view->image().save(workflowOutput_+QString("/mode-%1-view-%2.png").arg(mode).arg(int(view->projection())));}
            auto* root=menu(perspective);auto* save=discover(root,"view_operation","save_image");if(!save)return;const auto path=workflowOutput_+QString("/mode-%1-export.png").arg(mode);const auto expected=perspective->image();
            QTimer::singleShot(0,this,[this,path]{auto* file=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!file||file->property("view_operation")!="save_image_file"){finishWorkflow("Missing safe image dialog");return;}file->selectFile(path);QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);});save->trigger();root->hide();workflowCase("mode-export-"+QString::number(mode),QImage(path)==expected);
        }
        workflowCase("modes-do-not-edit-document",session_.model()==original&&session_.modified()==modified&&session_.canUndo()==undo&&session_.canRedo()==redo);captureWorkflow("after");workflowArtifacts_["original_analysis"]=hash();
        const fs::ControlPoint* movable=nullptr;for(const auto& p:original->points)if(p.batchMovable&&!p.locked){movable=&p;break;}
        if(!movable){missing("editable-fixture-point");return;}auto position=movable->position;position.z+=.125;session_.movePoint(movable->id,position);
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
