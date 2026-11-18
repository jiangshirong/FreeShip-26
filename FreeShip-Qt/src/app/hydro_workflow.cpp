// Hidden production action observer; no native desktop automation.
#include "main_window.h"
#include "hydro_dialog.h"
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
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QLineEdit>
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
#include <QWheelEvent>
#include <QMouseEvent>
#include "math_notation.h"
#include <cmath>
#include <QLayout>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QSignalBlocker>
#include <QEventLoop>
#include <stdexcept>

namespace {
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
                    else if(auto* n=qobject_cast<QLineEdit*>(w)){QStringList parts;for(auto v:values[key].toArray())parts<<QString::number(v.toDouble(),'g',17);n->setText(parts.join(","));}
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
        if(*stage==0){
            workflowArtifacts_["initial_inputs"]=hydroDialog_->savedInputs();
            if(hydroRequest_["switch_language_after_open"].isString()){
                const auto error=setUiLanguage(hydroRequest_["switch_language_after_open"].toString());
                if(!error.isEmpty()){unavailable("Hydro language switch failed: "+error);return;}
                workflowArtifacts_["language_switched_after_open"]=uiLanguage();
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
            for(auto* widget:hydroDialog_->findChildren<QWidget*>()){
                const auto key=widget->property("field").toString();if(key.isEmpty())continue;discovered.append(key);if(!values.contains(key))continue;used.insert(key);
                if(auto* n=qobject_cast<QDoubleSpinBox*>(widget))n->setValue(values[key].toDouble());
                else if(auto* n=qobject_cast<QCheckBox*>(widget))n->setChecked(values[key].toBool());
                else if(auto* n=qobject_cast<QLineEdit*>(widget)){
                    QStringList parts;for(auto v:values[key].toArray())parts<<QString::number(v.toDouble(),'g',17);
                    n->setText(values[key].isString()?values[key].toString():parts.join(","));
                }
            }
            workflowArtifacts_["discovered_fields"]=discovered;
            for(auto it=values.begin();it!=values.end();++it)if(it.key()!="mode"&&!used.contains(it.key())){unavailable("Hydro input not discovered: "+it.key());return;}
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
        if(auto* page=hydroDialog_->findChild<QWidget*>("HydroCurvePage")){
            for(auto* tabs:hydroDialog_->findChildren<QTabWidget*>()){
                const int index=tabs->indexOf(page);
                if(index>=0)workflowArtifacts_["curves_tab_visible"]=tabs->isTabVisible(index);
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
                for(auto* page:{curves,static_cast<QWidget*>(report),tabs->widget(0)}){
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
                    hydroDialog_->selectAuxiliary((i+1)%4);
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
