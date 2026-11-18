// Hidden production commands and flow placement, retained for regression.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <stdexcept>
void MainWindow::verifyAuxiliary(const QString& path,const QString& output){auxiliaryWorkflow_=true;verifyWorkflow(path,output);}
void MainWindow::auxiliaryStep(const QString&){
    if(workflowFinished_)return;
    try{
        auto action=[this](QString operation){
            QList<QAction*> matches;QJsonArray observed;
            for(auto* a:findChildren<QAction*>())if(a->property("operation")==operation){matches.append(a);observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});}
            workflowArtifacts_["action_"+operation]=observed;
            if(matches.size()!=1||!matches[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","auxiliary_workflow_unavailable"},{"status","not_run"},{"message","Missing, ambiguous or disabled action: "+operation}});finishWorkflow();return static_cast<QAction*>(nullptr);
            }
            return matches[0];
        };
        auto remove=[&]{
            auto* a=action("delete");if(!a)return;
            QTimer::singleShot(0,this,[this]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog||!dialog->testAttribute(Qt::WA_DontShowOnScreen)){finishWorkflow("Unsafe or missing deletion dialog");return;}
                auto* buttons=dialog->findChild<QDialogButtonBox*>();auto* yes=buttons?buttons->button(QDialogButtonBox::Yes):nullptr;
                if(!yes){dialog->reject();finishWorkflow("Missing deletion confirmation role");return;}yes->click();});a->trigger();
        };
        auto m=session_.model();
        switch(workflowStage_++){
        case 0:{
            editingOriginal_=m;captureWorkflow("original");
            QSet<QPair<fs::EntityId,fs::EntityId>> owned;
            for(const auto& c:m->controlCurves)for(int i=1;i<c.controls.size();++i)owned.insert(qMakePair(std::min(c.controls[i-1],c.controls[i]),std::max(c.controls[i-1],c.controls[i])));
            fs::EntityId edgeId=0;
            for(const auto& e:m->edges)if(!owned.contains(qMakePair(std::min(e.a,e.b),std::max(e.a,e.b)))){
                const auto* p=m->point(e.a);if(p&&!p->locked&&p->batchMovable){edgeId=e.id;workflowPoint_=p->id;workflowOriginalPoint_=p->position;break;}
            }
            if(!edgeId)throw std::runtime_error("Fixture needs an unassigned edge with an editable point");
            if(auto* a=action("select_edge"))a->trigger();else return;
            selectEntities(HullEntity::Edge,{edgeId});if(auto* a=action("create_control_curve"))a->trigger();return;
        }
        case 1:{
            workflowCase("curve-created-by-production-action",m->controlCurves.size()==editingOriginal_->controlCurves.size()+1);
            for(const auto& c:m->controlCurves)if(std::none_of(editingOriginal_->controlCurves.begin(),editingOriginal_->controlCurves.end(),[&](auto& old){return old.id==c.id;}))auxiliaryCurve_=c.id;
            captureWorkflow("control");auto point=workflowOriginalPoint_;point.z+=std::max(1.,m->draft)*.002;session_.movePoint(workflowPoint_,point);return;
        }
        case 2:captureWorkflow("moved");session_.undo();return;
        case 3:captureWorkflow("undo_move");session_.redo();return;
        case 4:{
            captureWorkflow("redo_move");editingChanged_=m;
            fs::Vec3 source;bool found=false;double best=1e300;
            const double length=std::max(1e-9,m->maximum.x-m->minimum.x),draft=std::max(1e-9,m->draft);
            for(const auto& t:m->triangles){const auto* l=m->layer(t.layer);if(!l||!l->useInHydrostatics)continue;
                auto a=t.vertices[0],b=t.vertices[1],c=t.vertices[2];fs::Vec3 p{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
                if(p.z>=m->waterlineZ||std::abs((b.x-a.x)*(c.z-a.z)-(c.x-a.x)*(b.z-a.z))<1e-8)continue;
                // Use an interior side point. A triangle at the bow tip can
                // legitimately produce only a stagnation point in the old core.
                double score=std::pow((p.x-m->minimum.x)/length-.8,2)+std::pow((p.z-m->minimum.z)/draft-.4,2);
                if(score<best){best=score;source=p;found=true;}
            }
            if(!found)throw std::runtime_error("Fixture has no submerged side triangle");
            auto* a=action("create_flowline");if(!a)return;a->trigger();HullView* target=nullptr;for(auto* v:views_)if(v->projection()==HullProjection::Profile)target=v;
            if(!target||!target->flowPlacement())throw std::runtime_error("Flow placement was not activated");
            target->fit();auto p=target->project(source);workflowArtifacts_["flow_source"]=QJsonArray{source.x,source.z};
            QMouseEvent press(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(target,&press);
            QMouseEvent release(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(target,&release);return;
        }
        case 5:
            workflowCase("flow-created-from-view-click",m->flowlines.size()==editingChanged_->flowlines.size()+1);
            workflowCase("flow-tool-remains-active-after-document-update",addFlow_&&addFlow_->isChecked()&&std::all_of(views_.begin(),views_.end(),[](auto* v){return v->projection()==HullProjection::Perspective||v->flowPlacement();}));
            for(const auto& c:m->flowlines)if(std::none_of(editingChanged_->flowlines.begin(),editingChanged_->flowlines.end(),[&](auto& old){return old.id==c.id;}))auxiliaryFlow_=c.id;
            captureWorkflow("flow");
            for(int i=0;i<int(views_.size());++i){views_[i]->setFillVisible(false);views_[i]->setControlNetVisible(false);views_[i]->finishPresentation();views_[i]->image().save(workflowOutput_+"/views/curves-"+QString::number(i)+".png");}
            if(auto* a=action("select_curve"))a->trigger();else return;selectEntities(HullEntity::Curve,{auxiliaryCurve_});remove();return;
        case 6:captureWorkflow("deleted_curve");session_.undo();return;
        case 7:
            captureWorkflow("restored_curve");if(auto* a=action("select_flow"))a->trigger();else return;selectEntities(HullEntity::Flowline,{auxiliaryFlow_});remove();return;
        case 8:captureWorkflow("deleted_flow");session_.undo();return;
        case 9:
            captureWorkflow("restored_flow");workflowArtifacts_["saved_file"]=workflowOutput_+"/saved.fbm";session_.save(workflowOutput_+"/saved.fbm");return;
        case 10:session_.load(workflowOutput_+"/saved.fbm");return;
        case 11:
            captureWorkflow("reopened");workflowCases_.append(QJsonObject{{"name","curves_visual_parity"},{"status","review"},{"message","Saved four-view images; physical desktop acceptance remains separate"}});finishWorkflow();return;
        }
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
