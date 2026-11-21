#include "core/loading_cases.h"
// Hidden production action observer; no native desktop automation.
#include "main_window.h"
#include "hydro_dialog.h"
#include "hydro_number_list.h"
#include "calculation_plot.h"
#include <QAbstractAnimation>
#include <QToolTip>
#include "exchange_dialog.h"
#include "performance_dialog.h"
#include "plates_dialog.h"
#include "report_message_probe.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "core/cad_export.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QStackedWidget>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>
#include <QTextBrowser>
#include "report_navigation_probe.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextTable>
#include <QToolBar>
#include <QToolButton>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QScrollArea>
#include <QWheelEvent>
#include <QMouseEvent>
#include "math_notation.h"
#include <cmath>
#include <QLayout>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QKeyEvent>
#include <QEventLoop>
#include <stdexcept>

namespace {
void draftKey(QWidget* window,QWidget* focus,int key,Qt::KeyboardModifiers modifiers){
    QApplication::setActiveWindow(window);focus->setFocus();
    const QPointer<QWidget> receiver(focus);
    QKeyEvent press(QEvent::KeyPress,key,modifiers);
    QApplication::sendEvent(focus,&press);
    for(int releaseKey:{key,int(Qt::Key_Control),int(Qt::Key_Shift)}){
        QKeyEvent release(QEvent::KeyRelease,releaseKey,Qt::NoModifier);
        QApplication::sendEvent(receiver?receiver.data():window,&release);
    }
}
QJsonObject tableRange(const QJsonObject& inputs){
    QJsonObject fields;
    for(auto key:{"start","end","step","trim"})fields[key]=inputs[key];
    return fields;
}
QJsonObject iconObservation(const QIcon& icon){
    const auto bitmap=icon.pixmap(QSize(44,24),1.25).toImage();
    int green=0;
    for(int y=0;y<bitmap.height();++y)for(int x=0;x<bitmap.width();++x){
        const auto color=bitmap.pixelColor(x,y);
        if(color==QColor("#006447"))++green;
    }
    return {{"icon",icon.name()},{"green_pixels",green}};
}
QJsonObject buttonObservation(const QAbstractButton* button){
    auto evidence=iconObservation(button->icon());
    evidence["text"]=button->text();evidence["accessible"]=button->accessibleName();
    evidence["tool_tip"]=button->toolTip();return evidence;
}
}

