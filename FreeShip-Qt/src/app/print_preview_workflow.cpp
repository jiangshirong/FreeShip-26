// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the current viewport PDF menu in an isolated test instance.
#include "main_window.h"
#include "studio_ui.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QEventLoop>
#include <QMenu>
#include <QTimer>

void MainWindow::auditViewportPdf(){
    if(!startupProbe_||session_.busy()||!session_.ready()){finishWorkflow("View export needs a private idle document");return;}
    const bool oldWorkflow=workflow_;workflow_=true;
    const auto model=session_.model();const bool modified=session_.modified(),undo=session_.canUndo();
    QJsonArray rows;
    for(int sample=0;sample<5;++sample){
        auto* view=views_[sample%4];const bool filled=sample==4;
        view->setAttribute(Qt::WA_DontShowOnScreen);view->setAnimationsEnabled(false);
        view->resize(960,540);view->setFillVisible(filled);view->fit();view->finishPresentation();
        const auto projection=int(view->projection());
        const auto prefix=workflowOutput_+QString("/view-%1%2").arg(projection).arg(filled?"-filled":"");
        const auto route=[&](const QString& destination,bool pan,bool cancel){
            QContextMenuEvent event(QContextMenuEvent::Mouse,{20,20},view->mapToGlobal(QPoint(20,20)));
            QApplication::sendEvent(view,&event);
            QMenu* menu=nullptr;
            for(auto* candidate:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))
                if(candidate->isVisible())menu=candidate;
            QAction* entry=nullptr;int matches=0;bool print=false;
            if(menu)for(auto* action:menu->actions()){
                const auto operation=action->property("view_operation").toString();
                if(operation=="export_pdf"){entry=action;++matches;}
                if(operation=="print")print=true;
            }
            const bool ready=matches==1&&!print&&entry->isEnabled()&&!entry->icon().isNull()&&entry->property("studio_opens_dialog").toBool();
            if(!ready){if(menu)menu->close();return false;}
            bool handled=false;
            QTimer picker;picker.setInterval(10);
            connect(&picker,&QTimer::timeout,this,[&]{
                auto* file=qobject_cast<QFileDialog*>(frontStudioDialog());
                if(!file||file->property("view_operation")!="export_pdf_file")return;
                picker.stop();handled=true;
                if(pan)view->panBy({37,19});
                file->selectFile(destination);
                if(cancel)file->reject();else QMetaObject::invokeMethod(file,"accept",Qt::DirectConnection);
            });
            QEventLoop wait;QTimer safety;safety.setSingleShot(true);
            connect(&safety,&QTimer::timeout,&wait,[&]{if(auto* file=frontStudioDialog())file->reject();wait.quit();});
            connect(&picker,&QTimer::timeout,&wait,[&]{if(handled)wait.quit();});
            picker.start();safety.start(5000);entry->trigger();menu->close();wait.exec();
            if(pan)view->panBy({-37,-19});
            return handled;
        };
        const auto file=prefix+".pdf";
        const bool saved=route(prefix,false,false)&&QFileInfo(file).size()>0;
        workflowCase("viewport-pdf-menu-"+QString::number(sample),saved);
        QJsonObject row{{"projection",projection},{"filled",filled},{"label",view->projectionLabel()},{"pdf",file}};
        if(sample==0){
            const auto frozen=prefix+"-frozen.pdf",cancelled=prefix+"-cancelled.pdf";
            workflowCase("viewport-pdf-captures-before-dialog",route(frozen,true,false)&&QFileInfo(frozen).size()>0);
            workflowCase("viewport-pdf-can-cancel",route(cancelled,false,true)&&!QFileInfo::exists(cancelled));
            row["frozen_pdf"]=frozen;
        }
        rows.append(row);
    }
    workflowArtifacts_["viewport_pdfs"]=rows;
    workflowCase("viewport-pdf-keeps-document",session_.model()==model&&session_.modified()==modified&&session_.canUndo()==undo);
    workflow_=oldWorkflow;
}
