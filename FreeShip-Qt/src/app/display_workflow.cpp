// Hidden exercise of production actions and the real section checkbox.
#include "main_window.h"
#include "section_dialog.h"
#include <QAction>
#include <QMenu>
#include <QTableWidget>
#include <QTimer>
#include <stdexcept>
void MainWindow::verifyDisplay(const QString& path,const QString& output){displayWorkflow_=true;verifyWorkflow(path,output);}
void MainWindow::displayStep(const QString&){
    if(workflowFinished_)return;
    try{
        auto findAction=[this](const QString& name)->QAction*{
            QList<QAction*> matches;QJsonArray candidates;
            for(auto* a:findChildren<QAction*>())if(a->property("operation")==name){matches.append(a);candidates.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});}
            workflowArtifacts_.insert("action_"+name,candidates);
            if(matches.size()!=1||!matches[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","display_workflow_unavailable"},{"status","not_run"},{"message","Action missing, ambiguous or disabled: "+name}});finishWorkflow();return nullptr;
            }
            return matches[0];
        };
        auto trigger=[&](const QString& name){auto* action=findAction(name);if(!action)return false;action->trigger();return true;};
        auto pictures=[this](const QString& name){
            for(int i=0;i<int(views_.size());++i){auto* v=views_[i];v->setFillVisible(false);v->setControlNetVisible(false);v->finishPresentation();v->fit();v->image().save(workflowOutput_+"/views/"+name+QString::number(i)+".png");}
        };
        switch(workflowStage_++){
        case 0:
            if(std::none_of(session_.model()->sections.begin(),session_.model()->sections.end(),[](const auto& s){return s.kind==1;})){
                workflowStage_=0;const auto m=session_.model();
                session_.editSections("add",1,{{"positions",QJsonArray{double(float((m->minimum.x+m->maximum.x)*.5))}}});return;
            }
            editingOriginal_=session_.model();captureWorkflow("original");trigger("both_sides");return;
        case 1:
            workflowCase("both_sides_action_changes_model",session_.model()->bothSides!=editingOriginal_->bothSides);captureWorkflow("toggled");pictures("toggled");session_.undo();return;
        case 2:
            workflowCase("both_sides_undo",session_.model()->bothSides==editingOriginal_->bothSides);captureWorkflow("undo");session_.redo();return;
        case 3:
            captureWorkflow("redo");trigger("show_curvature");return;
        case 4:
            workflowCase("curvature_action_changes_model",session_.model()->showCurvature!=editingOriginal_->showCurvature);captureWorkflow("curvature_toggled");
            if(!session_.model()->showCurvature){trigger("show_curvature");return;}
            QTimer::singleShot(0,this,[this]{displayStep({});});return;
        case 5:
            workflowCase("curvature_enabled",session_.model()->showCurvature);trigger("curvature_increase");return;
        case 6:
            workflowCase("curvature_scale_increased",session_.model()->curvatureScale==double(float(editingOriginal_->curvatureScale*1.1)));captureWorkflow("scaled");
            if(!trigger("sections"))return;
            QTimer::singleShot(0,this,[this]{
                QList<QTableWidgetItem*> cells;
                for(auto* table:sections_->findChildren<QTableWidget*>())for(int r=0;r<table->rowCount();++r)for(int c=0;c<table->columnCount();++c)
                    if(auto* item=table->item(r,c);item&&item->data(Qt::UserRole)=="show_curvature"&&(item->flags()&Qt::ItemIsUserCheckable))cells.append(item);
                if(cells.isEmpty()){
                    workflowCases_.append(QJsonObject{{"name","display_workflow_unavailable"},{"status","not_run"},{"message","No discoverable section curvature checkbox"}});finishWorkflow();return;
                }
                // All candidates perform the same operation on their own definition.
                auto* item=cells[0];item->setCheckState(item->checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked);
            });return;
        case 7:
            workflowCase("section_checkbox_edits_one_flag",[&]{int count=0;for(int i=0;i<session_.model()->sections.size();++i)count+=session_.model()->sections[i].showCurvature!=editingOriginal_->sections[i].showCurvature;return count==1;}());
            captureWorkflow("section");pictures("curvature");sections_->hide();
            workflowArtifacts_.insert("saved_file",workflowOutput_+"/saved.fbm");session_.save(workflowOutput_+"/saved.fbm");return;
        case 8:
            editingChanged_=session_.model();session_.load(workflowOutput_+"/saved.fbm");return;
        case 9: {
            captureWorkflow("reopened");workflowCase("reopened_display_options",session_.model()->bothSides==editingChanged_->bothSides&&session_.model()->showCurvature==editingChanged_->showCurvature&&session_.model()->curvatureScale==editingChanged_->curvatureScale);
            editingChanged_=session_.model();
            auto* sides=findAction("both_sides");
            auto* curves=findAction("show_control_curves");
            auto* markers=findAction("show_markers");
            if(!sides||!curves||!markers)return;
            QMenu* menu=nullptr;
            for(auto* owner:sides->associatedObjects())if(auto* popup=qobject_cast<QMenu*>(owner)){menu=popup;break;}
            auto* body=views_[1];
            const bool originalSides=editingChanged_->bothSides;
            const bool originalCurves=editingChanged_->showControlCurves;
            const bool originalMarkers=editingChanged_->showMarkers;
            sides->trigger();
            body->image();
            workflowCase("display-immediate-both-sides-preview",session_.busy()&&session_.model()==editingChanged_&&
                body->snapshot()==editingChanged_&&body->overlayEvidence()["both_sides"]==!originalSides);
            if(menu){
                menu->setAttribute(Qt::WA_DontShowOnScreen);
                menu->popup(QPoint(100,100));
                workflowCase("display-reopened-menu-keeps-actions-live",menu->isVisible()&&session_.busy()&&
                    sides->isEnabled()&&curves->isEnabled()&&markers->isEnabled()&&sides->isChecked()==!originalSides);
                menu->hide();
            }else workflowCases_.append(QJsonObject{{"name","display-reopened-menu"},{"status","not_run"},{"message","No associated production popup"}});
            curves->trigger();
            markers->trigger();
            sides->trigger();
            body->image();
            const auto visible=body->overlayEvidence();
            workflowCase("display-latest-clicks-preview-before-reply",session_.busy()&&sides->isChecked()==originalSides&&
                visible["both_sides"]==originalSides&&visible["show_control_curves"]==!originalCurves&&visible["show_markers"]==!originalMarkers);
            return;
        }
        case 10:
            if(session_.busy()){--workflowStage_;return;}
            workflowCase("display-rapid-changes-confirmed",session_.model()->bothSides==editingChanged_->bothSides&&
                session_.model()->showControlCurves!=editingChanged_->showControlCurves&&session_.model()->showMarkers!=editingChanged_->showMarkers&&
                session_.model()->revision==editingChanged_->revision+2);
            captureWorkflow("rapid_confirmed");
            workflowCases_.append(QJsonObject{{"name","display_visual_parity"},{"status","review"},{"message","Four view images retained; desktop acceptance remains separate."}});finishWorkflow();return;
        }
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
