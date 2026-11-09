// Hidden production action observer; no native desktop automation.
#include "main_window.h"
#include "hydro_dialog.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QLayout>
#include <stdexcept>

void MainWindow::verifyHydro(const QString& request,const QString& output){
    workflow_=true;hydroWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read hydro workflow");return;}
    hydroRequest_=QJsonDocument::fromJson(f.readAll()).object();
    if(!QDir().mkpath(workflowOutput_+"/views")){finishWorkflow("Cannot create hydro evidence");return;}
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();
    openModel(hydroRequest_["model"].toString());
}
void MainWindow::hydroStep(const QString& operation){
    if(operation=="hydro_defaults"||operation=="hydrostatics"||operation=="hydro_settings")return;
    if(operation=="closed"){
        captureWorkflow("after");
        const auto before=quint64(workflowArtifacts_["before_revision"].toDouble());
        if(!workflowArtifacts_["entry_unavailable"].toBool())
            workflowCase("hydro.document-revision",session_.model()->revision==before+(hydroRequest_["save_settings"].toBool()?1:0));
        finishWorkflow();return;
    }
    if(operation!="load")return;
    captureWorkflow("original");workflowArtifacts_["before_revision"]=double(session_.model()->revision);
    const auto mode=hydroRequest_["inputs"].toObject()["mode"].toString();QList<QAction*> candidates;
    for(auto* a:findChildren<QAction*>())if(a->property("operation")=="hydro_"+mode)candidates.append(a);
    QJsonArray observed;for(auto* a:candidates)observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});workflowArtifacts_["action_candidates"]=observed;
    if(candidates.size()!=1||!candidates.first()->isEnabled()){
        workflowCases_.append(QJsonObject{{"name","hydro_entry"},{"status","not_run"},{"message","Action absent, ambiguous or disabled"}});finishWorkflow();return;
    }
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!hydroDialog_||!hydroDialog_->isVisible())return;
        auto findButton=[this](QString operation){QList<QPushButton*> found;for(auto* b:hydroDialog_->findChildren<QPushButton*>())if(b->property("hydro_action")==operation)found.append(b);return found.size()==1?found.first():nullptr;};
        auto unavailable=[this,timer](QString message){
            workflowArtifacts_["entry_unavailable"]=true;workflowCases_.append(QJsonObject{{"name","hydro_entry"},{"status","not_run"},{"message",message}});
            timer->stop();timer->deleteLater();hydroDialog_->reject();
        };
        if(*stage==0){
            const QFontInfo font(hydroDialog_->font());
            workflowArtifacts_["dialog_font"]=QJsonObject{{"requested",hydroDialog_->font().family()},{"resolved",font.family()},{"style",font.styleName()},{"exact_match",font.exactMatch()},{"point_size",font.pointSizeF()}};
            QJsonArray discovered;const auto values=hydroRequest_["inputs"].toObject();QSet<QString> used;
            for(auto* widget:hydroDialog_->findChildren<QWidget*>()){
                const auto key=widget->property("field").toString();if(key.isEmpty())continue;discovered.append(key);if(!values.contains(key))continue;used.insert(key);
                if(auto* n=qobject_cast<QDoubleSpinBox*>(widget))n->setValue(values[key].toDouble());
                else if(auto* n=qobject_cast<QCheckBox*>(widget))n->setChecked(values[key].toBool());
                else if(auto* n=qobject_cast<QLineEdit*>(widget)){QStringList parts;for(auto v:values[key].toArray())parts<<QString::number(v.toDouble(),'g',17);n->setText(parts.join(","));}
            }
            workflowArtifacts_["discovered_fields"]=discovered;
            for(auto it=values.begin();it!=values.end();++it)if(it.key()!="mode"&&!used.contains(it.key())){unavailable("Hydro input not discovered: "+it.key());return;}
            if(!hydroDialog_->grab().save(workflowOutput_+"/views/parameters.png")){timer->stop();hydroDialog_->reject();finishWorkflow("Cannot save parameters image");return;}
            auto* run=findButton("calculate");if(!run){unavailable("Calculate callback unavailable or ambiguous");return;}
            *stage=1;run->click();
            if(hydroRequest_["cancel"].toBool()){auto* stop=findButton("cancel");if(stop&&stop->isEnabled())stop->click();else workflowCase("cancel-action",false);}
            return;
        }
        if(!hydroDialog_->property("hydro_finished").toBool()||session_.busy())return;
        auto resultHash=[this]{return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(hydroDialog_->result()).toJson(),QCryptographicHash::Sha256).toHex());};
        if(*stage==2||*stage==3){
            if(*stage==3){
                workflowCase("cancel-preserves-complete-result",resultHash()==workflowArtifacts_["result_before_cancel"].toString());
                workflowCase("repeat-cancel-reported",hydroDialog_->property("hydro_cancelled").toBool()||!hydroDialog_->property("hydro_error").toString().isEmpty());
            }
            timer->stop();timer->deleteLater();hydroDialog_->reject();return;
        }
        workflowArtifacts_["error"]=hydroDialog_->property("hydro_error").toString();workflowArtifacts_["cancelled"]=hydroDialog_->property("hydro_cancelled").toBool();
        if(!hydroRequest_["cancel"].toBool()){
            workflowCase("calculation-delivered",!hydroDialog_->result().isEmpty());
            if(hydroRequest_["export_auxiliary"].toBool()){
                auto* save=findButton("save_auxiliary");
                if(!save){unavailable("Auxiliary save callback unavailable or ambiguous");return;}
                QJsonArray artifacts;const auto files=hydroDialog_->result()["auxiliary"].toObject()["files"].toArray();
                workflowCase("auxiliary-count",hydroDialog_->auxiliaryCount()==hydroRequest_["expected_auxiliary_count"].toInt(4));
                for(int i=0;i<files.size();++i){
                    workflowCase("auxiliary-select-"+QString::number(i),hydroDialog_->selectAuxiliary(i));
                    const auto path=workflowOutput_+"/"+files[i].toObject()["name"].toString();
                    save->setProperty("probe_save_path",path);save->click();QFile file(path);
                    workflowCase("auxiliary-save-"+QString::number(i),save->property("export_error").toString().isEmpty()&&file.open(QIODevice::ReadOnly)&&file.readAll()==files[i].toObject()["text"].toString().toUtf8());
                    artifacts.append(path);workflowCase("auxiliary-image-"+QString::number(i),hydroDialog_->grab().save(workflowOutput_+"/views/auxiliary-"+QString::number(i)+".png"));
                }
                save->setProperty("probe_save_path",QString{});workflowArtifacts_["auxiliary_files"]=artifacts;
                const auto invalidPath=workflowOutput_+"/absent-parent/report.txt";
                workflowCase("auxiliary-failed-save",!hydroDialog_->exportAuxiliary(invalidPath,0).isEmpty());
                workflowCase("auxiliary-invalid-index",!hydroDialog_->exportAuxiliary(workflowOutput_+"/invalid.txt",-1).isEmpty()&&!QFileInfo::exists(workflowOutput_+"/invalid.txt"));
            }
            for(const auto& format:{QString("json"),QString("csv"),QString("txt"),QString("pdf")}){
                const auto path=workflowOutput_+"/hydro-report."+format;auto error=hydroDialog_->exportReport(path,format);workflowCase("export-"+format,error.isEmpty(),error);workflowArtifacts_["report_"+format]=path;
            }
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);workflowCase("image-"+QString::number(i),hydroDialog_->grab().save(workflowOutput_+"/views/results-"+QString::number(i)+".png"));}
        }else workflowCase("cancel-reported",hydroDialog_->property("hydro_cancelled").toBool()||!hydroDialog_->property("hydro_error").toString().isEmpty());
        if(hydroRequest_["repeat_cancel"].toBool()){
            auto* run=findButton("calculate");auto* cancel=findButton("cancel");
            if(run&&cancel){workflowArtifacts_["result_before_cancel"]=resultHash();*stage=3;run->click();cancel->click();return;}
            unavailable("Repeated cancellation callback unavailable");return;
        }
        if(hydroRequest_["save_settings"].toBool()){
            auto* save=findButton("save_settings");
            if(save&&save->isVisible()&&save->isEnabled()){*stage=2;save->click();return;}
            unavailable("Save settings callback unavailable");return;
        }
        timer->stop();timer->deleteLater();
        hydroDialog_->reject();
    });
    timer->start();candidates.first()->trigger();
}