void MainWindow::verifyHydro(const QString& request,const QString& output){
    workflow_=true;hydroWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read hydro workflow");return;}
    hydroRequest_=QJsonDocument::fromJson(f.readAll()).object();
    if(hydroRequest_["language"].isString()){
        const auto error=setUiLanguage(hydroRequest_["language"].toString());
        if(!error.isEmpty()){finishWorkflow(error);return;}
    }
    if(!QDir().mkpath(workflowOutput_+"/views")){finishWorkflow("Cannot create hydro evidence");return;}
    if(hydroRequest_["observe_tab_clicks"].toBool()||hydroRequest_["background_task"].toBool()||hydroRequest_["parallel_tasks"].toBool()){
        setAttribute(Qt::WA_DontShowOnScreen);
        showMaximized();
    }
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();
    openModel(hydroRequest_["model"].toString());
}
void MainWindow::hydroStep(const QString& operation){
    if(operation=="hydro_defaults"||operation=="hydrostatics"||operation=="hydro_settings")return;
    if(operation=="closed"){
        if(hydroRequest_["background_task"].toBool()||hydroRequest_["parallel_tasks"].toBool())return;
        captureWorkflow("after");
        const auto before=quint64(workflowArtifacts_["before_revision"].toDouble());
        const auto after=session_.model()->revision;
        if(!workflowArtifacts_["entry_unavailable"].toBool())
            workflowCase("hydro.document-revision",after==before||
                         (workflowArtifacts_["table_range_recorded"].toBool()&&after==before+1));
        finishWorkflow();return;
    }
    if(operation!="load")return;
    if(hydroRequest_["parallel_tasks"].toBool()){startCalculationTaskProbe();return;}
    captureWorkflow("original");workflowArtifacts_["before_revision"]=double(session_.model()->revision);
    const auto mode=hydroRequest_["inputs"].toObject()["mode"].toString();QList<QAction*> candidates;
    for(auto* a:findChildren<QAction*>())if(a->property("operation")=="hydro_"+mode)candidates.append(a);
    QJsonArray observed;for(auto* a:candidates)observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});workflowArtifacts_["action_candidates"]=observed;
    if(candidates.size()!=1||!candidates.first()->isEnabled()){
        workflowCases_.append(QJsonObject{{"name","hydro_entry"},{"status","not_run"},{"message","Action absent, ambiguous or disabled"}});finishWorkflow();return;
    }
    if(hydroRequest_["background_task"].toBool()){
        // Observe the actual production calculation/close/reopen callbacks on
        // the isolated test desktop. No replacement solver or simulated task.
        auto* timer=new QTimer(this);timer->setInterval(20);
        struct Observation {int stage=0,progress=0;QPointer<HydroDialog> dialog;QElapsedTimer elapsed;QJsonObject first;QByteArray ballFrame;bool sawFlight=false,spinnerChanged=false;};
        auto state=std::make_shared<Observation>();state->elapsed.start();
        connect(timer,&QTimer::timeout,this,[this,timer,state]{
            const QSignalBlocker guard(timer);
            if(workflowTimer_.elapsed()>180000){timer->stop();finishWorkflow("Background stability observer timed out");return;}
            auto* job=calculationSession("hydro:cross");if(!job)return;
            auto* d=state->dialog.data();
            auto* ball=findChild<QAbstractButton*>("HydroTaskBall");
            auto button=[&](const QString& action)->QPushButton*{
                if(!d)return nullptr;QList<QPushButton*> found;
                for(auto* b:d->findChildren<QPushButton*>())if(b->property("hydro_action")==action)found.append(b);
                return found.size()==1?found.first():nullptr;
            };
            auto observe=[&](const QString& label){
                QJsonObject item{{"stage",label},{"busy",job->busy()},{"progress",job->hydroProgress()},
                    {"dialog_visible",d&&d->isVisible()},{"same_dialog",d&&d==stabilityDialog_},
                    {"ball_visible",ball&&ball->isVisible()}};
                if(ball){for(auto* key:{"task_running","task_unread","task_failed","task_docking"})item[key]=QJsonValue::fromVariant(ball->property(key));
                    item["ball_x"]=ball->x();item["ball_y"]=ball->y();}
                auto list=workflowArtifacts_["background_states"].toArray();list.append(item);workflowArtifacts_["background_states"]=list;
            };
            auto laterOpen=[this]{QTimer::singleShot(0,this,[this]{if(auto* b=findChild<QAbstractButton*>("HydroTaskBall"))b->click();});};
            if(state->stage==7){
                if(session_.busy())return;
                workflowCase("background.model-change-labels-old-result",stabilityDialog_&&!stabilityDialog_->property("calculation_snapshot_current").toBool()&&job->model()->revision!=session_.model()->revision);
                timer->stop();finishWorkflow();return;
            }
            if(state->stage==0){
                if(!hydroDialog_||!hydroDialog_->isVisible())return;
                state->dialog=hydroDialog_;d=state->dialog;
                const auto values=hydroRequest_["inputs"].toObject();
                for(auto* w:d->findChildren<QWidget*>()){
                    auto key=w->property("field").toString();if(!values.contains(key))continue;
                    if(auto* n=qobject_cast<QDoubleSpinBox*>(w))n->setValue(values[key].toDouble());
                    else if(auto* n=qobject_cast<QCheckBox*>(w))n->setChecked(values[key].toBool());
                    else if(auto* n=dynamic_cast<HydroNumberList*>(w))n->setValues(values[key]);
                }
                if(!button("calculate")){timer->stop();finishWorkflow("Calculate callback missing");return;}
                state->stage=1;button("calculate")->click();return;
            }
            if(!d){timer->stop();finishWorkflow("Stability task dialog lost");return;}
            if(state->stage==1){
                const auto progress=job->hydroProgress();
                if(progress["completed"].toInt()<1)return;
                workflowCase("background.close-during-active-task",job->hydroRunning());
                state->progress=progress["completed"].toInt();state->stage=2;state->elapsed.restart();
                d->reject();return;
            }
            if(state->stage==2){
                if(ball&&ball->isVisible()){
                    const auto frame=ball->grab().toImage();
                    const auto hash=QCryptographicHash::hash(QByteArray::fromRawData(reinterpret_cast<const char*>(frame.constBits()),frame.sizeInBytes()),QCryptographicHash::Sha256);
                    if(state->ballFrame.isEmpty())state->ballFrame=hash;else if(state->ballFrame!=hash)state->spinnerChanged=true;
                    state->sawFlight|=ball->property("task_docking").toBool();
                }
                if(state->elapsed.elapsed()<650)return;
                observe("hidden-running");
                workflowCase("background.close-keeps-solver-running",job->hydroRunning()&&!d->isVisible()&&d==stabilityDialog_);
                workflowCase("background.ball-running",ball&&ball->isVisible()&&ball->property("task_running").toBool());
                workflowCase("background.ball-docked-right",ball&&ball->x()>=width()-80&&!ball->property("task_docking").toBool());
                workflowCase("background.ball-animated",state->spinnerChanged);
                workflowArtifacts_["rolling_observed"]=state->sawFlight;
                workflowCase("background.progress-continues-hidden",job->hydroProgress()["completed"].toInt()>state->progress);
                grab().save(workflowOutput_+"/views/background-running.png");
                state->progress=job->hydroProgress()["completed"].toInt();state->stage=3;laterOpen();return;
            }
            if(state->stage==3){
                if(!d->isVisible())return;
                observe("reopened-running");
                workflowCase("background.reopen-same-job",d==hydroDialog_&&job->hydroRunning()&&job->hydroProgress()["completed"].toInt()>=state->progress);
                workflowCase("background.reopen-shows-progress",d->property("hydro_progress").toJsonObject()==job->hydroProgress());
                workflowCase("background.no-second-calculation",button("calculate")&&!button("calculate")->isEnabled());
                if(hydroRequest_["cancel"].toBool()){
                    workflowCase("background.explicit-cancel-enabled",button("cancel")&&button("cancel")->isEnabled());
                    if(button("cancel"))button("cancel")->click();
                }
                state->stage=4;d->reject();return;
            }
            if(state->stage==4){
                if(job->busy()||!d->property("hydro_finished").toBool())return;
                observe("hidden-finished");
                workflowCase("background.finished-notification",ball&&ball->isVisible()&&!ball->property("task_running").toBool()&&ball->property("task_unread").toBool());
                if(hydroRequest_["cancel"].toBool())workflowCase("background.only-explicit-cancel-stops",d->property("hydro_cancelled").toBool());
                else {const auto inputs=hydroRequest_["inputs"].toObject();workflowCase("background.complete-results-retained",d->result()["rows"].toArray().size()==inputs["angles"].toArray().size()*inputs["displacements"].toArray().size());}
                state->first=d->result();grab().save(workflowOutput_+"/views/background-finished.png");
                state->stage=5;laterOpen();return;
            }
            if(state->stage==5){
                if(!d->isVisible())return;
                observe("reopened-finished");
                workflowCase("background.read-clears-notification",!ball||!ball->isVisible());
                workflowCase("background.reopen-preserves-result",d->result()==state->first&&!job->busy());
                d->grab().save(workflowOutput_+"/views/background-result.png");
                if(hydroRequest_["cancel"].toBool()){timer->stop();d->reject();finishWorkflow();return;}
                workflowArtifacts_["first_result"]=state->first;
                workflowCase("background.export-before",d->exportReport(workflowOutput_+"/before.csv","csv").isEmpty());
                workflowArtifacts_["csv_before"]=workflowOutput_+"/before.csv";
                state->stage=6;button("calculate")->click();return;
            }
            if(state->stage==6){
                if(job->busy()||!d->property("hydro_finished").toBool())return;
                workflowArtifacts_["repeat_result"]=d->result();
                workflowCase("background.repeat-cached",d->result()["result_cache_hit"].toBool()&&d->result()["volume_evaluations"].toInt(-1)==0);
                workflowCase("background.export-after",d->exportReport(workflowOutput_+"/after.csv","csv").isEmpty());
                workflowArtifacts_["csv_after"]=workflowOutput_+"/after.csv";
                state->stage=7;d->reject();
                workflowCase("background.model-change-started",session_.updateProject({{"water_density",1.03}}));return;
            }
        });
        timer->start();candidates.first()->trigger();return;
    }
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!hydroDialog_||!hydroDialog_->isVisible())return;
        const QSignalBlocker callbackGuard(timer);
        auto settleLayout=[](){
            // Corner notices reparent, update their size hint and invalidate
            // QTabWidget in successive queued turns. Drain that bounded chain,
            // rather than recording an old rectangle after only the first turn.
            for(int pass=0;pass<6;++pass){
                QApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        };
        auto captureMessages=[this](const QString& state){
            hydroDialog_->layout()->activate();
            QJsonArray messages;
            for(const auto* label:hydroDialog_->findChildren<QLabel*>()){
                const auto role=label->property("studio_message_role").toString();
                if(role.isEmpty())continue;
                const auto box=label->geometry();
                messages.append(QJsonObject{{"role",role},{"text",label->text()},{"object",label->objectName()},
                    {"visible",label->isVisibleTo(hydroDialog_)},{"working",label->property("studio_message_busy").toBool()},
                    {"foreground",label->property("studio_message_foreground").toString()},
                    {"background",label->property("studio_message_background").toString()},
                    {"width",box.width()},{"height",box.height()},{"required_height",label->heightForWidth(box.width())},
                    {"height_for_width",label->hasHeightForWidth()},{"preferred_height",label->sizeHint().height()}});
            }
            auto states=workflowArtifacts_["hydro_messages"].toArray();
            QJsonArray pages;
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>()){
                QJsonArray names;for(int i=0;i<tabs->count();++i)names.append(QJsonObject{{"title",tabs->tabText(i)},{"visible",tabs->isTabVisible(i)}});
                pages.append(names);
            }
            states.append(QJsonObject{{"state",state},{"messages",messages},{"pages",pages}});
            workflowArtifacts_["hydro_messages"]=states;
        };
        auto findButton=[this](QString operation){QList<QPushButton*> found;for(auto* b:hydroDialog_->findChildren<QPushButton*>())if(b->property("hydro_action")==operation)found.append(b);return found.size()==1?found.first():nullptr;};
        if(*stage==4){
            if(session_.busy())return;
            timer->stop();timer->deleteLater();hydroDialog_->reject();return;
        }
        auto unavailable=[this,timer](QString message){
            workflowArtifacts_["entry_unavailable"]=true;workflowCases_.append(QJsonObject{{"name","hydro_entry"},{"status","not_run"},{"message",message}});
            timer->stop();timer->deleteLater();hydroDialog_->reject();
        };
        auto runHydro=[&]{
            auto* run=findButton("calculate");if(!run){unavailable("Calculate callback unavailable or ambiguous");return;}
            *stage=1;run->click();
            captureMessages("running");
            if(hydroRequest_["cancel"].toBool()){auto* stop=findButton("cancel");if(stop&&stop->isEnabled())stop->click();else workflowCase("cancel-action",false);}
        };
        auto* parameterTabs=hydroDialog_->findChild<QTabWidget*>();
        const auto pageByKey=[&](const QString& page)->QWidget*{
            if(!parameterTabs)return nullptr;
            for(int i=0;i<parameterTabs->count();++i)
                if(parameterTabs->widget(i)->property("calculation_page").toString()==page)return parameterTabs->widget(i);
            return nullptr;
        };
        const auto pageFor=[&](bool named){return pageByKey(named?"loading_parameters":"general");};
        const QStringList outputPages={"results","curve","report","auxiliary","comparison"};
        const auto selectPage=[&](QWidget* page){
            if(!page||parameterTabs->currentWidget()==page)return;
            auto* bar=parameterTabs->tabBar();
            const int index=parameterTabs->indexOf(page);
            if(index>=2)if(auto* animation=bar->findChild<QAbstractAnimation*>("HydroLoadingTabMotion"))
                if(animation->state()==QAbstractAnimation::Running)animation->setCurrentTime(animation->duration());
            const auto point=bar->tabRect(index).center();
            QMouseEvent press(QEvent::MouseButtonPress,point,bar->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease,point,bar->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(bar,&press);QApplication::sendEvent(bar,&release);
            settleLayout();
        };
        const auto selectNamed=[&](bool named){selectPage(pageFor(named));};
        const auto criteriaPanel=[&]{return hydroDialog_->findChild<QWidget*>("HydroCriteriaPanel");};
        const auto criteriaBesideInputs=[&](bool named){
            auto* page=pageFor(named);
            auto* editor=criteriaPanel();
            QWidget* input=nullptr;
            if(page)for(auto* child:page->findChildren<QWidget*>())
                if(child->property("field")=="trim"){input=child;break;}
            return page&&editor&&input&&input->property("field")=="trim"&&page->isAncestorOf(editor)&&
                editor->isVisibleTo(hydroDialog_)&&
                editor->mapTo(page,QPoint{}).x()>input->mapTo(page,input->rect().topRight()).x();
        };
        const auto editorPage=[&](const char* key){
            selectNamed(true);
            auto* page=pageByKey(key);
            selectPage(page);
            return page;
        };
        const auto removeInNameCell=[](QAbstractItemView* grid,int row)->QToolButton*{
            const auto cell=grid->visualRect(grid->model()->index(row,0));
            for(auto* button:grid->viewport()->findChildren<QToolButton*>("LoadBookRemoveRow")){
                if(button->isVisibleTo(grid)&&cell.contains(button->geometry())&&
                    grid->viewport()->childAt(button->geometry().center())==button)return button;
            }
            return nullptr;
        };
        if(*stage==0){
            workflowArtifacts_["initial_inputs"]=hydroDialog_->savedInputs();
            workflowCase("hydro-empty-output-tabs-disabled",parameterTabs&&
                std::none_of(outputPages.cbegin(),outputPages.cend(),[&](const auto& key){
                    auto* page=pageByKey(key);
                    return page&&parameterTabs->isTabEnabled(parameterTabs->indexOf(page));
                }));
            if(hydroRequest_["exercise_manual_criterion"].toBool()){
                selectNamed(false);
                auto* page=criteriaPanel();
                auto* grid=page?page->findChild<QTableWidget*>("load_criteria"):nullptr;
                auto* add=page?page->findChild<QToolButton*>("load_add_criteria"):nullptr;
                if(!grid||!add||grid->rowCount()!=0||!criteriaBesideInputs(false)){
                    unavailable("Missing standalone criterion editor");return;
                }
                workflowCase("criterion-panel-has-no-apply-button",!page->findChild<QPushButton*>("LoadBookApply"));
                add->click();
                const auto enter=[&](const QString& text){
                    for(auto* editor:grid->findChildren<QLineEdit*>())if(editor->isVisibleTo(grid)){
                        editor->setText(text);
                        QKeyEvent key(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                        QApplication::sendEvent(editor,&key);
                        settleLayout();
                        return true;
                    }
                    return false;
                };
                workflowCase("blank-criterion-row-is-ignored",grid->rowCount()==1&&
                    grid->item(0,0)->text().isEmpty()&&grid->item(0,1)->text().isEmpty()&&
                    !hydroDialog_->savedInputs().contains("gz_criteria"));
                const bool angleEntered=enter("10");
                auto* criteriaWarning=page->findChild<QLabel*>("HydroCriteriaWarning");
                auto* calculate=findButton("calculate");
                workflowCase("partial-criterion-shows-reason-and-blocks-calculation",angleEntered&&criteriaWarning&&calculate&&
                    criteriaWarning->text()==uiText("请填写横倾角和最低复原力臂。")&&!calculate->isEnabled());
                grid->setCurrentCell(0,1);grid->editItem(grid->item(0,1));
                const bool armEntered=enter("0.2");
                const auto saved=hydroDialog_->savedInputs();
                workflowCase("criterion-applies-without-any-loads",armEntered&&!saved.contains("loading_book")
                    &&!saved["use_loading_book"].toBool()&&saved["gz_criteria"]==QJsonArray{QJsonArray{10,.2}});
                auto* remove=removeInNameCell(grid,0);
                if(remove)remove->click();
                const bool cleared=remove&&!hydroDialog_->savedInputs().contains("gz_criteria");
                draftKey(hydroDialog_,grid,Qt::Key_Z,Qt::ControlModifier);
                workflowCase("last-criterion-removal-and-undo-update-active-criteria",cleared&&
                    hydroDialog_->savedInputs()["gz_criteria"]==saved["gz_criteria"]);
                selectNamed(true);
                if(auto* motion=parameterTabs->tabBar()->findChild<QAbstractAnimation*>("HydroLoadingTabMotion"))
                    if(motion->state()==QAbstractAnimation::Running)motion->setCurrentTime(motion->duration());
                auto* notice=hydroDialog_->findChild<QLabel*>("HydroLoadingNotice");
                auto* run=findButton("calculate");
                for(auto* motion:notice?notice->findChildren<QAbstractAnimation*>():QList<QAbstractAnimation*>{})
                    if(motion->state()==QAbstractAnimation::Running)motion->setCurrentTime(motion->duration());
                settleLayout();
                workflowCase("criteria-moves-to-named-parameters-without-losing-entry",criteriaBesideInputs(true)&&
                    criteriaPanel()==page&&grid->rowCount()==1&&grid->item(0,1)->data(Qt::EditRole).toDouble()==.2);
                workflowCase("missing-load-cases-warning-below-controls",notice&&run&&notice->isVisibleTo(hydroDialog_)&&
                    notice->text()==uiText("请先编辑装载工况。")&&!run->isEnabled()&&
                    notice->mapTo(hydroDialog_,QPoint{}).y()>run->mapTo(hydroDialog_,run->rect().bottomLeft()).y()&&
                    notice->mapTo(hydroDialog_,QPoint{}).y()>page->mapTo(hydroDialog_,page->rect().bottomLeft()).y());
                hydroDialog_->grab().save(workflowOutput_+"/views/loading-no-cases.png");
                selectNamed(false);
            }
            if(hydroRequest_["exercise_developer_template"].toBool()){
                auto* page=editorPage("loading_cases");
                if(!page){unavailable("Missing loading editor");return;}
                const auto initial=hydroDialog_->savedInputs();
                auto* d=hydroDialog_;
                auto* button=page->findChild<QPushButton*>("LoadBookTestTemplate");
                auto* grid=page->findChild<QListWidget*>("load_cases");
                if(!button||!grid){unavailable("Missing template controls");return;}
                auto toggle=[d]{QKeyEvent key(QEvent::KeyPress,Qt::Key_F12,Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier);QApplication::sendEvent(d,&key);};
                workflowCase("developer-disabled-by-default",!button->isVisible());
                toggle();workflowCase("developer-template-visible",button->isVisible());button->click();
                if(grid->count()<1){unavailable("Template created no rows");return;}
                grid->item(0)->setText("keep my edit");
                const auto edited=hydroDialog_->savedInputs();
                bool prompted=false;
                QTimer::singleShot(0,d,[&]{if(auto* m=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){prompted=true;m->button(QMessageBox::No)->click();}});
                button->click();
                workflowCase("decline-replacement-preserves-draft",prompted&&grid->item(0)->text()=="keep my edit");
                workflowCase("template-edits-save-immediately",edited!=initial&&hydroDialog_->savedInputs()==edited);
                QTimer::singleShot(0,d,[]{if(auto* m=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))m->button(QMessageBox::Yes)->click();});
                button->click();
                workflowCase("confirmed-replacement-reloads-template",grid->item(0)->text()!="keep my edit");
                toggle();workflowCase("developer-tools-hide-again",!button->isVisible());
                selectNamed(true);
                if(hydroDialog_->savedInputs()["loading_book"].toObject().isEmpty()){unavailable("Template not applied");return;}
            }
            if(hydroRequest_["observe_loading_ledger"].toBool()){
                auto* page=editorPage("loading_cases");
                if(!page){unavailable("Missing loading ledger");return;}
                auto* cases=page->findChild<QListWidget*>("load_cases");
                auto* items=page->findChild<QTableWidget*>("load_case_items");
                auto* summary=page->findChild<QWidget*>("load_summary");
                QJsonArray observed;
                if(cases&&items&&summary)for(int i=0;i<cases->count();++i){
                    cases->setCurrentRow(i);
                    if(i==1&&items->rowCount()){
                        auto* weight=items->item(0,fs::loading::Mass);
                        weight->setData(Qt::EditRole,QString::number(weight->data(Qt::EditRole).toDouble()+10,'g',17));
                    }
                    observed.append(QJsonObject{{"items",items->rowCount()},{"mass",summary->property("mass").toDouble()}});
                }
                workflowArtifacts_["ledger_cases"]=observed;
                auto* canvas=page->findChild<QScrollArea*>("LoadWeightCanvas");
                auto* fixed=page->findChild<QTableWidget*>("load_shared_items");
                if(canvas&&fixed&&items){
                    settleLayout();
                    const auto before=items->visualRect(items->model()->index(0,0)).center();
                    const auto global=items->viewport()->mapToGlobal(before);
                    QWheelEvent wheel(before,global,{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                    QApplication::sendEvent(items->viewport(),&wheel);
                    auto* outer=canvas->verticalScrollBar();
                    const bool wheelScrolled=outer->value()>0;
                    const auto last=items->visualRect(items->model()->index(items->rowCount()-1,0));
                    const auto sharedRow=fixed->visualRect(fixed->model()->index(fixed->rowCount()-1,0));
                    const bool allRowsFit=items->viewport()->rect().contains(last);
                    const bool ordered=canvas->isAncestorOf(items)&&canvas->isAncestorOf(fixed)&&
                        items->mapTo(canvas->widget(),items->rect().bottomLeft()).y()<fixed->mapTo(canvas->widget(),QPoint{}).y();
                    outer->setValue(outer->maximum());settleLayout();
                    const bool sharedReached=canvas->viewport()->rect().contains(
                        fixed->viewport()->mapTo(canvas->viewport(),sharedRow.center()));
                    workflowArtifacts_["weight_canvas_scroll"]=QJsonObject{{"detail_rows",items->rowCount()},
                        {"outer_range",outer->maximum()},{"wheel_scrolled",wheelScrolled},{"all_detail_rows_fit",allRowsFit},
                        {"shared_reached",sharedReached},{"detail_inner_range",items->verticalScrollBar()->maximum()},
                        {"shared_inner_range",fixed->verticalScrollBar()->maximum()}};
                    workflowCase("long-case-scrolls-whole-weight-canvas",items->rowCount()>40&&outer->maximum()>0&&wheelScrolled&&
                        allRowsFit&&ordered&&sharedReached&&items->verticalScrollBar()->maximum()==0&&fixed->verticalScrollBar()->maximum()==0);
                    hydroDialog_->grab().save(workflowOutput_+"/views/loading-workspace-bottom.png");
                    outer->setValue(0);settleLayout();
                }else workflowCase("long-case-scrolls-whole-weight-canvas",false);
                if(summary){
                    auto* title=summary->findChild<QLabel*>("load_summary_gravity_label");
                    auto* frame=summary->findChild<QFrame*>("load_summary_coordinates");
                    auto* metres=summary->findChild<QLabel*>("load_summary_gravity_unit");
                    QLabel* tonnes=nullptr;
                    for(auto* label:summary->findChildren<QLabel*>())
                        if(label->property("loadSummaryRole")=="unit"&&label->text()=="t")tonnes=label;
                    const int gap=title&&tonnes
                        ?title->mapTo(summary,QPoint{}).x()-tonnes->mapTo(summary,tonnes->rect().topRight()).x():-1;
                    workflowCase("d-summary-keeps-one-compact-coordinate-group",summary->isVisible()&&frame&&frame->isVisible()&&
                        metres&&metres->text()=="m"&&!frame->isAncestorOf(metres)&&gap>0&&gap<=36);
                }
                hydroDialog_->grab().save(workflowOutput_+"/views/loading-workspace.png");
                selectNamed(true);
            }
            if(hydroRequest_["exercise_loading_editor"].toBool()){
                auto* casePage=editorPage("loading_cases");
                if(!casePage){unavailable("Missing loading editor");return;}
                workflowCase("weight-panel-has-no-apply-button",!casePage->findChild<QPushButton*>("LoadBookApply"));
                using namespace fs::loading;
                auto* dlg=hydroDialog_;
                auto* cases=casePage->findChild<QListWidget*>("load_cases");
                auto* right=casePage->findChild<QScrollArea*>("LoadWeightCanvas");
                auto* left=casePage->findChild<QScrollArea*>("LoadCaseCanvas");
                auto* shared=casePage->findChild<QTableWidget*>("load_shared_items");
                auto* grid=casePage->findChild<QTableWidget*>("load_case_items");
                auto* summary=casePage->findChild<QWidget*>("load_summary");
                auto* warning=casePage->findChild<QLabel*>("LoadBookWarning");
                auto* addCase=casePage->findChild<QToolButton*>("load_add_cases");
                auto* addItem=casePage->findChild<QToolButton*>("load_add_case_items");
                if(!cases||cases->count()!=3||!left||!right||!grid||!shared||!summary||!warning||!addCase||!addItem){
                    unavailable("Missing unified weight editor");return;
                }
                dlg->resize(1440,dlg->height());settleLayout();
                const auto mass=[&]{return summary->property("mass").toDouble();};
                const auto centre=[&](QWidget* w){return w->mapTo(casePage,w->rect().center());};
                workflowCase("case-add-below-list-and-shared-weights-below-case-weights",
                    left->isAncestorOf(cases)&&left->isAncestorOf(addCase)&&centre(cases).y()<centre(addCase).y()&&
                    right->isAncestorOf(grid)&&right->isAncestorOf(shared)&&
                    grid->mapTo(casePage,grid->rect().bottomLeft()).y()<shared->mapTo(casePage,QPoint{}).y());
                QJsonArray samples;
                for(int col=Mass;col<=Z;++col){
                    const auto* item=shared->item(1,col);
                    samples.append(QJsonObject{{"display",item->text()},{"edit",item->data(Qt::EditRole).toString()}});
                }
                workflowArtifacts_["loading_book_number_display"]=samples;
                workflowCase("legacy-cargo-and-tank-in-same-table",mass()==210&&grid->rowCount()==2&&
                    grid->item(0,Name)->text()==uiText("货物")&&grid->item(1,Name)->text()=="Ballast"&&
                    grid->item(1,Fill)->data(Qt::EditRole).toDouble()==0);
                dlg->grab().save(workflowOutput_+"/views/loading-book-precision.png");

                const auto click=[](QWidget* widget,QPoint point){
                    QMouseEvent press(QEvent::MouseButtonPress,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                    QMouseEvent release(QEvent::MouseButtonRelease,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(widget,&press);QApplication::sendEvent(widget,&release);
                };
                const auto key=[](QWidget* widget,int value){
                    QKeyEvent event(QEvent::KeyPress,value,Qt::NoModifier);QApplication::sendEvent(widget,&event);
                };
                const auto cellValue=[](QTableWidget* table,int row,int col){
                    return table->item(row,col)->data(Qt::EditRole).toString();
                };
                const auto editCell=[](QTableWidget* table,int row,int column,const QString& value){
                    table->item(row,column)->setData(Qt::EditRole,value);
                };
                const auto removeAndUndo=[&](QTableWidget* table,int row){
                    table->scrollTo(table->model()->index(row,0),QAbstractItemView::EnsureVisible);
                    settleLayout();
                    auto* remove=removeInNameCell(table,row);
                    if(!remove){
                        const auto cell=table->visualRect(table->model()->index(row,0));
                        QJsonArray buttons;
                        for(auto* button:table->viewport()->findChildren<QToolButton*>("LoadBookRemoveRow")){
                            const auto box=button->geometry();
                            buttons.append(QJsonArray{box.x(),box.y(),box.width(),box.height(),button->isVisibleTo(table)});
                        }
                        workflowCase("weight-row-delete-control-visible",false,
                            QString::fromUtf8(QJsonDocument(QJsonObject{{"table",table->objectName()},
                                {"cell",QJsonArray{cell.x(),cell.y(),cell.width(),cell.height()}},{"buttons",buttons}}).toJson(QJsonDocument::Compact)));
                        return false;
                    }
                    const int count=table->rowCount();
                    remove->click();
                    const bool deleted=table->rowCount()==count-1;
                    draftKey(dlg,table,Qt::Key_Z,Qt::ControlModifier);
                    settleLayout();
                    return deleted&&table->rowCount()==count;
                };
                click(cases->viewport(),cases->visualItemRect(cases->item(2)).center());
                workflowCase("single-click-switches-load-case-without-editing",
                    cases->currentRow()==2&&mass()==220&&!cases->findChild<QLineEdit*>());
                key(cases,Qt::Key_F2);
                auto* nameEditor=cases->findChild<QLineEdit*>();
                if(nameEditor){nameEditor->setText("Renamed case");key(nameEditor,Qt::Key_Return);settleLayout();}
                const bool renamed=cases->item(2)->text()=="Renamed case";
                draftKey(dlg,cases,Qt::Key_Z,Qt::ControlModifier);
                workflowCase("explicit-case-rename-can-be-undone",renamed&&cases->item(2)->text()=="Full");

                addCase->click();
                auto* newName=cases->findChild<QLineEdit*>();
                const bool added=newName&&cases->count()==4&&cases->currentRow()==3&&centre(cases).y()<centre(addCase).y();
                if(newName){newName->setText("Warning test");key(newName,Qt::Key_Return);settleLayout();}
                editCell(shared,0,Mass,"0");settleLayout();
                for(auto* motion:warning->findChildren<QAbstractAnimation*>())
                    if(motion->state()==QAbstractAnimation::Running&&motion->loopCount()==1)
                        motion->setCurrentTime(motion->duration());
                settleLayout();
                const auto warningImage=warning->grab().toImage();
                const auto warningBg=QColor(warning->property("studio_message_background").toString());
                workflowCase("weight-warning-below-the-table",warning->isVisible()&&!warningImage.isNull()&&
                    warningImage.pixelColor(warningImage.width()-15,warningImage.height()/2)==warningBg&&
                    warning->text()==uiText("工况总重量必须在有效排水量范围内。")&&
                    warning->property("studio_message_role")=="warning"&&
                    centre(warning).y()>centre(right).y()&&!summary->isVisible()&&warning->parentWidget()==summary->parentWidget());
                dlg->grab().save(workflowOutput_+"/views/loading-book-warning.png");
                draftKey(dlg,shared,Qt::Key_Z,Qt::ControlModifier);
                draftKey(dlg,cases,Qt::Key_Z,Qt::ControlModifier); // Name.
                draftKey(dlg,cases,Qt::Key_Z,Qt::ControlModifier); // New case.
                workflowCase("adding-case-opens-name-editor-beneath-last-row",added&&cases->count()==3);

                cases->setCurrentRow(0);
                editCell(shared,0,Mass,"205");
                const bool first=mass()==215;
                cases->setCurrentRow(2);
                workflowCase("shared-weight-change-counted-in-each-case",first&&mass()==225);
                editCell(shared,0,Mass,"200");
                cases->setCurrentRow(1);settleLayout();
                const auto beforeFill=dlg->savedInputs().value("loading_book");
                const auto fillIndex=grid->model()->index(0,Fill);
                click(grid->viewport(),grid->visualRect(fillIndex).center());
                auto* fillEditor=grid->findChild<QLineEdit*>();
                if(fillEditor){fillEditor->setText("25");key(fillEditor,Qt::Key_Return);settleLayout();}
                workflowCase("live-tank-displacement",fillEditor&&mass()==205);
                const auto afterFill=dlg->savedInputs()["loading_book"].toObject();
                const auto savedFill=afterFill["cases"].toArray().at(1).toArray().at(1).toArray().at(0).toObject().value("fill");
                workflowCase("edit-loading-fraction",afterFill["version"]==4&&savedFill==25);
                if(hydroRequest_["exercise_delete_undo"].toBool()){
                    draftKey(dlg,grid,Qt::Key_Z,Qt::ControlModifier);
                    const bool undone=dlg->savedInputs()["loading_book"]==beforeFill;
                    draftKey(dlg,grid,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
                    workflowArtifacts_["loading_live_edit_undo"]=undone&&dlg->savedInputs()["loading_book"]==afterFill;
                }
                QJsonArray metrics;
                bool unitsVisible=true;
                for(auto* label:summary->findChildren<QLabel*>()){
                    const auto role=label->property("loadSummaryRole").toString();
                    if(role=="number")metrics.append(label->text());
                    if(role=="number"||role=="unit")
                        unitsVisible=unitsVisible&&label->isVisibleTo(summary)&&label->width()>=label->sizeHint().width();
                }
                auto* coordinates=summary->findChild<QFrame*>("load_summary_coordinates");
                auto* gravityUnit=summary->findChild<QLabel*>("load_summary_gravity_unit");
                int unitCount=0;
                for(auto* label:summary->findChildren<QLabel*>())
                    if(label->property("loadSummaryRole")=="unit")++unitCount;
                workflowCase("load-summary-groups-displacement-and-three-centre-coordinates",
                    coordinates&&gravityUnit&&!coordinates->isAncestorOf(gravityUnit)&&gravityUnit->text()=="m"&&
                    unitCount==2&&unitsVisible&&metrics==QJsonArray{"205","10","0","1.9573"});
                addItem->click();settleLayout();
                const int row=grid->rowCount()-1;
                auto* edit=grid->findChild<QLineEdit*>();
                if(edit){edit->setText("Cargo check");key(edit,Qt::Key_Return);settleLayout();}
                editCell(grid,row,X,"91.25");
                const auto combo=[&](int r){return qobject_cast<QComboBox*>(grid->cellWidget(r,Kind));};
                const bool kinds=combo(row)&&combo(row)->count()==4&&combo(row)->currentData()=="fixed"&&
                    combo(row)->findData("trapezoids")==-1;
                bool converted=kinds;
                for(int i=1;i<=3&&converted;++i){
                    auto* box=combo(row);
                    box->showPopup();settleLayout();
                    auto* list=box->view();
                    const auto index=list->model()->index(i,0);
                    click(list->viewport(),list->visualRect(index).center());
                    settleLayout();
                    auto* selected=combo(row);
                    const auto* kindName=tankTables()[i-1].key;
                    converted=selected&&selected->currentData()==kindName&&
                        !grid->item(row,Mass)->flags().testFlag(Qt::ItemIsEditable)&&
                        !grid->isColumnHidden(Fill)&&grid->item(row,Fill)->flags().testFlag(Qt::ItemIsEditable)&&
                        grid->item(row,Name)->text()=="Cargo check"&&cellValue(grid,row,X)=="91.25";
                    for(auto field:tankDimensions()[i-1])
                        converted=converted&&!grid->isColumnHidden(field)&&grid->item(row,field)->flags().testFlag(Qt::ItemIsEditable);
                }
                workflowCase("four-weight-types-available-for-new-items",kinds&&converted);
                for(int i=0;i<3;++i)draftKey(dlg,grid,Qt::Key_Z,Qt::ControlModifier);
                workflowCase("undo-restores-fixed-type-and-values",combo(row)&&combo(row)->currentData()=="fixed"&&cellValue(grid,row,X)=="91.25");

                if(hydroRequest_["exercise_delete_undo"].toBool()){
                    const bool fixedUndo=removeAndUndo(shared,1)&&cellValue(shared,1,X)==samples[1].toObject()["edit"].toString();
                    const bool tankUndo=removeAndUndo(grid,0)&&cellValue(grid,0,Fill)=="25";
                    cases->setCurrentRow(2);
                    auto* remove=removeInNameCell(cases,1);
                    if(remove)remove->click();
                    const bool deleted=remove&&cases->count()==2&&cases->currentRow()==1&&mass()==220;
                    draftKey(dlg,cases,Qt::Key_Z,Qt::ControlModifier);
                    const bool restored=cases->count()==3&&cases->currentRow()==2&&mass()==220;
                    cases->setCurrentRow(1);
                    const bool caseUndo=deleted&&restored&&mass()==205&&grid->item(1,Name)->text()=="Cargo check";
                    const bool itemUndo=removeAndUndo(grid,1)&&cellValue(grid,1,X)=="91.25";
                    workflowArtifacts_["loading_delete_undo"]=QJsonObject{{"fixed",fixedUndo},{"tank_and_fills",tankUndo},
                        {"case_and_weights",caseUndo},{"case_weight",itemUndo}};
                }
                if(hydroRequest_["exercise_delete_undo"].toBool()){
                    const auto before=dlg->savedInputs()["loading_book"].toObject();
                    int removed=0;
                    for(int i=cases->count()-1;i>=0;--i){
                        auto* remove=removeInNameCell(cases,i);
                        if(!remove)break;
                        remove->click();
                        ++removed;
                    }
                    const auto empty=dlg->savedInputs()["loading_book"].toObject();
                    workflowCase("removing-final-case-keeps-shared-weights",
                        cases->count()==0&&empty["cases"].toArray().isEmpty()&&
                        empty["shared_items"]==before["shared_items"]);
                    for(int i=0;i<removed;++i)draftKey(dlg,cases,Qt::Key_Z,Qt::ControlModifier);
                    workflowCase("undo-restores-removed-cases",dlg->savedInputs()["loading_book"]==before);
                }
            }
            if(hydroRequest_["exercise_delete_undo"].toBool()){
                const auto before=hydroDialog_->savedInputs()["loading_book"].toObject();
                selectNamed(true);
                auto* criteriaPage=criteriaPanel();
                if(!criteriaPage||!criteriaBesideInputs(true)){unavailable("Missing criteria editor");return;}
                auto* table=criteriaPage->findChild<QTableWidget*>("load_criteria");
                auto* remove=table?removeInNameCell(table,0):nullptr;
                if(!table||!remove||table->rowCount()!=1){unavailable("Missing criterion row");return;}
                table->setCurrentCell(0,0);remove->click();
                const bool deleted=table->rowCount()==0&&
                    hydroDialog_->savedInputs()["loading_book"].toObject()["criteria"].toArray().isEmpty();
                draftKey(hydroDialog_,table,Qt::Key_Z,Qt::ControlModifier);
                const bool undone=table->rowCount()==1&&table->item(0,0)->text()=="10";
                draftKey(hydroDialog_,table,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
                const bool redone=table->rowCount()==0;
                draftKey(hydroDialog_,table,Qt::Key_Z,Qt::ControlModifier);
                auto observed=workflowArtifacts_["loading_delete_undo"].toObject();
                observed["criterion"]=deleted&&undone&&redone&&table->rowCount()==1;
                workflowArtifacts_["loading_delete_undo"]=observed;
                if(table->rowCount()!=1){unavailable("Criterion could not be restored");return;}
                table->item(0,1)->setData(Qt::EditRole,"0.35");

                auto expected=before;expected["criteria"]=QJsonArray{QJsonArray{10,0.35}};
                workflowCase("criteria-edit-preserves-loads",hydroDialog_->savedInputs()["loading_book"]==expected);
                draftKey(hydroDialog_,table,Qt::Key_Z,Qt::ControlModifier);
                workflowCase("criterion-undo-restores-active-value",hydroDialog_->savedInputs()["loading_book"]==before);
                selectNamed(true);
            }
            if(hydroRequest_["edit_unified_workbook"].toBool()){
                auto* page=editorPage("loading_cases");
                auto* list=page?page->findChild<QListWidget*>("load_cases"):nullptr;
                if(!list||!list->count()){unavailable("Missing unified loading list");return;}
                auto* weights=page->findChild<QTableWidget*>("load_case_items");
                bool legacyTypeRetained=false;
                for(int row=0;weights&&row<list->count();++row){
                    list->setCurrentRow(row);
                    for(int item=0;item<weights->rowCount();++item){
                        const auto* type=weights->item(item,fs::loading::Kind);
                        if(type&&type->text()==uiText("梯形液货舱")&&!weights->cellWidget(item,fs::loading::Kind))
                            legacyTypeRetained=true;
                    }
                }
                workflowCase("saved-trapezoid-keeps-type-without-a-selector",legacyTypeRetained);
                list->setCurrentRow(0);
                const auto name=list->item(0)->text();
                list->item(0)->setText(name+" (edit)");
                list->item(0)->setText(name);
                workflowCase("legacy-tank-edit-saves-in-unified-format",
                    hydroDialog_->savedInputs()["loading_book"].toObject()["version"]==4);
                selectNamed(true);
            }
            if(hydroRequest_["switch_language_after_open"].isString()){
                const auto error=setUiLanguage(hydroRequest_["switch_language_after_open"].toString());
                if(!error.isEmpty()){unavailable("Hydro language switch failed: "+error);return;}
                workflowArtifacts_["language_switched_after_open"]=uiLanguage();
                QJsonArray titles;
                for(auto name:{"LoadCaseCanvas","LoadWeightCanvas"}){
                    if(auto* scroll=hydroDialog_->findChild<QScrollArea*>(name))
                        for(const auto* label:scroll->widget()->findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly))
                            titles.append(label->text());
                }
                workflowArtifacts_["loading_weight_scope_titles"]=titles;
                if(hydroRequest_["exercise_loading_editor"].toBool()){
                    auto* casePage=editorPage("loading_cases");
                    auto* gravityTitle=casePage->findChild<QLabel*>("load_summary_gravity_label");
                    workflowCase("load-summary-heading-follows-language",gravityTitle&&gravityTitle->text()==uiText("总重心"));
                    for(auto* motion:parameterTabs->tabBar()->findChildren<QAbstractAnimation*>())
                        if(motion->state()==QAbstractAnimation::Running&&motion->loopCount()==1)
                            motion->setCurrentTime(motion->duration());
                    settleLayout();
                    hydroDialog_->grab().save(workflowOutput_+"/views/unified-weight-items.png");
                    selectNamed(true);
                }
                if(hydroRequest_["observe_language"].toBool()){
                    QJsonObject labels;
                    for(const auto* label:hydroDialog_->findChildren<QLabel*>()){
                        const auto key=label->property("hydro_field_label").toString();
                        if(!key.isEmpty())labels[key]=label->text();
                    }
                    workflowArtifacts_["language_labels_after_open"]=labels;
                }
            }
            workflowCase("hydro.no-manual-model-parameter-save",!findButton("save_settings"));
            if(hydroRequest_["inputs"].toObject()["mode"]=="table"){
                workflowArtifacts_["initial_table_range"]=tableRange(hydroDialog_->inputs());
                workflowArtifacts_["design_draft"]=session_.model()->draft;
                workflowCase("hydro.range-has-no-manual-button",!findButton("save_settings"));
                if(hydroRequest_["read_table_range"].toBool()){
                    timer->stop();timer->deleteLater();hydroDialog_->reject();return;
                }
            }
            const QFontInfo font(hydroDialog_->font());
            workflowArtifacts_["dialog_font"]=QJsonObject{{"requested",hydroDialog_->font().family()},{"resolved",font.family()},{"style",font.styleName()},{"exact_match",font.exactMatch()},{"point_size",font.pointSizeF()}};
            QJsonArray discovered;const auto values=hydroRequest_["inputs"].toObject();QSet<QString> used;
            if(pageFor(true)){
                discovered.append("use_loading_book");
                if(values.contains("use_loading_book")){selectNamed(values["use_loading_book"].toBool());used.insert("use_loading_book");}
            }
            auto* inputPage=pageFor(hydroDialog_->savedInputs()["use_loading_book"].toBool());
            for(auto* widget:(inputPage?inputPage:hydroDialog_)->findChildren<QWidget*>()){
                const auto key=widget->property("field").toString();if(key.isEmpty())continue;discovered.append(key);if(!values.contains(key))continue;used.insert(key);
                if(auto* n=qobject_cast<QDoubleSpinBox*>(widget))n->setValue(values[key].toDouble());
                else if(auto* n=qobject_cast<QCheckBox*>(widget))n->setChecked(values[key].toBool());
                else if(auto* n=dynamic_cast<HydroNumberList*>(widget))n->setValues(values[key]);
            }
            workflowArtifacts_["discovered_fields"]=discovered;
            for(auto it=values.begin();it!=values.end();++it)if(it.key()!="mode"&&!used.contains(it.key())){unavailable("Hydro input not discovered: "+it.key());return;}
            if(hydroRequest_["exercise_stability_inputs"].toBool()){
                QMap<QString,QWidget*> controls;
                if(!pageFor(false)||!pageFor(true)){unavailable("Stability parameter tabs unavailable");return;}
                for(auto* widget:pageFor(false)->findChildren<QWidget*>()){
                    const auto field=widget->property("field").toString();if(!field.isEmpty())controls[field]=widget;
                }
                auto* summary=hydroDialog_->findChild<QTableWidget*>("HydroLoadingValues");
                auto* casePage=pageByKey("loading_cases");
                auto* angles=dynamic_cast<HydroNumberList*>(controls.value("angles"));
                auto* displacements=dynamic_cast<HydroNumberList*>(controls.value("displacements"));
                HydroNumberList* loadingAngles=nullptr;
                for(auto* w:pageFor(true)->findChildren<QWidget*>())
                    if(w->property("field")==QString("angles")){loadingAngles=dynamic_cast<HydroNumberList*>(w);break;}
                if(!loadingAngles||!summary||!casePage||!angles||!displacements){unavailable("Stability input controls unavailable");return;}
                const auto visible=[&](QWidget* w){return w&&w->isVisibleTo(hydroDialog_);};
                const auto observe=[&]{
                    QJsonObject state,positions;QJsonArray headers,rows,shownAngles,shownDisplacements;
                    for(auto key:{"displacements","lcg","tcg","vcg","angles"}){
                        const bool named=parameterTabs->currentWidget()==pageFor(true);
                        auto* w=QString(key)=="angles"&&named?loadingAngles:controls.value(key);QLabel* label=nullptr;
                        for(auto* candidate:pageFor(QString(key)=="angles"&&named)->findChildren<QLabel*>())
                            if(candidate->property("hydro_field_label")==QString(key)){label=candidate;break;}
                        state[key]=QJsonObject{{"field_visible",visible(w)},{"label_visible",visible(label)}};
                        if(visible(w))positions[key]=w->mapTo(hydroDialog_,QPoint{}).y();
                    }
                    for(int col=0;col<summary->columnCount();++col)headers.append(summary->horizontalHeaderItem(col)->text());
                    for(int row=0;row<summary->rowCount();++row){
                        QJsonArray values;bool editable=false;
                        for(int col=0;col<summary->columnCount();++col){auto* item=summary->item(row,col);values.append(QJsonValue::fromVariant(item->data(Qt::UserRole)));editable|=item->flags().testFlag(Qt::ItemIsEditable);}
                        rows.append(QJsonObject{{"values",values},{"editable",editable}});
                    }
                    auto* visibleAngles=parameterTabs->currentWidget()==pageFor(true)?loadingAngles:angles;
                    for(auto* block:visibleAngles->findChildren<QToolButton*>())
                        if(block->property("hydro_value_index").isValid()&&visible(block))shownAngles.append(block->text());
                    for(auto* block:displacements->findChildren<QToolButton*>())
                        if(block->property("hydro_value_index").isValid()&&visible(block))shownDisplacements.append(block->text());
                    return QJsonObject{{"controls",state},{"input_positions",positions},{"summary_visible",visible(summary)},
                        {"summary_title",summary->accessibleName()},{"headers",headers},{"rows",rows},
                        {"edit_load_cases",parameterTabs->tabText(parameterTabs->indexOf(casePage))},
                        {"edit_load_cases_visible",parameterTabs->isTabVisible(parameterTabs->indexOf(casePage))},
                        {"angle_blocks",shownAngles},{"displacement_blocks",shownDisplacements},
                        {"calc_enabled",findButton("calculate")->isEnabled()},
                        {"inputs",hydroDialog_->savedInputs()}};
                };
                const auto originalLoadingAngles=loadingAngles->values();
                loadingAngles->setValues(QJsonArray{-2,0,8});
                auto* bar=parameterTabs->tabBar();
                auto* motion=bar->findChild<QAbstractAnimation*>("HydroLoadingTabMotion");
                const auto finishMotion=[motion]{
                    if(motion&&motion->state()==QAbstractAnimation::Running)motion->setCurrentTime(motion->duration());
                };
                selectNamed(false);finishMotion();settleLayout();
                const auto compactAnchor=bar->tabRect(1);
                selectNamed(true);
                const bool expanded=motion&&motion->state()==QAbstractAnimation::Running;
                bool anchored=false;
                if(expanded){
                    motion->setCurrentTime(motion->duration()/2);
                    const auto partial=bar->grab().toImage();
                    hydroDialog_->grab().save(workflowOutput_+"/views/loading-tabs-emerging.png");
                    finishMotion();
                    const auto settled=bar->grab();
                    const auto reportRect=bar->tabRect(parameterTabs->indexOf(pageByKey("report")));
                    const auto scaledRect=QTransform::fromScale(settled.devicePixelRatio(),settled.devicePixelRatio()).mapRect(reportRect);
                    anchored=bar->tabRect(1)==compactAnchor&&partial.copy(scaledRect)!=settled.toImage().copy(scaledRect);
                }
                workflowCase("loading-tabs-emerge-from-stationary-parameter-tab",anchored);
                workflowArtifacts_["stability_named_inputs"]=observe();
                const auto namedRequest=hydroDialog_->inputs();
                selectPage(pageByKey("results"));
                workflowCase("disabled-result-tab-keeps-loading-parameters",
                    parameterTabs->currentWidget()==pageFor(true)&&hydroDialog_->inputs()==namedRequest);
                selectNamed(true);
                hydroDialog_->grab().save(workflowOutput_+"/views/loading-inputs.png");
                selectNamed(false);
                const bool collapsed=motion&&motion->state()==QAbstractAnimation::Running;
                if(collapsed){
                    motion->setCurrentTime(motion->duration()/2);
                    hydroDialog_->grab().save(workflowOutput_+"/views/loading-tabs-retracting.png");
                    finishMotion();
                }
                const int editorIndex=parameterTabs->indexOf(pageByKey("loading_cases"));
                const bool hidden=!parameterTabs->isTabVisible(editorIndex);
                workflowCase("shared-criteria-embedded-beside-manual-inputs",criteriaBesideInputs(false)&&
                    !hydroDialog_->inputs()["use_loading_book"].toBool());
                selectNamed(true);
                const bool shown=parameterTabs->isTabVisible(editorIndex)&&parameterTabs->indexOf(pageByKey("results"))==3;
                workflowCase("shared-criteria-embedded-beside-loading-inputs",criteriaBesideInputs(true));
                auto* criteriaTitle=criteriaPanel()->findChild<QLabel*>("HydroCriteriaTitle");
                workflowArtifacts_["loading_tab_titles"]=QJsonArray{parameterTabs->tabText(editorIndex)};
                workflowArtifacts_["criteria_panel_title"]=criteriaTitle?criteriaTitle->text():QString{};
                if(motion&&motion->state()==QAbstractAnimation::Running)motion->setCurrentTime(motion->duration());
                hydroDialog_->grab().save(workflowOutput_+"/views/loading-tabs-expanded.png");
                selectNamed(false);
                if(motion&&motion->state()==QAbstractAnimation::Running)motion->setCurrentTime(motion->duration());
                workflowCase("loading-editor-tabs-animate-in-and-out",expanded&&collapsed&&shown&&hidden);
                workflowArtifacts_["stability_manual_inputs"]=observe();
                const auto directRequest=hydroDialog_->inputs();
                workflowCase("stability-parameter-tabs-are-exclusive",
                    parameterTabs->indexOf(pageFor(false))==0&&parameterTabs->indexOf(pageFor(true))==1&&
                    parameterTabs->tabText(1)==uiText("参数表（装载工况）")&&
                    namedRequest["use_loading_book"].toBool()&&!namedRequest.contains("displacements")&&
                    namedRequest["angles"]==QJsonArray{-2,0,8}&&namedRequest["loading_book"].isObject()&&
                    !directRequest["use_loading_book"].toBool()&&!directRequest.contains("loading_book")&&
                    directRequest["angles"]==angles->values()&&directRequest["angles"]!=namedRequest["angles"]);
                loadingAngles->setValues(originalLoadingAngles);
                hydroDialog_->grab().save(workflowOutput_+"/views/manual-inputs.png");
                const auto action=[&](const QString& name)->QWidget*{
                    for(auto* w:angles->findChildren<QWidget*>())if(w->property("hydro_list_action")==name)return w;
                    return nullptr;
                };
                const auto grouped=[](HydroNumberList* list){
                    const auto action=[list](const QString& name)->QWidget*{
                        for(auto* w:list->findChildren<QWidget*>())
                            if(w->property("hydro_list_action")==name)return w;
                        return nullptr;
                    };
                    auto* add=action("add");auto* range=action("range");auto* drop=action("drop");
                    QLabel* label=nullptr;
                    for(auto* candidate:list->findChildren<QLabel*>())
                        if(candidate->property("hydro_field_label").isValid()){label=candidate;break;}
                    if(!label||!add||!range||!drop)return false;
                    const auto x=[list](QWidget* w){return w->mapTo(list,QPoint{}).x();};
                    return x(add)>x(label)&&x(add)-x(label)-label->width()<45&&
                           x(range)-x(add)-add->width()<12&&x(drop)-x(range)-range->width()<18&&
                           drop->y()<=add->y()+add->height();
                };
                workflowCase("number-actions-next-to-parameter-caption",grouped(angles)&&grouped(displacements));
                const auto blockAt=[&](int index)->QToolButton*{
                    for(auto* b:angles->findChildren<QToolButton*>())
                        if(visible(b)&&b->property("hydro_value_index").isValid()&&b->property("hydro_value_index").toInt()==index)return b;
                    return nullptr;
                };
                auto* add=qobject_cast<QToolButton*>(action("add"));
                auto* drop=action("drop");
                QLineEdit* editor=nullptr;
                for(auto* line:angles->findChildren<QLineEdit*>())
                    if(line->property("hydro_list_value").toBool())editor=line;
                if(!add||!drop||!editor){unavailable("Stability value actions unavailable");return;}
                add->click();settleLayout();
                workflowArtifacts_["stability_empty_row"]=observe();
                QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
                QApplication::sendEvent(editor,&enter);
                workflowCase("blank-add-stays-uncommitted",editor->isVisible()&&angles->values()==QJsonArray{-5,.1,5});
                editor->setText("15");
                QApplication::sendEvent(editor,&enter);settleLayout();
                workflowArtifacts_["stability_row_added"]=observe();
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-value-blocks.png");
                auto* last=blockAt(3);
                if(!last){unavailable("New stability value unavailable");return;}
                bool ghostMatches=false;
                const auto drag=[&](QToolButton* block,const QPoint& destination){
                    const auto start=block->mapToGlobal(block->rect().center());
                    const auto send=[&](QEvent::Type type,QPoint global,Qt::MouseButton button,Qt::MouseButtons buttons){
                        QMouseEvent event(type,block->mapFromGlobal(global),global,button,buttons,Qt::NoModifier);
                        QApplication::sendEvent(block,&event);
                    };
                    send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                    send(QEvent::MouseMove,destination,Qt::NoButton,Qt::LeftButton);
                    const auto* ghost=angles->findChild<QLabel*>("HydroValueGhost");
                    ghostMatches=ghost&&ghost->isVisible()&&ghost->size()==block->size()&&
                        ghost->mapToGlobal(block->rect().center())==destination;
                    const bool highlighted=drop->property("dragOver").toBool();
                    send(QEvent::MouseButtonRelease,destination,Qt::LeftButton,Qt::NoButton);
                    settleLayout();
                    return highlighted;
                };
                const bool cancelHighlighted=drag(last,angles->mapToGlobal(QPoint(-20,0)));
                workflowArtifacts_["stability_drag_cancel"]=observe();
                const bool highlighted=drag(last,drop->mapToGlobal(drop->rect().center()));
                workflowArtifacts_["stability_drop_highlight"]=QJsonObject{
                    {"outside",cancelHighlighted},{"inside",highlighted},{"after",drop->property("dragOver").toBool()},
                    {"ghost_matches",ghostMatches}};
                workflowArtifacts_["stability_row_removed"]=observe();
                if(hydroRequest_["exercise_delete_undo"].toBool()){
                    draftKey(hydroDialog_,add,Qt::Key_Z,Qt::ControlModifier);settleLayout();
                    const auto afterUndo=angles->values();
                    draftKey(hydroDialog_,add,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);settleLayout();
                    const auto afterRedo=angles->values();
                    bool keyboardUndo=false;
                    if(auto* first=blockAt(0)){
                        draftKey(hydroDialog_,first,Qt::Key_Delete,Qt::NoModifier);settleLayout();
                        const auto deleted=angles->values();
                        draftKey(hydroDialog_,add,Qt::Key_Z,Qt::ControlModifier);settleLayout();
                        keyboardUndo=deleted==QJsonArray{.1,5}&&angles->values()==afterRedo;
                    }
                    workflowArtifacts_["stability_delete_undo"]=QJsonObject{{"undo",afterUndo},{"redo",afterRedo},
                        {"keyboard",keyboardUndo}};
                }
                if(auto* first=blockAt(0)){
                    first->click();editor->setText("-6");QApplication::sendEvent(editor,&enter);settleLayout();
                    workflowArtifacts_["stability_edited_block"]=observe();
                    if(auto* edited=blockAt(0)){edited->click();editor->setText("-5");QApplication::sendEvent(editor,&enter);}
                }
                const auto originalAngles=angles->values();
                auto* batch=qobject_cast<QToolButton*>(action("range"));
                auto* start=qobject_cast<QLineEdit*>(action("range_start"));
                auto* end=qobject_cast<QLineEdit*>(action("range_end"));
                auto* count=qobject_cast<QSpinBox*>(action("range_count"));
                auto* step=qobject_cast<QLabel*>(action("range_step"));
                auto* apply=qobject_cast<QToolButton*>(action("range_apply"));
                auto* cancelRange=qobject_cast<QToolButton*>(action("range_cancel"));
                if(!batch||!start||!end||!count||!step||!apply||!cancelRange){unavailable("Stability range editor unavailable");return;}
                batch->click();start->setText("10");end->setText("-5");
                const bool reversedBlocked=!apply->isEnabled();
                start->setText("-5");end->setText("10");count->setValue(4);settleLayout();
                const auto interval=step->text();
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-value-interval.png");
                apply->click();settleLayout();
                const auto merged=angles->values();
                batch->click();end->setText("25");cancelRange->click();settleLayout();
                workflowArtifacts_["stability_batch"]=QJsonObject{
                    {"reversed_blocked",reversedBlocked},{"interval",interval},
                    {"values",merged},{"after_cancel",angles->values()}};
                QJsonArray range;
                for(int angle=0;angle<=60;angle+=5)range.append(angle);
                angles->setValues(range);settleLayout();
                QSet<int> rows;
                int left=angles->width(),right=0;
                bool fits=true,compact=true;
                const int dropBottom=drop->mapTo(angles,QPoint{}).y()+drop->height();
                for(int i=0;i<range.size();++i){
                    auto* block=blockAt(i);
                    if(!block){fits=false;continue;}
                    const auto pos=block->mapTo(angles,QPoint{});
                    rows.insert(pos.y());
                    left=qMin(left,pos.x());right=qMax(right,pos.x()+block->width());
                    const int textWidth=block->fontMetrics().horizontalAdvance(block->text());
                    compact&=block->width()>=textWidth+12&&block->width()<=qMax(24,textWidth+14);
                    fits&=pos.y()>=dropBottom&&pos.y()+block->height()<=angles->height();
                }
                workflowArtifacts_["stability_full_grid"]=QJsonObject{
                    {"state",observe()},{"rows",rows.size()},
                    {"fits_below_header",fits},{"more_button",action("more")!=nullptr},
                    {"left",left},{"right",right},{"width",angles->width()},{"compact",compact},
                    {"inner_scroll",angles->findChild<QScrollArea*>()!=nullptr||
                                    displacements->findChild<QScrollArea*>()!=nullptr}};
                angles->setValues(QJsonArray{-0.90005,0.90005,99.876,123});settleLayout();
                QJsonArray precisions;
                for(int i=0;i<4;++i)if(auto* block=dynamic_cast<HydroValueBlock*>(blockAt(i))){
                    const auto preview=block->previewText();
                    precisions.append(QJsonObject{{"preview",preview},{"full",block->toolTip()},
                        {"width",block->width()},{"text_width",block->fontMetrics().horizontalAdvance(preview)}});
                }
                workflowArtifacts_["stability_chip_precision"]=QJsonObject{{"blocks",precisions},{"values",angles->values()}};
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-value-precision.png");
                if(auto* first=blockAt(0)){
                    const auto original=angles->values();
                    const auto committedBefore=hydroDialog_->savedInputs();
                    const auto priorRect=first->geometry();
                    first->click();settleLayout();
                    const auto full=first->toolTip();
                    workflowCase("stability-value-edits-in-place-at-full-width",
                        editor->isVisible()&&editor->text()==full&&editor->geometry().contains(priorRect)&&
                        editor->width()>=editor->fontMetrics().horizontalAdvance(full)+12&&
                        editor->width()>priorRect.width()&&!action("commit")&&!action("cancel"));
                    hydroDialog_->grab().save(workflowOutput_+"/views/stability-value-inline-edit.png");
                    editor->setText("-0.1234567890123456");
                    workflowCase("stability-uncommitted-value-excluded-from-save",hydroDialog_->savedInputs()==committedBefore);
                    QApplication::sendEvent(editor,&escape);settleLayout();
                    workflowCase("stability-escape-discards-edit",!editor->isVisible()&&angles->values()==original);
                    first->click();
                    editor->setText("-0.1234567890123456");
                    displacements->setFocus();settleLayout();
                    workflowCase("stability-focus-loss-discards-edit",
                        displacements->focusProxy()->hasFocus()&&!editor->isVisible()&&angles->values()==original);
                }
                angles->setValues(originalAngles);settleLayout();
                selectNamed(!hydroRequest_["solve_manual_stability"].toBool());
                if(hydroRequest_["solve_manual_stability"].toBool())loadingAngles->setValues(QJsonArray{-2,0,8});
                workflowArtifacts_["stability_returned_inputs"]=observe();
            }
            if(hydroRequest_["observe_input_feedback"].toBool()){
                QJsonObject descriptions;
                for(const auto* label:hydroDialog_->findChildren<QLabel*>()){
                    const auto key=label->property("calculation_input_status").toString();
                    if(!key.isEmpty())descriptions[key]=label->toolTip();
                }
                workflowArtifacts_["validation_descriptions"]=descriptions;
                const auto* run=findButton("calculate");
                workflowArtifacts_["can_calculate"]=run&&run->isEnabled();
            }
            if(!hydroDialog_->grab().save(workflowOutput_+"/views/parameters.png")){timer->stop();hydroDialog_->reject();finishWorkflow("Cannot save parameters image");return;}
            captureMessages("parameters");
            if(hydroRequest_["observe_calculation_tabs"].toBool()){
                if(auto* tabs=hydroDialog_->findChild<QTabWidget*>()){
                    QJsonArray pages;
                    for(int i=0;i<tabs->count();++i)if(tabs->isTabVisible(i))pages.append(tabs->tabText(i));
                    workflowArtifacts_["calculation_tabs"]=QJsonObject{{"pages",pages},
                        {"selected",tabs->tabText(tabs->currentIndex())},{"nested",!tabs->findChildren<QTabWidget*>().isEmpty()}};
                }
            }
            workflowArtifacts_["effective_inputs"]=hydroDialog_->savedInputs();
            if(hydroRequest_["save_input_draft"].toBool()){
                const bool attached=hydroDialog_->actions().contains(save_)&&save_->shortcuts().contains(QKeySequence("Ctrl+S"));
                workflowCase("draft-save-shortcut-attached",attached);
                *stage=4;
                if(attached)save_->trigger();
                return;
            }
            if(hydroRequest_["describe_only"].toBool()){timer->stop();timer->deleteLater();hydroDialog_->reject();return;}
            runHydro();
            return;
        }
        if(!hydroDialog_->property("hydro_finished").toBool()||session_.busy())return;
        auto resultHash=[this]{return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(hydroDialog_->result()).toJson(),QCryptographicHash::Sha256).toHex());};
        if(*stage==2||*stage==3){
            if(*stage==2)workflowArtifacts_["table_range_saved"]=!session_.modified();
            if(*stage==3){
                const bool cancelled=hydroDialog_->property("hydro_cancelled").toBool()||!hydroDialog_->property("hydro_error").toString().isEmpty();
                if(cancelled){
                    workflowCase("cancel-preserves-complete-result",resultHash()==workflowArtifacts_["result_before_cancel"].toString());
                    workflowCase("repeat-cancel-reported",true);
                }else{
                    const auto result=hydroDialog_->result();
                    const QJsonObject values{{"rows",result["rows"]},{"auxiliary",result["auxiliary"]}};
                    workflowCase("repeat-fast-completion-keeps-values",values==workflowArtifacts_["values_before_cancel"].toObject());
                    workflowCases_.append(QJsonObject{{"name","cancel-preserves-complete-result"},{"status","not_run"},{"message","Cached calculation completed before cancellation was observed; active batch cancellation is checked separately."}});
                }
            }
            timer->stop();timer->deleteLater();hydroDialog_->reject();return;
        }
        workflowArtifacts_["error"]=hydroDialog_->property("hydro_error").toString();workflowArtifacts_["cancelled"]=hydroDialog_->property("hydro_cancelled").toBool();
        workflowArtifacts_["table_range_recorded"]=hydroDialog_->result()["parameters_recorded"].toBool();
        workflowArtifacts_["model_dirty_after_calculation"]=session_.modified();
        settleLayout();captureMessages("result");
        if(parameterTabs&&!hydroDialog_->result().isEmpty()){
            QJsonObject states;
            for(const auto& key:outputPages)if(auto* page=pageByKey(key)){
                const int index=parameterTabs->indexOf(page);
                if(parameterTabs->isTabVisible(index))states[key]=parameterTabs->isTabEnabled(index);
            }
            workflowArtifacts_["hydro_output_tabs_after_result"]=states;
            workflowCase("hydro-report-opens-after-result",states["report"].toBool());
            auto* bar=parameterTabs->tabBar();
            const int selected=bar->currentIndex();
            const auto point=bar->tabRect(selected).center();
            const auto global=bar->mapToGlobal(point);
            bool fixed=selected>=0&&bar->count()>1;
            for(const auto delta:{-120,120}){
                QWheelEvent wheel(point,global,{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QApplication::sendEvent(bar,&wheel);
                fixed=fixed&&bar->currentIndex()==selected;
            }
            workflowCase("hydro-tab-wheel-keeps-active-page",fixed);
            if(bar->currentIndex()!=selected)bar->setCurrentIndex(selected);
        }
        if(auto* page=hydroDialog_->findChild<QWidget*>("HydroCurvePage")){
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>()){
                const int index=tabs->indexOf(page);
                if(index>=0)workflowArtifacts_["curves_tab_visible"]=tabs->isTabVisible(index);
            }
        }
        const auto inspectResultStatus=[&](QTableWidget* table){
            QJsonArray icons;QString reason,instantTip;int failed=-1;
            for(int row=0;row<table->rowCount();++row){
                const auto* cell=table->item(row,0);
                if(!cell)continue;
                const bool ok=cell->data(Qt::UserRole).toBool();
                icons.append(QJsonObject{{"success",ok},{"icon",!cell->icon().isNull()},
                    {"text",cell->text()},{"reason",cell->toolTip()}});
                if(failed<0&&!ok){failed=row;reason=cell->toolTip();}
            }
            if(failed>=0){
                table->scrollToItem(table->item(failed,0));settleLayout();
                const auto point=table->visualItemRect(table->item(failed,0)).center();
                QMouseEvent move(QEvent::MouseMove,point,table->viewport()->mapToGlobal(point),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(table->viewport(),&move);
                if(QToolTip::isVisible())instantTip=QToolTip::text();
                QEvent leave(QEvent::Leave);QApplication::sendEvent(table->viewport(),&leave);
            }
            const auto* heading=table->horizontalHeaderItem(0);
            return QJsonObject{{"heading",heading?heading->text():QString{}},
                {"icons",icons},{"reason",reason},{"hover",instantTip}};
        };
        if(hydroRequest_["observe_stability_presentation"].toBool()){
            auto* chart=dynamic_cast<CalculationPlot*>(hydroDialog_->findChild<QWidget*>("HydroStabilityChart"));
            auto* comparison=hydroDialog_->findChild<QTableWidget*>("LoadingComparison");
            auto* page=hydroDialog_->findChild<QWidget*>("HydroCurvePage");
            auto* tabs=hydroDialog_->findChild<QTabWidget*>();
            QTableWidget* result=nullptr;
            for(auto* table:hydroDialog_->findChildren<QTableWidget*>())
                if(table->property("hydro_result_table").toBool()){result=table;break;}
            if(chart&&comparison&&page&&result){
                tabs->setCurrentWidget(page);settleLayout();
                auto* canvas=chart->findChild<QWidget*>("CalculationCanvas");
                QMap<int,QToolButton*> controls;
                for(auto* button:chart->findChildren<QToolButton*>())if(button->property("chart_curve").isValid())
                    controls.insert(button->property("chart_curve").toInt(),button);
                const auto overlay=[&]{if(canvas)canvas->grab();return chart->evidence();};
                const auto all=overlay();
                QJsonArray legendButtons;
                for(auto* button:controls){
                    const auto pos=button->mapTo(chart,QPoint{});
                    legendButtons.append(QJsonObject{{"text",button->text()},{"tooltip",button->toolTip()},
                        {"x",pos.x()},{"y",pos.y()},{"width",button->width()}});
                }
                auto* first=controls.value(0);
                const auto original=hydroDialog_->result();
                if(first)first->click();
                const auto hidden=overlay();
                if(first)first->click();
                const auto restored=overlay();
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-clickable-legend.png");

                tabs->setCurrentWidget(comparison);settleLayout();
                QJsonArray columnSizes,rowSizes;
                const auto labelSize=[](const QString& s,const QFont& font){
                    return std::max(double(QFontMetricsF(font).horizontalAdvance(s)),double(notation::size(s,font).width()));
                };
                for(int col=0;col<comparison->columnCount();++col){
                    const auto caption=comparison->horizontalHeaderItem(col)->text();
                    double needed=labelSize(caption,comparison->horizontalHeader()->font());
                    for(int i=0;i<comparison->rowCount();++i)
                        needed=std::max(needed,labelSize(comparison->item(i,col)->text(),comparison->font()));
                    columnSizes.append(QJsonObject{{"label",caption},{"size",comparison->columnWidth(col)},{"content",needed}});
                }
                for(int row=0;row<comparison->rowCount();++row){
                    const auto label=comparison->verticalHeaderItem(row)->text();
                    rowSizes.append(QJsonObject{{"label",label},{"size",comparison->verticalHeader()->width()},
                        {"content",labelSize(label,comparison->verticalHeader()->font())}});
                }
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-comparison-columns.png");

                const auto inspectStatus=[&]{
                    tabs->setCurrentWidget(result);settleLayout();
                    return inspectResultStatus(result);
                };
                const auto status=inspectStatus();
                hydroDialog_->grab().save(workflowOutput_+"/views/stability-status-first-column.png");
                tabs->setCurrentWidget(page);settleLayout();
                const bool legendAbove=first&&canvas&&first->mapTo(chart,QPoint{}).y()<canvas->mapTo(chart,QPoint{}).y();
                if(first)first->click();
                setUiLanguage("en");settleLayout();
                tabs->setCurrentWidget(page);settleLayout();
                const auto switched=overlay();
                const auto statusEn=inspectStatus();
                workflowArtifacts_["stability_presentation"]=QJsonObject{
                    {"legend_above",legendAbove},{"legend_buttons",legendButtons},
                    {"all",all},{"hidden",hidden},{"restored",restored},{"language_switched",switched},
                    {"result_unchanged",original==hydroDialog_->result()},
                    {"columns",columnSizes},{"row_headers",rowSizes},{"status",status},{"status_en",statusEn}};
            }
        }
        if(hydroRequest_["observe_tab_clicks"].toBool()){
            auto* curves=hydroDialog_->findChild<QWidget*>("HydroCurvePage");
            auto* report=hydroDialog_->findChild<QTextBrowser*>("HydroReport");
            auto* tabs=hydroDialog_->findChild<QTabWidget*>();
            QToolButton* coveredButton=nullptr;
            for(auto* button:studioBar_->buttons())
                if(button->isVisible()&&button->isEnabled()&&button->property("studio_hover_popup").toBool()){
                    coveredButton=button;break;
                }
            const bool ready=curves&&report&&tabs&&coveredButton&&!hydroDialog_->isModal();
            workflowCase("hydro-modeless-overlaps-toolbar",ready);
            if(ready){
                auto* bar=tabs->tabBar();
                const auto toolbarPosition=studioBar_->pos();
                QJsonArray clicks;
                for(int i=0;i<tabs->count();++i){
                    if(!tabs->isTabVisible(i))continue;
                    auto* page=tabs->widget(i);
                    const int index=tabs->indexOf(page);
                    const QPoint center=bar->tabRect(index).center();
                    // Align the visible tab with a menu button in the disabled owner,
                    // just as when the maximized dialog covers the main window.
                    const QPoint global=bar->mapToGlobal(center);
                    const auto buttonCenter=coveredButton->rect().center();
                    studioBar_->move(studioBar_->pos()+global-coveredButton->mapToGlobal(buttonCenter));
                    const bool overlaps=studioBar_->rect().contains(studioBar_->mapFromGlobal(global))&&
                                        coveredButton->rect().contains(coveredButton->mapFromGlobal(global));
                    auto* hit=hydroDialog_->childAt(hydroDialog_->mapFromGlobal(global));
                    if(hit){
                        const auto local=hit->mapFromGlobal(global);
                        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                        QApplication::sendEvent(hit,&press);
                        QApplication::sendEvent(hit,&release);
                    }
                    studioBar_->move(toolbarPosition);
                    settleLayout();
                    clicks.append(QJsonObject{{"tab",index},{"selected",tabs->currentIndex()},{"tab_at",bar->tabAt(center)},
                        {"hit",hit?QString::fromLatin1(hit->metaObject()->className()):QString{}},
                        {"overlaps_toolbar_button",overlaps},
                        {"page_visible",page->isVisible()}});
                    workflowCase("hydro-mouse-switches-tab-"+QString::number(index),
                                 overlaps&&hit==bar&&tabs->currentWidget()==page&&page->isVisible());
                }
                workflowArtifacts_["tab_clicks"]=clicks;
            }
        }
        for(auto* table:hydroDialog_->findChildren<QTableWidget*>()){
            if(!table->property("hydro_result_table").toBool())continue;
            // Auxiliary workflows can leave a different tab selected. Observe
            // the actual visible result page, not stale hidden-view geometry.
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>())if(tabs->indexOf(table)>=0)tabs->setCurrentWidget(table);
            settleLayout();table->ensurePolished();
            if(hydroRequest_["observe_draft_status"].toBool())
                workflowArtifacts_["draft_status"]=inspectResultStatus(table);
            // Observe the actual calculated result widget independently of CSV/report layout.
            QJsonArray headers,rows,widths;
            for(int c=0;c<table->columnCount();++c){headers.append(table->horizontalHeaderItem(c)?table->horizontalHeaderItem(c)->text():QString{});widths.append(table->columnWidth(c));}
            for(int r=0;r<std::min(table->rowCount(),500);++r){
                QJsonArray cells;QString key;QJsonValue raw;QString tooltip;
                for(int c=0;c<table->columnCount();++c){const auto* item=table->item(r,c);cells.append(item?item->text():QString{});
                    if(c==0&&item)key=item->data(Qt::UserRole).toString();
                    if(c==1&&item){tooltip=item->toolTip();if(item->data(Qt::UserRole+1).isValid())raw=item->data(Qt::UserRole+1).toDouble();}}
                rows.append(QJsonObject{{"cells",cells},{"field",key},{"raw",raw},{"tooltip",tooltip},{"span",table->columnSpan(r,0)}});
            }
            if(hydroRequest_["observe_report"].toBool()&&table->objectName()=="HydroSingleResult"){
                const auto originalSize=hydroDialog_->size();QJsonArray layouts;
                for(const auto size:QList<QSize>{{950,700},{1180,760},{1600,900}}){
                    hydroDialog_->resize(size);for(auto* layout:hydroDialog_->findChildren<QLayout*>())layout->activate();settleLayout();
                    int clipped=0;
                    int required[3];
                    for(int col=0;col<3;++col)required[col]=int(std::ceil(notation::size(
                        table->horizontalHeaderItem(col)->text(),table->horizontalHeader()->font()).width()))+14;
                    for(int at=0;at<table->rowCount();++at)for(int col=0;col<3;++col){
                        const auto* cell=table->item(at,col);if(!cell||table->columnSpan(at,0)>1)continue;
                        const int cellWidth=int(std::ceil(notation::size(cell->text(),table->font()).width()));
                        required[col]=std::max(required[col],cellWidth+14);
                        if(cellWidth+12>table->columnWidth(col))++clipped;
                    }
                    layouts.append(QJsonObject{{"width",hydroDialog_->width()},{"height",hydroDialog_->height()},
                        {"horizontal_range",table->horizontalScrollBar()->maximum()},{"clipped_cells",clipped},
                        {"label_width",table->columnWidth(0)},{"number_width",table->columnWidth(1)},
                        {"label_required",required[0]},{"number_required",required[1]}});
                }
                hydroDialog_->resize(originalSize);for(auto* layout:hydroDialog_->findChildren<QLayout*>())layout->activate();settleLayout();
                workflowArtifacts_["single_result_layouts"]=layouts;
            }
            if(hydroRequest_["observe_table_navigation"].toBool()&&table->objectName()=="HydroSingleResult"&&table->rowCount()>0){
                const auto position=table->viewport()->rect().center();
                const auto wheel=[&](int delta,Qt::KeyboardModifiers modifiers){
                    QWheelEvent event(position,table->viewport()->mapToGlobal(position),{},QPoint(0,delta),
                                      Qt::NoButton,modifiers,Qt::NoScrollPhase,false);
                    QApplication::sendEvent(table->viewport(),&event);settleLayout();
                };
                const auto font=table->font().pointSizeF();const auto headerFont=table->horizontalHeader()->font().pointSizeF();
                const auto initialHeight=table->rowHeight(0);
                auto* bar=table->verticalScrollBar();bar->setValue(0);
                wheel(-120,Qt::NoModifier);const bool plainScroll=bar->value()>0&&table->font().pointSizeF()==font;
                bar->setValue(0);
                wheel(120,Qt::ControlModifier);const auto zoomedFont=table->font().pointSizeF();
                const auto zoomedHeight=table->rowHeight(0);
                hydroDialog_->grab().save(workflowOutput_+"/views/table-single-result-zoomed.png");
                wheel(-120,Qt::ControlModifier);const auto restoredFont=table->font().pointSizeF();
                wheel(-12000,Qt::ControlModifier);
                const auto minimumFont=table->font().pointSizeF();
                const bool contentFits=!bar->maximum()&&!table->horizontalScrollBar()->maximum();
                wheel(-120,Qt::ControlModifier);
                const bool bounded=table->font().pointSizeF()==minimumFont;
                // Restore the original scale using the same wheel path.
                const auto steps=120*std::log(font/minimumFont)/std::log(1.12);
                wheel(qRound(steps),Qt::ControlModifier);
                bar->setValue(0);
                hydroDialog_->grab().save(workflowOutput_+"/views/table-single-result.png");
                auto* header=table->horizontalHeader();
                const int originalWidth=header->sectionSize(0);
                int longestRow=-1;qreal longestWidth=0;
                for(int row=0;row<table->rowCount();++row){
                    if(table->columnSpan(row,0)>1||!table->item(row,0))continue;
                    const auto width=notation::size(table->item(row,0)->text(),table->font()).width();
                    if(width>longestWidth){longestRow=row;longestWidth=width;}
                }
                if(longestRow>=0){
                    const int originalHeight=table->rowHeight(longestRow);
                    const QPoint from(header->sectionViewportPosition(0)+originalWidth-1,header->height()/2);
                    const QPoint to(header->sectionViewportPosition(0)+15,from.y());
                    auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons buttons){
                        QMouseEvent event(type,point,header->viewport()->mapToGlobal(point),button,buttons,Qt::NoModifier);
                        QApplication::sendEvent(header->viewport(),&event);
                    };
                    send(QEvent::MouseButtonPress,from,Qt::LeftButton,Qt::LeftButton);
                    send(QEvent::MouseMove,to,Qt::NoButton,Qt::LeftButton);
                    send(QEvent::MouseButtonRelease,to,Qt::LeftButton,Qt::NoButton);
                    settleLayout();
                    const int narrowWidth=header->sectionSize(0),wrappedHeight=table->rowHeight(longestRow);
                    table->scrollToItem(table->item(longestRow,0),QAbstractItemView::PositionAtCenter);
                    hydroDialog_->grab().save(workflowOutput_+"/views/table-single-result-narrow.png");
                    header->resizeSection(0,originalWidth);settleLayout();
                    workflowArtifacts_["single_result_wrap"]=QJsonObject{
                        {"original_width",originalWidth},{"narrow_width",narrowWidth},{"original_height",originalHeight},
                        {"wrapped_height",wrappedHeight},{"restored_height",table->rowHeight(longestRow)},
                        {"header_width",QFontMetrics(header->font()).horizontalAdvance(table->horizontalHeaderItem(0)->text())}};
                    bar->setValue(0);
                }
                workflowArtifacts_["single_result_navigation"]=QJsonObject{
                    {"font",font},{"header_font",headerFont},{"zoomed_font",zoomedFont},{"restored_font",restoredFont},
                    {"row_height",initialHeight},{"zoomed_row_height",zoomedHeight},
                    {"plain_scroll",plainScroll},{"content_fits",contentFits},{"bounded",bounded}};
            }
            workflowArtifacts_["result_table"]=QJsonObject{{"columns",table->columnCount()},{"rows",table->rowCount()},
                {"headers",headers},{"data",rows},{"widths",widths},{"viewport_width",table->viewport()->width()},
                {"horizontal_range",table->horizontalScrollBar()->maximum()-table->horizontalScrollBar()->minimum()},
                {"measurement_phase","visible_result_tab_settled"},{"visible",table->isVisibleTo(hydroDialog_)},{"horizontal_bar_visible",table->horizontalScrollBar()->isVisibleTo(hydroDialog_)},
                {"alternating",table->alternatingRowColors()},{"vertical_header",table->verticalHeader()->isVisibleTo(hydroDialog_)}};
        }
        if(!hydroRequest_["cancel"].toBool()){
            workflowCase("calculation-delivered",!hydroDialog_->result().isEmpty());
            if(hydroRequest_["export_auxiliary"].toBool()){
                auto* save=findButton("save_auxiliary");
                if(!save){unavailable("Auxiliary save callback unavailable or ambiguous");return;}
                QJsonArray artifacts;
                QJsonObject viewports;
                workflowCase("one-action-includes-auxiliary",hydroDialog_->result().contains("auxiliary"));
                bool extraRun=false;for(auto* button:hydroDialog_->findChildren<QPushButton*>())extraRun|=button->property("hydro_action").toString().startsWith("calculate_");
                workflowCase("no-separate-auxiliary-calculation",!extraRun);
                const int count=hydroRequest_["expected_auxiliary_count"].toInt(4);
                for(int i=0;i<count;++i){
                    const auto entry=hydroDialog_->auxiliaryFile(i);
                    if(entry.isEmpty()){workflowCase("auxiliary-missing-"+QString::number(i),false);continue;}
                    // Exercise the production selector's clicked connection, not only
                    // the private selection helper. Start from a different page.
                    hydroDialog_->selectAuxiliary(i==0?1:0);
                    QToolButton* choice=nullptr;
                    for(auto* button:hydroDialog_->findChildren<QToolButton*>())
                        if(button->property("auxiliary_index").isValid()&&button->property("auxiliary_index").toInt()==i)choice=button;
                    const bool available=choice&&choice->isVisibleTo(hydroDialog_)&&choice->isEnabled();
                    if(available)choice->click();
                    workflowCase("auxiliary-select-"+QString::number(i),available&&choice->isChecked());
                    for(auto* childLayout:hydroDialog_->findChildren<QLayout*>())childLayout->activate();settleLayout();
                    for(auto* view:hydroDialog_->findChildren<QTableView*>()){
                        if(!view->isVisibleTo(hydroDialog_)||!view->model()||view->objectName()!="AuxiliaryReportTable")continue;
                        workflowCase("auxiliary-table-"+QString::number(i),view->model()->rowCount()>0&&view->model()->columnCount()>=3);
                        auto* bar=view->horizontalScrollBar();
                        hydroDialog_->layout()->activate();
                        const auto* notice=hydroDialog_->findChild<QLabel*>("AuxiliaryReportNotice");
                        const auto* selector=hydroDialog_->findChild<QWidget*>("AuxiliaryReportSelector");
                        const auto* noticeBar=hydroDialog_->findChild<QWidget*>("AuxiliaryReportBar");
                        QJsonArray choices;
                        if(selector)for(const auto* button:selector->findChildren<QToolButton*>())
                            choices.append(QJsonObject{{"index",button->property("auxiliary_index").toInt()},
                                {"text",button->text()},{"selected",button->isChecked()},{"visible",button->isVisibleTo(hydroDialog_)}});
                        QJsonArray layerLabels;
                        if(i==2)for(int row=0;row<view->model()->rowCount();++row)
                            layerLabels.append(view->model()->index(row,0).data().toString());
                        QJsonObject observation{{"pixel_mode",view->horizontalScrollMode()==QAbstractItemView::ScrollPerPixel},
                            {"styled",!bar->styleSheet().isEmpty()},{"rows",view->model()->rowCount()},
                            {"notice",notice&&notice->isVisibleTo(hydroDialog_)?notice->text():QString{}},
                            {"notice_instances",hydroDialog_->findChildren<QLabel*>("AuxiliaryReportNotice").size()},
                            {"notice_layout",noticeBar?noticeBar->property("studio_warning_layout").toString():QString{}},
                            {"notice_inside",notice&&noticeBar&&noticeBar->rect().contains(QRect(notice->mapTo(noticeBar,QPoint{}),notice->size()))},
                            {"notice_inside_dialog",notice&&hydroDialog_->rect().contains(QRect(notice->mapTo(hydroDialog_,QPoint{}),notice->size()))},
                            {"notice_bottom",notice?notice->mapTo(hydroDialog_,QPoint(0,notice->height())).y():-1},
                            {"selector_top",selector?selector->mapTo(hydroDialog_,QPoint{}).y():-1},
                            {"notice_above_selector",notice&&selector&&notice->isVisibleTo(hydroDialog_)&&
                                notice->mapTo(hydroDialog_,QPoint(0,notice->height())).y()<=selector->mapTo(hydroDialog_,QPoint{}).y()},
                            {"choices",choices},{"layer_labels",layerLabels},
                            {"tab_containers",hydroDialog_->findChildren<QTabWidget*>().size()},
                            {"area_charts",hydroDialog_->findChildren<QWidget*>("BonjeanAreaCurve").size()+hydroDialog_->findChildren<QWidget*>("SectionAreaCurve").size()}};
                        if(hydroRequest_["observe_table_navigation"].toBool()){
                            // Large tables fit columns over several event-loop turns.
                            // Check final widths only after that production work completes.
                            QElapsedTimer measurement;measurement.start();
                            while(view->property("studio_table_measure_pending").toBool()&&measurement.elapsed()<15000){
                                QEventLoop turn;QTimer::singleShot(10,&turn,&QEventLoop::quit);turn.exec(QEventLoop::ExcludeUserInputEvents);
                            }
                            workflowCase("auxiliary-measure-complete-"+QString::number(i),!view->property("studio_table_measure_pending").toBool());
                            observation["native_table_navigation"]=view->property("studio_table_navigation").toBool();
                            QJsonArray sizes;
                            const auto textWidth=[](QString text,const QFont& font){return notation::contains(text)?notation::size(text,font).width():QFontMetricsF(font).horizontalAdvance(text);};
                            for(int column=0;column<view->model()->columnCount();++column){
                                double width=textWidth(view->model()->headerData(column,Qt::Horizontal).toString(),view->horizontalHeader()->font());
                                for(int row=0;row<view->model()->rowCount();++row)
                                    width=std::max(width,textWidth(view->model()->index(row,column).data().toString(),view->font()));
                                sizes.append(QJsonObject{{"width",view->columnWidth(column)},{"required",int(std::ceil(width))+14}});
                            }
                            observation["column_sizes"]=sizes;
                            const double scale=view->property("studio_table_scale").toDouble();
                            const auto position=view->viewport()->rect().center();
                            auto wheel=[&](int delta){QWheelEvent event(position,view->viewport()->mapToGlobal(position),{},QPoint(0,delta),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(view->viewport(),&event);settleLayout();};
                            wheel(120);const double zoomed=view->property("studio_table_scale").toDouble();
                            wheel(-120);observation["navigation"]=QJsonObject{{"initial_scale",scale},{"zoomed_scale",zoomed},{"restored_scale",view->property("studio_table_scale").toDouble()}};
                        }
                        const int previous=bar->value();
                        bar->setValue(bar->minimum());
                        bar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
                        observation["horizontal_range"]=bar->maximum()-bar->minimum();
                        observation["single_step_pixels"]=bar->value()-bar->minimum();
                        bar->setValue(previous);
                        viewports[entry["name"].toString()]=observation;
                    }
                    const auto path=workflowOutput_+"/"+entry["name"].toString();
                    save->setProperty("probe_save_path",path);save->click();QFile file(path);
                    workflowCase("auxiliary-save-"+QString::number(i),save->property("export_error").toString().isEmpty()&&file.open(QIODevice::ReadOnly)&&file.readAll()==entry["text"].toString().toUtf8());
                    captureMessages("auxiliary-"+QString::number(i));
                    artifacts.append(path);workflowCase("auxiliary-image-"+QString::number(i),hydroDialog_->grab().save(workflowOutput_+"/views/auxiliary-"+QString::number(i)+".png"));
                }
                workflowCase("auxiliary-count",artifacts.size()==count);
                save->setProperty("probe_save_path",QString{});workflowArtifacts_["auxiliary_files"]=artifacts;
                workflowArtifacts_["auxiliary_viewports"]=viewports;
                if(hydroRequest_["observe_table_navigation"].toBool()){
                    const auto originalSize=hydroDialog_->size();QJsonArray layouts;
                    hydroDialog_->selectAuxiliary(0);
                    for(const auto& size:{QSize(920,720),QSize(1480,860)}){
                        hydroDialog_->resize(size);
                        for(int pass=0;pass<3;++pass){for(auto* layout:hydroDialog_->findChildren<QLayout*>())layout->activate();settleLayout();}
                        const auto* notice=hydroDialog_->findChild<QLabel*>("AuxiliaryReportNotice");
                        const auto* selector=hydroDialog_->findChild<QWidget*>("AuxiliaryReportSelector");
                        const auto* host=hydroDialog_->findChild<QWidget*>("AuxiliaryReportBar");
                        layouts.append(QJsonObject{{"width",hydroDialog_->width()},{"layout",host?host->property("studio_warning_layout").toString():QString{}},
                            {"has_warning",notice&&!notice->text().isEmpty()},
                            {"inside",notice&&hydroDialog_->rect().contains(QRect(notice->mapTo(hydroDialog_,QPoint{}),notice->size()))},
                            {"above_selector",notice&&selector&&notice->mapTo(hydroDialog_,QPoint(0,notice->height())).y()<=selector->mapTo(hydroDialog_,QPoint{}).y()}});
                        hydroDialog_->grab().save(workflowOutput_+"/views/table-layout-"+QString::number(size.width())+".png");
                    }
                    workflowArtifacts_["table_responsive_layouts"]=layouts;
                    hydroDialog_->resize(originalSize);for(int pass=0;pass<3;++pass)settleLayout();
                    for(auto* view:hydroDialog_->findChildren<QTableView*>())if(view->isVisibleTo(hydroDialog_)&&view->objectName()=="AuxiliaryReportTable"){
                        const auto point=view->viewport()->rect().center();
                        for(int step=0;step<32;++step){QWheelEvent wheel(point,view->viewport()->mapToGlobal(point),{},QPoint(0,-120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(view->viewport(),&wheel);}
                        settleLayout();workflowArtifacts_["production_table_fit"]=QJsonObject{{"scale",view->property("studio_table_scale").toDouble()},
                            {"horizontal_range",view->horizontalScrollBar()->maximum()},{"vertical_range",view->verticalScrollBar()->maximum()}};
                        hydroDialog_->grab().save(workflowOutput_+"/views/table-fit.png");
                    }
                }
                const auto invalidPath=workflowOutput_+"/absent-parent/report.txt";
                workflowCase("auxiliary-failed-save",!hydroDialog_->exportAuxiliary(invalidPath,0).isEmpty());
                workflowCase("auxiliary-invalid-index",!hydroDialog_->exportAuxiliary(workflowOutput_+"/invalid.txt",-1).isEmpty()&&!QFileInfo::exists(workflowOutput_+"/invalid.txt"));
            }
            for(const auto& format:{QString("json"),QString("csv"),QString("txt"),QString("pdf")}){
                const auto path=workflowOutput_+"/hydro-report."+format;auto error=hydroDialog_->exportReport(path,format);workflowCase("export-"+format,error.isEmpty(),error);workflowArtifacts_["report_"+format]=path;
            }
            if(hydroRequest_["observe_report"].toBool()){
                auto* browser=hydroDialog_->findChild<QTextBrowser*>("HydroReport");QJsonArray tables;
                if(browser){
                    for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>())if(tabs->indexOf(browser)>=0)tabs->setCurrentWidget(browser);
                    QApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest);
                    for(auto* frame:browser->document()->rootFrame()->childFrames())
                        if(auto* table=qobject_cast<QTextTable*>(frame);table&&report::messageRole(table).isEmpty()){
                            const auto bounds=browser->document()->documentLayout()->frameBoundingRect(table);
                            tables.append(QJsonObject{{"rows",table->rows()},{"columns",table->columns()},
                                {"width",bounds.width()},{"viewport_width",browser->viewport()->width()},
                                {"heading",table->cellAt(0,0).firstCursorPosition().block().text()}});
                        }
                    for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>())
                        if(tabs->indexOf(browser)>=0)tabs->setCurrentWidget(browser);
                    workflowCase("report-preview",hydroDialog_->grab().save(workflowOutput_+"/views/report.png"));
                    browser->verticalScrollBar()->setValue(browser->verticalScrollBar()->maximum());
                    workflowCase("report-message-preview",hydroDialog_->grab().save(workflowOutput_+"/views/report-messages.png"));
                    if(hydroRequest_["language"].isString())
                        workflowArtifacts_["report_plain_text"]=browser->toPlainText();
                    if(hydroRequest_["observe_report_navigation"].toBool())
                        workflowArtifacts_["report_navigation"]=probeReportNavigation(*browser);
                }
                workflowArtifacts_["report_tables"]=tables;
                workflowArtifacts_["report_messages"]=reportMessageProbe(browser);workflowArtifacts_["report_subscripts"]=reportSubscriptProbe(browser);
            }
            if(hydroRequest_["observe_format_icons"].toBool()){
                QJsonObject controls;
                for(auto* button:hydroDialog_->findChildren<QPushButton*>()){
                    const auto format=button->property("hydro_export").toString();
                    if(!format.isEmpty())controls["hydro-"+format]=buttonObservation(button);
                }
                PlatesDialog plates(session_,true);
                for(auto* button:plates.findChildren<QPushButton*>()){
                    const auto operation=button->property("plates_action").toString();
                    if(QStringList{"dxf","txt","json","png","pdf"}.contains(operation))
                        controls["plates-"+operation]=buttonObservation(button);
                }
                PerformanceDialog performance(session_,{{"calculator","delft"},{"title","Export preview"}},true,this);
                for(auto* button:performance.findChildren<QPushButton*>()){
                    const auto format=button->property("performance_export").toString();
                    if(!format.isEmpty())controls["performance-"+format]=buttonObservation(button);
                }
                const auto actions=findChildren<QAction*>();
                for(const auto* action:actions){
                    const auto operation=action->property("operation").toString();
                    if(!operation.startsWith("export_"))continue;
                    const auto format=operation.mid(7);
                    const auto extension=fs::cad::extension(format).toUpper();
                    auto evidence=iconObservation(action->icon());evidence["extension"]=extension;
                    controls["menu-"+format]=evidence;
                    ExchangeDialog exchange(session_,false,format);
                    for(auto* button:exchange.findChildren<QPushButton*>())
                        if(button->property("exchange_action")=="export"){
                            auto detail=buttonObservation(button);detail["extension"]=extension;
                            controls["exchange-"+format]=detail;
                        }
                }
                if(auto* toolbar=linesDialog_->findChild<QToolBar*>()){
                    for(auto* action:{linesPng_,dxf_}){
                        auto evidence=iconObservation(action->icon());
                        if(const auto* button=qobject_cast<QToolButton*>(toolbar->widgetForAction(action)))
                            evidence["icon_only"]=button->toolButtonStyle()==Qt::ToolButtonIconOnly;
                        controls["lines-"+action->text().toLower()]=evidence;
                    }
                    toolbar->grab().save(workflowOutput_+"/views/lines-export-icons.png");
                }
                workflowArtifacts_["format_controls"]=controls;
            }
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);workflowCase("image-"+QString::number(i),hydroDialog_->grab().save(workflowOutput_+"/views/results-"+QString::number(i)+".png"));}
        }else workflowCase("cancel-reported",hydroDialog_->property("hydro_cancelled").toBool()||!hydroDialog_->property("hydro_error").toString().isEmpty());
        if(hydroRequest_["repeat_cancel"].toBool()){
            auto* run=findButton("calculate");auto* cancel=findButton("cancel");
            if(run&&cancel){workflowArtifacts_["result_before_cancel"]=resultHash();const auto before=hydroDialog_->result();workflowArtifacts_["values_before_cancel"]=QJsonObject{{"rows",before["rows"]},{"auxiliary",before["auxiliary"]}};*stage=3;run->click();cancel->click();return;}
            unavailable("Repeated cancellation callback unavailable");return;
        }
        if(hydroRequest_["save_table_range"].toBool()){
            QFile source(session_.sourcePath());
            if(source.open(QIODevice::ReadOnly))
                workflowArtifacts_["file_before_save"]=QString::fromLatin1(QCryptographicHash::hash(source.readAll(),QCryptographicHash::Sha256).toHex());
            source.close();
            workflowCase("hydro.dialog-has-save-shortcut",hydroDialog_->actions().contains(save_)&&!save_->shortcuts().isEmpty());
            *stage=2;save_->trigger();return;
        }
        timer->stop();timer->deleteLater();
        hydroDialog_->reject();
    });
    timer->start();candidates.first()->trigger();
}
