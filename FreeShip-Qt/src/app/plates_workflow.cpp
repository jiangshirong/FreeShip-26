// Persistent hidden production workflow; native desktop acceptance is separate.
#include "main_window.h"
#include "plates_dialog.h"
#include <QAction>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsSimpleTextItem>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLayout>
#include <QThread>
void MainWindow::verifyPlates(const QString& request,const QString& output){
    workflow_=true;platesWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read plate request");return;}
    platesRequest_=QJsonDocument::fromJson(f.readAll()).object();QDir().mkpath(workflowOutput_+"/views");
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();openModel(platesRequest_["model"].toString());
}
void MainWindow::platesStep(const QString& operation){
    if(operation=="closed"){captureWorkflow("after");workflowCase("plates.readonly",!session_.modified()&&!session_.canUndo()&&double(session_.model()->revision)==workflowArtifacts_["before_revision"].toDouble());finishWorkflow();return;}
    if(operation!="load")return;
    captureWorkflow("original");workflowArtifacts_["before_revision"]=double(session_.model()->revision);
    QList<QAction*> candidates;for(auto*a:findChildren<QAction*>())if(a->property("operation")=="develop_plates")candidates<<a;
    if(candidates.size()!=1||!candidates[0]->isEnabled()){workflowCases_.append(QJsonObject{{"name","plate-entry"},{"status","not_run"}});finishWorkflow();return;}
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!platesDialog_||session_.busy())return;
        auto button=[this](QString name){QList<QPushButton*> found;for(auto*b:platesDialog_->findChildren<QPushButton*>())if(b->property("plates_action")==name)found<<b;return found.size()==1?found[0]:nullptr;};
        auto end=[this,timer]{timer->stop();timer->deleteLater();platesDialog_->reject();};
        if(*stage==0){auto*b=button("calculate");if(!b){workflowCases_.append(QJsonObject{{"name","plate-calculate-entry"},{"status","not_run"}});end();return;}*stage=1;b->click();return;}
        if(*stage==2){workflowCase("plates.cancel-preserves-result",!platesDialog_->failure().isEmpty()&&platesDialog_->result()==workflowArtifacts_["plate_data"].toObject());end();return;}
        if(!platesDialog_->failure().isEmpty()){workflowCase("plates.expected-failure",platesRequest_["expect_failure"].toBool(),platesDialog_->failure());end();return;}
        if(!platesDialog_->hasResult())return;
        workflowCase("plates.generated",!platesRequest_["expect_failure"].toBool());workflowArtifacts_["raw_plate_data"]=platesDialog_->result();
        for(auto*n:platesDialog_->findChildren<QDoubleSpinBox*>()){
            const auto key=n->property("plate_field").toString();if(key=="rotation")n->setValue(37);else if(key=="x")n->setValue(1.25);else if(key=="y")n->setValue(-2.5);
        }
        for(auto*c:platesDialog_->findChildren<QCheckBox*>())if(c->property("plate_field")=="mirror")c->setChecked(true);
        workflowArtifacts_["plate_data"]=platesDialog_->result();
        workflowCase("plates.transform",platesDialog_->result()["layout"].toArray()[0].toObject()["rotation"]==37&&platesDialog_->result()["layout"].toArray()[0].toObject()["mirrored"].toBool());
        auto before=platesDialog_->result();for(auto*v:platesDialog_->findChildren<QGraphicsView*>()){v->scale(15,15);v->translate(350,-450);}
        workflowCase("plates.camera-readonly",platesDialog_->result()==before);
        button("fit")->click();platesDialog_->grab().save(workflowOutput_+"/views/plates.png");
        bool outsideReadable=true;
        for(auto*v:platesDialog_->findChildren<QGraphicsView*>())for(auto*i:v->scene()->items())if(auto*label=dynamic_cast<QGraphicsSimpleTextItem*>(i))
            if(label->parentItem()&&!label->parentItem()->shape().contains(label->pos()))outsideReadable&=label->brush().color().lightness()<128;
        workflowCase("plates.labels-outside-fill-readable",outsideReadable);
        for(const auto* format:{"dxf","txt","json","png","pdf"}){auto path=workflowOutput_+"/plates."+format;auto error=platesDialog_->exportFile(path,format);workflowCase("plates.export."+QString(format),error.isEmpty()&&QFileInfo(path).size()>0,error);}
        // Printing at a declared physical scale is checked independently by the
        // PDF reader, including feet-to-mm conversion and page tiling.
        for(auto*n:platesDialog_->findChildren<QDoubleSpinBox*>())if(n->property("plate_field")=="print_scale"){
            n->setValue(100);workflowCase("plates.scaled-pdf",platesDialog_->exportFile(workflowOutput_+"/scaled.pdf","pdf").isEmpty());
            n->setValue(10);workflowCase("plates.tiled-pdf",platesDialog_->exportFile(workflowOutput_+"/tiled.pdf","pdf").isEmpty());
            n->setValue(.001);auto path=workflowOutput_+"/failed-print.pdf";QFile keep(path);keep.open(QIODevice::WriteOnly);keep.write("previous");keep.close();
            auto error=platesDialog_->exportFile(path,"pdf");keep.open(QIODevice::ReadOnly);workflowCase("plates.excess-pages-preserve-file",!error.isEmpty()&&keep.readAll()=="previous");n->setValue(0);
        }
        auto lists=platesDialog_->findChildren<QListWidget*>();if(lists.size()==1){
            auto*list=lists[0];list->item(0)->setCheckState(Qt::Unchecked);workflowArtifacts_["hidden_plate_data"]=platesDialog_->result();
            const auto error=platesDialog_->exportFile(workflowOutput_+"/hidden.dxf","dxf");
            workflowCase("plates.hidden-export",list->count()==1?!error.isEmpty():error.isEmpty());
            list->item(0)->setCheckState(Qt::Checked);
        }
        auto protectedPath=workflowOutput_+"/protected.txt";QFile f(protectedPath);f.open(QIODevice::WriteOnly);f.write("keep");f.close();auto error=platesDialog_->exportFile(protectedPath,"bad");f.open(QIODevice::ReadOnly);workflowCase("plates.failed-export-preserves-file",!error.isEmpty()&&f.readAll()=="keep");f.close();
        if(platesRequest_["cancel"].toBool()){
            auto*run=button("calculate");auto*stop=button("cancel");
            if(run&&stop){
                // Diagnostic workflow only: hold Qt dispatch until the real
                // worker publishes its result, then cancel the pending UI
                // request. This makes the late-success race reproducible.
                const QDir workspace(QFileInfo(QFileInfo(session_.modelPath()).absolutePath()).absolutePath());
                const auto previous=workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot);
                *stage=2;run->click();
                if(platesRequest_["late_cancel"].toBool()){
                    QElapsedTimer deadline;deadline.start();bool produced=false;
                    while(!produced && deadline.elapsed()<3000){
                        for(const auto &name:workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot))
                            if(!previous.contains(name) && QFileInfo::exists(workspace.filePath(name+"/plates.json")))produced=true;
                        if(!produced)QThread::msleep(1);
                    }
                    workflowCase("plates.late-result-produced-before-cancel",produced && session_.busy());
                }
                stop->click();return;
            }
        }
        end();
    });timer->start();candidates[0]->trigger();
}
