// User-authorized parallel task and draggable-ball regression observer.
// Invoked only by the existing --verify-hydro isolated desktop entry.
#include "studio_ui.h"
#include "main_window.h"
#include "hydro_dialog.h"
#include "performance_dialog.h"
#include <QApplication>
#include <QAction>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QSignalBlocker>
#include <QMessageBox>

void MainWindow::startCalculationTaskProbe(){
    struct State {
        int phase=0,maximum=0,prompts=0;QElapsedTimer timer;QStringList starts;
        QMap<QString,QPointer<QDialog>> dialogs;QMap<QString,QPointer<ModelSession>> workers;
        QMap<QString,bool> active;QJsonArray transitions;QPoint dragged;
    };
    auto state=std::make_shared<State>();state->timer.start();
    const QStringList keys{"hydro:cross","hydro:table","hydro:single","performance:holtrop"};
    auto* timer=new QTimer(this);timer->setInterval(15);
    connect(timer,&QTimer::timeout,this,[this,timer,state,keys]{
        const QSignalBlocker guard(timer);
        if(workflowTimer_.elapsed()>240000){timer->stop();finishWorkflow("Parallel calculation observer timed out");return;}
        auto action=[this](const QString& key)->QAction*{
            const QString operation=QString(key).replace(':','_');QList<QAction*> list;
            for(auto* a:findChildren<QAction*>())if(a->property("operation")==operation)list.append(a);
            return list.size()==1?list.first():nullptr;
        };
        auto ball=[this](const QString& key)->QAbstractButton*{
            for(auto* b:findChildren<QAbstractButton*>())if(b->property("calculation_task_key")==key&&b->property("task_running").isValid())return b;
            return nullptr;
        };
        auto button=[state](const QString& key,const QString& operation)->QPushButton*{
            const auto d=state->dialogs.value(key);if(!d)return nullptr;
            for(auto* b:d->findChildren<QPushButton*>())if(b->property(key.startsWith("hydro:")?"hydro_action":"performance_action")==operation)return b;
            return nullptr;
        };
        auto allIdle=[&]{for(const auto& worker:state->workers)if(worker&&worker->busy())return false;return !session_.busy();};
        if(state->phase==0){
            if(session_.busy())return;
            workflowCase("tasks.unsaved-model-edit",session_.updateProject({{"water_density",1.027}}));
            state->phase=1;return;
        }
        if(state->phase==1){
            if(session_.busy())return;
            workflowArtifacts_["origin_revision"]=double(session_.model()->revision);
            workflowArtifacts_["origin_document"]=session_.model()->documentId;
            for(const auto& key:keys){auto* a=action(key);if(!a||!a->isEnabled()){timer->stop();finishWorkflow("Calculation action absent or disabled: "+key);return;}a->trigger();}
            state->phase=2;return;
        }
        if(state->phase==2){
            for(auto* d:findChildren<QDialog*>()){
                const auto key=d->property("calculation_task_key").toString();if(!keys.contains(key))continue;
                auto* w=dynamic_cast<ModelSession*>(d->property("calculation_session").value<QObject*>());
                if(w){state->dialogs[key]=d;state->workers[key]=w;}
            }
            if(state->dialogs.size()!=keys.size())return;
            if(!allIdle())return;
            workflowCase("tasks.windows-nonmodal",!QApplication::activeModalWidget());
            for(const auto& key:keys){
                auto* d=state->dialogs[key].data();const auto fields=hydroRequest_["task_inputs"].toObject()[key].toObject();
                for(auto* w:d->findChildren<QWidget*>()){
                    auto field=w->property(key.startsWith("hydro:")?"field":"performance_field").toString();if(!fields.contains(field))continue;
                    const QSignalBlocker block(w);
                    if(auto* n=qobject_cast<QDoubleSpinBox*>(w)){n->setValue(fields[field].toDouble());if(key.startsWith("performance:"))n->setProperty("performance_exact_value",fields[field].toDouble());}
                    else if(auto* n=qobject_cast<QCheckBox*>(w))n->setChecked(fields[field].toBool());
                    else if(auto* n=qobject_cast<QComboBox*>(w))n->setCurrentIndex(fields[field].toInt());
                    else if(auto* n=qobject_cast<QLineEdit*>(w)){QStringList values;for(auto v:fields[field].toArray())values<<QString::number(v.toDouble(),'g',17);n->setText(values.join(','));}
                }
                // Batch assignment above blocks signals; complete the same edit
                // notification a user sends so input validity refreshes the button.
                for(auto* n:d->findChildren<QDoubleSpinBox*>())QMetaObject::invokeMethod(n,"editingFinished",Qt::DirectConnection);
                auto* worker=state->workers[key].data();const auto original=worker->onStateChanged;
                worker->onStateChanged=[state,key,original]{
                    if(original)original();int active=0,queued=0;
                    for(auto i=state->workers.cbegin();i!=state->workers.cend();++i)if(i.value()){
                        const bool running=i.value()->calculationActive();active+=running;queued+=i.value()->calculationQueued();
                        if(running&&!state->active[i.key()])state->starts.append(i.key());state->active[i.key()]=running;
                    }
                    state->maximum=std::max(state->maximum,active);
                    state->transitions.append(QJsonObject{{"key",key},{"ms",state->timer.elapsed()},{"active",active},{"queued",queued}});
                };
            }
            for(const auto& key:keys){auto* run=button(key,"calculate");if(!run){timer->stop();finishWorkflow("Calculate callback absent: "+key);return;}run->click();}
            workflowCase("tasks.two-running",state->workers[keys[0]]->calculationActive()&&state->workers[keys[1]]->calculationActive());
            workflowCase("tasks.remaining-queued",state->workers[keys[2]]->calculationQueued()&&state->workers[keys[3]]->calculationQueued());
            workflowCase("tasks.owner-not-busy",!session_.busy());
            for(const auto& key:keys){workflowCase("tasks.entry-enabled-"+key,action(key)&&action(key)->isEnabled());state->dialogs[key]->reject();}
            state->phase=3;state->timer.restart();return;
        }
        if(state->phase==3){
            if(state->timer.elapsed()<80)return;
            if(hydroRequest_.contains("close_tasks")){
                QTimer::singleShot(0,this,[]{if(auto* box=qobject_cast<QMessageBox*>(frontStudioDialog()))box->button(QMessageBox::Cancel)->click();});
                close();
                workflowCase("exit.decline-keeps-window",isVisible());
                workflowCase("exit.decline-keeps-running-and-queue",state->workers[keys[0]]->calculationActive()&&state->workers[keys[1]]->calculationActive()&&state->workers[keys[2]]->calculationQueued()&&state->workers[keys[3]]->calculationQueued());
                state->phase=20;state->timer.restart();
                connect(qApp,&QCoreApplication::aboutToQuit,this,[this,state,keys]{
                    workflowCase("exit.main-hidden",!isVisible());
                    workflowCase("exit.owner-idle",!session_.busy()&&!session_.recordingCalculationParameters());
                    for(const auto& key:keys)workflowCase("exit.task-stopped-"+key,state->workers[key]&&!state->workers[key]->busy());
                    workflowCase("exit.queue-never-started",state->starts==QStringList{keys[0],keys[1]});
                    workflowCase("exit.bounded-shutdown",state->timer.elapsed()<15000);
                    workflowArtifacts_["exit_ms"]=state->timer.elapsed();workflowArtifacts_["save_prompts"]=state->prompts;
                    workflowArtifacts_["saved_model"]=session_.sourcePath();finishWorkflow();
                });
                auto* choices=new QTimer(this);choices->setInterval(10);
                connect(choices,&QTimer::timeout,this,[this,state,choices]{
                    auto* box=qobject_cast<QMessageBox*>(frontStudioDialog());if(!box||box->property("exit_probe_answered").toBool())return;
                    box->setProperty("exit_probe_answered",true);
                    if(box->objectName()=="CalculationExitConfirmation"){
                        box->grab().save(workflowOutput_+"/views/exit-confirmation.png");
                        workflowCase("exit.cancel-default",box->defaultButton()==box->button(QMessageBox::Cancel));
                        box->button(QMessageBox::Yes)->click();
                    }else if(box->standardButtons().testFlag(QMessageBox::Save)){
                        ++state->prompts;const auto choice=hydroRequest_["close_tasks"].toString();
                        box->button(choice=="save"?QMessageBox::Save:choice=="discard"?QMessageBox::Discard:QMessageBox::Cancel)->click();choices->stop();
                    }
                });choices->start();close();return;
            }
            auto* tableBall=ball("hydro:table");auto* queuedBall=ball("performance:holtrop");
            workflowCase("tasks.table-has-ball",tableBall&&tableBall->isVisible());
            workflowCase("tasks.queue-has-ball",queuedBall&&queuedBall->isVisible());
            if(!tableBall||!queuedBall){timer->stop();finishWorkflow("Task ball missing");return;}
            const QPoint start=tableBall->pos(),local=tableBall->rect().center(),global=tableBall->mapToGlobal(local),delta(-120,70);
            auto send=[&](QEvent::Type type,QPoint globalPosition,Qt::MouseButton b,Qt::MouseButtons held){
                QMouseEvent e(type,tableBall->mapFromGlobal(globalPosition),globalPosition,b,held,Qt::NoModifier);QApplication::sendEvent(tableBall,&e);
            };
            send(QEvent::MouseButtonPress,global,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseMove,global+delta,Qt::NoButton,Qt::LeftButton);
            send(QEvent::MouseButtonRelease,global+delta,Qt::LeftButton,Qt::NoButton);
            state->dragged=tableBall->pos();
            workflowCase("tasks.ball-drags",state->dragged!=start);
            workflowCase("tasks.drag-does-not-open",!state->dialogs["hydro:table"]->isVisible());
            queuedBall->click();
            workflowCase("tasks.queue-reopens-original",state->dialogs["performance:holtrop"]->isVisible());
            auto* cancel=button("performance:holtrop","cancel");workflowCase("tasks.queue-cancel-enabled",cancel&&cancel->isEnabled());if(cancel)cancel->click();
            state->dialogs["performance:holtrop"]->reject();state->phase=4;state->timer.restart();return;
        }
        if(state->phase==20){
            if(hydroRequest_["close_tasks"]!="cancel_save"||state->prompts<1||!allIdle()||calculationExitPending_)return;
            workflowCase("exit.cancel-save-keeps-dirty-window",isVisible()&&session_.modified());
            workflowCase("exit.cancel-save-no-queued-work-started",state->starts==QStringList{keys[0],keys[1]});
            // A cancelled exit must leave the scheduler usable for the next job.
            state->dialogs[keys[2]]->show();button(keys[2],"calculate")->click();
            workflowCase("exit.cancel-save-can-restart",state->workers[keys[2]]->calculationActive());
            timer->stop();disconnect(qApp,&QCoreApplication::aboutToQuit,this,nullptr);finishWorkflow();return;
        }
        if(state->phase==4){
            if(state->workers["performance:holtrop"]->busy())return;
            workflowCase("tasks.queue-cancel-isolated",state->workers["hydro:cross"]->busy()&&state->workers["hydro:table"]->busy()&&state->workers["hydro:single"]->calculationQueued());
            workflowCase("tasks.drag-position-retained",ball("hydro:table")&&ball("hydro:table")->pos()==state->dragged);
            action("performance:holtrop")->trigger();button("performance:holtrop","calculate")->click();state->dialogs["performance:holtrop"]->reject();
            workflowCase("tasks.resubmit-queued",state->workers["performance:holtrop"]->calculationQueued());
            if(hydroRequest_["edit_and_cancel"].toBool()){
                workflowCase("tasks.edit-while-computing",session_.updateProject({{"water_density",1.03}}));
                action("hydro:cross")->trigger();button("hydro:cross","cancel")->click();state->dialogs["hydro:cross"]->reject();
            }
            grab().save(workflowOutput_+"/views/parallel-balls.png");state->phase=5;return;
        }
        if(state->phase==5){
            if(!allIdle())return;
            for(const auto& key:keys){const auto d=state->dialogs[key];if(!d||!d->property(key.startsWith("hydro:")?"hydro_finished":"performance_finished").toBool())return;}
            workflowCase("tasks.maximum-two",state->maximum==2);
            workflowCase("tasks.drag-position-survives-completion",ball("hydro:table")&&ball("hydro:table")->pos()==state->dragged);
            workflowCase("tasks.fifo",state->starts.indexOf("hydro:single")>=0&&state->starts.indexOf("performance:holtrop")>state->starts.indexOf("hydro:single"));
            workflowArtifacts_["transitions"]=state->transitions;workflowArtifacts_["start_order"]=QJsonArray::fromStringList(state->starts);
            QJsonObject results,files;
            for(const auto& key:keys){
                auto* d=state->dialogs[key].data();QJsonObject result;
                if(auto* h=dynamic_cast<HydroDialog*>(d))result=h->result();else if(auto* p=dynamic_cast<PerformanceDialog*>(d))result=p->result();
                if(key=="hydro:cross"&&hydroRequest_["edit_and_cancel"].toBool())workflowCase("tasks.running-cancel-isolated",d->property("hydro_cancelled").toBool());
                else workflowCase("tasks.result-retained-"+key,!result.isEmpty());
                results[key]=result;
                auto* b=ball(key);workflowCase("tasks.completion-ball-"+key,b&&b->isVisible()&&!b->property("task_running").toBool());
                if(b)b->click();workflowCase("tasks.reopen-complete-"+key,d->isVisible()&&(!b||!b->isVisible()));
                const auto filename=workflowOutput_+'/'+QString(key).replace(':','-')+".csv";QString error;
                if(!result.isEmpty()){
                    if(auto* h=dynamic_cast<HydroDialog*>(d))error=h->exportReport(filename,"csv");else error=static_cast<PerformanceDialog*>(d)->exportReport(filename,"csv");
                    workflowCase("tasks.export-"+key,error.isEmpty());files[key]=filename;
                }
                d->reject();
            }
            workflowArtifacts_["results"]=results;workflowArtifacts_["exports"]=files;
            workflowCase("tasks.save-owner",session_.save(workflowOutput_+"/after.fbm"));state->phase=6;return;
        }
        if(state->phase==6){
            if(session_.busy())return;
            workflowArtifacts_["saved_model"]=session_.sourcePath();workflowArtifacts_["final_revision"]=double(session_.model()->revision);
            if(!hydroRequest_["edit_and_cancel"].toBool()){
                auto* entry=action("performance:propeller2");if(!entry){timer->stop();finishWorkflow("Propulsion entry missing");return;}
                entry->trigger();state->phase=7;state->timer.restart();return;
            }
            timer->stop();finishWorkflow();return;
        }
        if(state->phase==7){
            auto* dialog=dynamic_cast<PerformanceDialog*>(calculationDialog("performance:propeller2"));
            auto* worker=calculationSession("performance:propeller2");
            if(!dialog||!worker||worker->busy()){
                if(state->timer.elapsed()>10000){timer->stop();finishWorkflow("Propulsion dependency did not open");}return;
            }
            workflowArtifacts_["propulsion_defaults"]=dialog->inputs();
            workflowCase("tasks.propulsion-reads-completed-resistance",dialog->inputs()["values"].toObject()["Edit16"].toDouble()>0);
            timer->stop();dialog->reject();finishWorkflow();return;
        }
    });timer->start();
}
