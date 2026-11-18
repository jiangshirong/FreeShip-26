// Retained hidden production-action regression. No desktop automation.
#include "main_window.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <stdexcept>

void MainWindow::verifyAdvanced(const QString& model,const QString& output){advancedWorkflow_=true;verifyWorkflow(model,output);}
void MainWindow::advancedStep(const QString&){
    if(workflowFinished_)return;
    try{
        const auto m=session_.model();
        auto invoke=[&](const QString& operation,QJsonObject values,bool accept=true){
            QList<QAction*> matches;QJsonArray observed;
            for(auto* a:findChildren<QAction*>())if(a->property("operation")==operation){matches.append(a);observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});}
            workflowArtifacts_["action_"+operation]=observed;
            if(matches.size()!=1||!matches[0]->isEnabled()){
                workflowCases_.append(QJsonObject{{"name","advanced_workflow_unavailable"},{"status","not_run"},{"message","Missing, ambiguous or disabled action: "+operation}});finishWorkflow();return false;
            }
            bool handled=false;QString error;QTimer timer;timer.setSingleShot(true);
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(!dialog||!dialog->testAttribute(Qt::WA_DontShowOnScreen)){error="Missing private production dialog";if(dialog)dialog->reject();return;}
                QSet<QString> used;QJsonArray fields;
                for(auto* control:dialog->findChildren<QObject*>()){
                    auto field=control->property("field").toString();if(field.isEmpty())continue;fields.append(field);
                    if(!values.contains(field))continue;
                    used.insert(field);
                    if(auto* spin=qobject_cast<QDoubleSpinBox*>(control))spin->setValue(values[field].toDouble());
                    else if(auto* combo=qobject_cast<QComboBox*>(control))combo->setCurrentIndex(values[field].toInt());
                    else if(auto* check=qobject_cast<QCheckBox*>(control))check->setChecked(values[field].toBool());
                    else{error="Unknown production parameter type";dialog->reject();return;}
                }
                for(auto it=values.begin();it!=values.end();++it)if(!used.contains(it.key())){error="Parameter not discovered: "+it.key();dialog->reject();return;}
                workflowArtifacts_[operation+"_fields"]=fields;
                if(!dialog->grab().save(workflowOutput_+"/views/"+operation+(accept?"":"-cancel")+"-dialog.png")){error="Cannot save dialog evidence";dialog->reject();return;}
                auto* buttons=dialog->findChild<QDialogButtonBox*>();auto* button=buttons?buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel):nullptr;
                if(!button){error="Missing confirmation role";dialog->reject();return;}handled=true;button->click();
            });
            timer.start(0);matches[0]->trigger();timer.stop();
            if(!handled)throw std::runtime_error(error.toStdString());
            if(accept)workflowArtifacts_[operation+"_input"]=values;
            return true;
        };
        auto chooseEdge=[&]{
            for(const auto& e:m->edges){const auto* a=m->point(e.a);const auto* b=m->point(e.b);if(!a||!b||!a->batchMovable||!b->batchMovable)continue;
                int incidence=0;for(const auto& f:m->faces)for(int i=0;i<f.points.size();++i)if((f.points[i]==e.a&&f.points[(i+1)%f.points.size()]==e.b)||(f.points[i]==e.b&&f.points[(i+1)%f.points.size()]==e.a))++incidence;
                if(incidence<=1){advancedEdge_=e.id;return e;}
            }throw std::runtime_error("Fixture needs an editable boundary edge");
        };
        switch(workflowStage_++){
        case 0:{
            captureWorkflow("original");editingOriginal_=m;
            for(const auto& f:m->faces){bool valid=true;for(auto id:f.points){auto* p=m->point(id);valid&=p&&p->batchMovable&&!p->locked;}auto* l=m->layer(f.layer);if(valid&&l&&l->visible){advancedFace_=f.id;break;}}
            if(!advancedFace_)throw std::runtime_error("Fixture needs an editable face");
            setSelectionKind(HullEntity::Face);selectEntities(HullEntity::Face,{advancedFace_});
            for(const auto& op:{"translate","scale","rotate","mirror"})if(!invoke(op,{},false))return;
            chooseEdge();setSelectionKind(HullEntity::Edge);selectEntities(HullEntity::Edge,{advancedEdge_});if(!invoke("extrude",{},false))return;
            workflowCase("cancel-all-five-dialogs-keeps-document",session_.model()==m&&!session_.busy());
            setSelectionKind(HullEntity::Face);selectEntities(HullEntity::Face,{advancedFace_});workflowArtifacts_["transformed_face"]=double(advancedFace_);
            invoke("translate",{{"x",.03125},{"y",.015625},{"z",.0625}});return;
        }
        case 1:captureWorkflow("translated");session_.undo();return;
        case 2:captureWorkflow("undo_translate");session_.redo();return;
        case 3:captureWorkflow("redo_translate");invoke("scale",{{"x",1.001},{"y",1.002},{"z",1.003}});return;
        case 4:captureWorkflow("scaled");invoke("rotate",{{"x",.05},{"y",.1},{"z",.025}});return;
        case 5:{
            captureWorkflow("rotated");editingChanged_=m;auto e=chooseEdge();setSelectionKind(HullEntity::Edge);selectEntities(HullEntity::Edge,{e.id});
            workflowArtifacts_["extruded_edge"]=double(e.id);auto a=m->point(e.a)->position,b=m->point(e.b)->position;
            const double distance=std::max(1.,m->draft)*.01;
            if(std::abs(b.x-a.x)+std::abs(b.y-a.y)>1e-8)invoke("extrude",{{"z",distance},{"layer",0}});
            else invoke("extrude",{{"x",distance},{"layer",0}});
            return;
        }
        case 6:{
            captureWorkflow("extruded");workflowCase("extrusion-adds-one-face",m->faces.size()==editingChanged_->faces.size()+1);
            fs::EntityId faceId=0;for(const auto& f:m->faces)if(std::none_of(editingChanged_->faces.begin(),editingChanged_->faces.end(),[&](auto& old){return old.id==f.id;}))faceId=f.id;
            if(!faceId)throw std::runtime_error("Extruded face missing");
            workflowArtifacts_["mirrored_face"]=double(faceId);
            setSelectionKind(HullEntity::Face);selectEntities(HullEntity::Face,{faceId});
            invoke("mirror",{{"axis",0},{"distance",m->maximum.x+std::max(1.,m->length)*.01},{"weld",false}});return;
        }
        case 7:captureWorkflow("mirrored");session_.undo();return;
        case 8:captureWorkflow("undo_mirror");session_.redo();return;
        case 9:captureWorkflow("redo_mirror");workflowArtifacts_["saved_file"]=workflowOutput_+"/advanced.fbm";session_.save(workflowOutput_+"/advanced.fbm");return;
        case 10:session_.load(workflowOutput_+"/advanced.fbm");return;
        case 11:
            captureWorkflow("reopened");grab().save(workflowOutput_+"/views/advanced-main-window.png");
            workflowCases_.append(QJsonObject{{"name","advanced_visual_parity"},{"status","review"},{"message","Actual dialogs and hidden main window captured; desktop input acceptance remains separate"}});finishWorkflow();return;
        }
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
