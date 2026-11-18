// Hidden production callbacks and evidence; never desktop automation.
#include "main_window.h"
#include "performance_dialog.h"
#include "ui_language.h"
#include "grid_preview_panel.h"
#include "calculation_plot.h"
#include "math_notation.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontMetricsF>
#include <QCryptographicHash>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QElapsedTimer>
#include <QLayout>
#include <QInputDialog>
#include <QApplication>
#include <QTableView>
#include <QHeaderView>
#include <QWheelEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include "report_message_probe.h"
#include "report_layout.h"
#include <QTextBrowser>
#include "report_navigation_probe.h"
#include <QTextDocument>
#include <QTextTable>
#include <QToolButton>
#include <QMouseEvent>
namespace {
void revealParameter(QWidget* dialog,QWidget* target){
    for(auto* tabs:dialog->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)
        if(tabs->isTabVisible(i)&&(tabs->widget(i)==target||tabs->widget(i)->isAncestorOf(target)))tabs->setCurrentIndex(i);
}
QJsonArray observeCalculationLayout(PerformanceDialog* dialog,const QString& output,bool images){
    const auto originalFont=dialog->font();const auto originalSize=dialog->size();const auto originalInputs=dialog->inputs();
    auto settle=[] {QCoreApplication::sendPostedEvents();QCoreApplication::processEvents();};
    QJsonArray observations;
    for(int pointSize:{9,14,18})for(bool compact:{false,true}){
        auto font=originalFont;font.setPointSize(pointSize);dialog->setFont(font);const QSize requested=compact?QSize(860,600):QSize(1180,760);dialog->resize(requested);settle();
        QJsonArray fields,pages,actions,groupTitles;
        for(auto* w:dialog->findChildren<QWidget*>()){
            const auto key=w->property("performance_field").toString();if(key.isEmpty())continue;
            const bool declared=w->property("calculation_declared_visible").toBool();
            QJsonObject item{{"key",key},{"page",w->property("calculation_parameter_page").toString()},
                {"section",w->property("calculation_parameter_section").toString()},{"declared_visible",declared}};
            if(auto* number=qobject_cast<QDoubleSpinBox*>(w))if(auto* edit=number->findChild<QLineEdit*>()){
                const auto margins=edit->textMargins();
                item["numeric"]=true;item["text"]=edit->text();item["text_width"]=QFontMetricsF(edit->font()).horizontalAdvance(edit->text());
                item["edit_width"]=edit->contentsRect().width()-margins.left()-margins.right();
                item["numeric_width"]=number->width();item["measured_numeric_width"]=number->property("studio_numeric_content_width").toInt();
            }
            if(declared){
                revealParameter(dialog,w);QScrollArea* scroll=nullptr;
                for(auto* parent=w->parentWidget();parent;parent=parent->parentWidget())if((scroll=qobject_cast<QScrollArea*>(parent)))break;
                if(scroll){
                    settle();scroll->ensureWidgetVisible(w);settle();
                    // QScrollArea may reveal a spin box's inner edit rectangle
                    // only. Move the scrollbars as a user can, then observe the
                    // complete control; do not confuse its border with overflow.
                    const QPoint center=w->mapTo(scroll->viewport(),w->rect().center());
                    scroll->horizontalScrollBar()->setValue(scroll->horizontalScrollBar()->value()+center.x()-scroll->viewport()->width()/2);
                    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->value()+center.y()-scroll->viewport()->height()/2);
                    settle();const QRect rect(w->mapTo(scroll->viewport(),QPoint{}),w->size());
                    item["reachable"]=w->isVisible()&&scroll->viewport()->rect().contains(rect);
                    item["rect"]=QJsonArray{rect.x(),rect.y(),rect.width(),rect.height()};item["viewport"]=QJsonArray{scroll->viewport()->width(),scroll->viewport()->height()};}
                if(key=="A11Box"&&pointSize==14&&dialog->inputs()["calculator"]=="holtrop"){
                    if(auto* combo=qobject_cast<QComboBox*>(w)){
                        auto* view=combo->view();
                        combo->showPopup();settle();
                        item["choices_width"]=view->sizeHintForColumn(combo->modelColumn());
                        item["popup_width"]=view->viewport()->width();
                        item["popup_visible"]=view->isVisible();
                        combo->hidePopup();
                        const int minimum=combo->minimumWidth(),maximum=combo->maximumWidth();
                        combo->setFixedWidth(100);settle();
                        combo->showPopup();settle();
                        item["narrow_popup_fits"]=view->isVisible()&&
                            view->viewport()->width()>=item["choices_width"].toInt();
                        combo->hidePopup();
                        combo->setMinimumWidth(minimum);combo->setMaximumWidth(maximum);settle();
                    }
                }
                for(auto* label:dialog->findChildren<QLabel*>())if(label->buddy()==w){item["label_fits"]=label->height()>=label->heightForWidth(label->width());item["label_text"]=label->text();break;}
            }
            fields.append(item);
        }
        for(auto* group:dialog->findChildren<QGroupBox*>())groupTitles.append(group->title());
        for(auto* tabs:dialog->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool()){
            for(int i=0;i<tabs->count();++i){const auto page=tabs->widget(i)->property("calculation_page").toString();
                int charts=0;
                for(auto* widget:tabs->widget(i)->findChildren<QWidget*>())
                    if(widget->property("performance_chart").toBool())++charts;
                pages.append(QJsonObject{{"page",page},{"visible",tabs->isTabVisible(i)},{"charts",charts}});
                if(!tabs->isTabVisible(i))continue;
                tabs->setCurrentIndex(i);settle();
                if(images&&pointSize==14){for(auto* scroll:tabs->widget(i)->findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(0);
                    if(auto* scroll=qobject_cast<QScrollArea*>(tabs->widget(i)))scroll->verticalScrollBar()->setValue(0);
                    settle();
                    const auto path=output+"/views/layout-"+page+(compact?"-compact":"")+".png";if(!dialog->grab().save(path))pages.append(QJsonObject{{"image_error",path}});}
            }
            tabs->setCurrentIndex(0);
        }
        for(auto* button:dialog->findChildren<QPushButton*>())if(button->isVisible()){
            for(auto* parent=button->parentWidget();parent;parent=parent->parentWidget())
                if(auto* scroll=qobject_cast<QScrollArea*>(parent)){
                    scroll->ensureWidgetVisible(button,0,0);settle();break;
                }
            const QRect rect(button->mapTo(dialog,QPoint{}),button->size());actions.append(QJsonObject{{"action",button->property("performance_action").toString()},{"text",button->text()},{"contained",dialog->rect().contains(rect)}});
        }
        observations.append(QJsonObject{{"font",pointSize},{"compact",compact},{"requested_size",QJsonArray{requested.width(),requested.height()}},{"actual_size",QJsonArray{dialog->width(),dialog->height()}},{"fields",fields},{"pages",pages},{"actions",actions},{"groups",groupTitles},{"inputs_unchanged",dialog->inputs()==originalInputs}});
    }
    dialog->setFont(originalFont);dialog->resize(originalSize);settle();return observations;
}
}
void MainWindow::verifyPerformance(const QString& request,const QString& output){
    workflow_=true;performanceWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read performance workflow");return;}
    performanceRequest_=QJsonDocument::fromJson(f.readAll()).object();
    // Documentation screenshots observe settled pages, not the outgoing
    // animation layer. This switch belongs only to this isolated QA workflow.
    if(performanceRequest_["observe_method_help"].toBool())qApp->setProperty("studio_motion_enabled",false);
    if(performanceRequest_.contains("language")){
        const auto error=setUiLanguage(performanceRequest_["language"].toString());
        if(!error.isEmpty()){finishWorkflow(error);return;}
    }
    if(!QDir().mkpath(workflowOutput_+"/views")){finishWorkflow("Cannot create performance evidence");return;}
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();openModel(performanceRequest_["model"].toString());
}
void MainWindow::performanceStep(const QString& operation){
    if(operation=="closed"){captureWorkflow("after");workflowCase("performance.document-revision",session_.model()->revision>=quint64(workflowArtifacts_["before_revision"].toDouble()));finishWorkflow();return;}
    if(operation=="performance"&&workflowArtifacts_["preparing_performance"].toBool()){
        workflowArtifacts_["preparing_performance"]=false;workflowArtifacts_["prepared_performance"]=true;
        QTimer::singleShot(0,this,[this]{performanceStep("load");});return;
    }
    if(operation!="load")return;
    if(performanceRequest_["prepare"].isObject()&&!workflowArtifacts_["prepared_performance"].toBool()){
        if(performanceRequest_["prepare_via_ui"].toBool()){
            const auto request=performanceRequest_["prepare"].toObject();
            const auto method=request["calculator"].toString();
            QAction* entry=nullptr;
            for(auto* action:findChildren<QAction*>())if(action->property("operation")=="performance_"+method){entry=action;break;}
            if(!entry||!entry->isEnabled()){finishWorkflow("Cannot open prerequisite calculation");return;}
            auto* wait=new QTimer(this);wait->setInterval(10);
            auto configured=std::make_shared<bool>(false);
            auto started=std::make_shared<bool>(false);
            connect(wait,&QTimer::timeout,this,[this,wait,configured,started,request,method]{
                auto* dialog=performanceDialog_;
                if(!dialog||dialog->property("calculation_task_key")!="performance:"+method)return;
                auto* worker=dynamic_cast<ModelSession*>(dialog->property("calculation_session").value<QObject*>());
                if(!worker||worker->busy()||session_.busy()||dialog->choicesPending())return;
                if(!*configured){
                    const auto values=request["values"].toObject();
                    if(request.contains("solver_directory"))dialog->useSolverDirectory(request["solver_directory"].toString());
                    for(auto* field:dialog->findChildren<QWidget*>()){
                        const auto key=field->property("performance_field").toString();
                        if(!values.contains(key))continue;
                        auto value=values[key];
                        if(auto* number=qobject_cast<QDoubleSpinBox*>(field))number->setValue(value.toDouble());
                        else if(auto* box=qobject_cast<QCheckBox*>(field))box->setChecked(value.toBool());
                        else if(auto* choice=qobject_cast<QComboBox*>(field))choice->setCurrentIndex(value.toInt());
                    }
                    *configured=true;
                    return;
                }
                if(!*started){
                    for(auto* button:dialog->findChildren<QPushButton*>())
                        if(button->property("performance_action")=="calculate"&&button->isEnabled()){
                            *started=true;button->click();return;
                        }
                    wait->stop();finishWorkflow("Prerequisite Calculate button is disabled");return;
                }
                if(!dialog->property("performance_finished").toBool())return;
                wait->stop();wait->deleteLater();
                const auto problem=dialog->property("performance_error").toString();
                if(!problem.isEmpty()||dialog->result()["series"].toArray().isEmpty()){
                    finishWorkflow("Prerequisite calculation failed: "+problem);return;
                }
                workflowCase("prerequisite-calculated-in-window",true);
                workflowArtifacts_["prepared_performance"]=true;
                dialog->hide();
                QTimer::singleShot(0,this,[this]{performanceStep("load");});
            });
            wait->start();entry->trigger();return;
        }
        workflowArtifacts_["preparing_performance"]=true;
        if(!session_.calculatePerformance(performanceRequest_["prepare"].toObject()))finishWorkflow("Cannot prepare prerequisite calculation");
        return;
    }
    captureWorkflow("original");workflowArtifacts_["before_revision"]=double(session_.model()->revision);
    const auto method=performanceRequest_["inputs"].toObject()["calculator"].toString();QList<QAction*> candidates;
    for(auto* a:findChildren<QAction*>())if(a->property("operation")=="performance_"+method)candidates.append(a);
    QJsonArray observed;for(auto* a:candidates)observed.append(QJsonObject{{"text",a->text()},{"enabled",a->isEnabled()}});workflowArtifacts_["action_candidates"]=observed;
    if(candidates.size()!=1||!candidates.first()->isEnabled()){workflowCases_.append(QJsonObject{{"name","performance_entry"},{"status","not_run"},{"message","Action absent, ambiguous or disabled"}});finishWorkflow();return;}
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!performanceDialog_||!performanceDialog_->isVisible()||performanceDialog_->choicesPending()||session_.busy())return;
        if(auto* task=dynamic_cast<ModelSession*>(performanceDialog_->property("calculation_session").value<QObject*>()))if(task->busy())return;
        if(*stage==9)return; // Layout observation may process queued Qt events.
        if(*stage==10){timer->stop();timer->deleteLater();performanceDialog_->reject();return;}
        auto button=[this](QString op){QList<QPushButton*> found;for(auto* b:performanceDialog_->findChildren<QPushButton*>())if(b->property("performance_action")==op)found.append(b);return found.size()==1?found.first():nullptr;};
        auto unavailable=[this,timer](QString reason){workflowCases_.append(QJsonObject{{"name","performance_entry"},{"status","not_run"},{"message",reason}});timer->stop();timer->deleteLater();performanceDialog_->reject();};
        if(*stage==8){auto* preview=performanceDialog_->gridPreview();if(preview->pending())return;
            workflowCase("input-grid-preview",preview->current(),preview->evidence()["error"].toString());workflowArtifacts_["input_grid_preview"]=preview->evidence();workflowArtifacts_["preview_done"]=true;
            workflowCase("preview-image",preview->grab().save(workflowOutput_+"/views/input-grid.png"));*stage=6;return;}
        if(*stage==4){
            workflowCase("parameter-file-roundtrip",performanceDialog_->inputs()["values"]==workflowArtifacts_["preset_values"]);
            for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())workflowCase("preset-retains-page",tabs->currentWidget()->property("calculation_page").toString()==workflowArtifacts_["preset_page"].toString());
            timer->stop();timer->deleteLater();performanceDialog_->reject();return;
        }
        if(*stage==0){
            workflowArtifacts_["initial_inputs"]=performanceDialog_->inputs()["values"];
            if(performanceRequest_["observe_input_feedback"].toBool()){
                bool genericHint=false;
                for(auto* label:performanceDialog_->findChildren<QLabel*>())
                    if(label->isVisible()&&label->text().contains(uiText("部分输入需要当前船模的几何或前序计算结果，请核对参数。")))genericHint=true;
                workflowCase("no-generic-input-banner",!genericHint);
                if(auto* converter=button("water_temperature")){
                    const auto key=performanceDialog_->inputs()["calculator"]=="maneuver"?"Edit11":"WaterViscosity";
                    auto* parent=converter->parentWidget();
                    const auto y=converter->geometry().center().y();
                    bool inlineRow=false;
                    for(auto* spin:parent->findChildren<QDoubleSpinBox*>(QString{},Qt::FindDirectChildrenOnly))
                        if(spin->property("performance_field")==key)
                            inlineRow=converter->x()>spin->geometry().right()&&std::abs(y-spin->geometry().center().y())<=2;
                    workflowCase("temperature-converter-inline",inlineRow);
                }
            }
            workflowCase("presentation.no-extract-button",!button("extract"));
            workflowCase("presentation.no-manual-model-parameter-save",!button("save_settings"));
            if(performanceDialog_->inputs()["calculator"]=="wind")
                workflowArtifacts_["initial_wind_inputs"]=performanceDialog_->inputs()["values"];
            const auto inputs=performanceRequest_["inputs"].toObject(),values=inputs["values"].toObject();QSet<QString> used;QJsonArray fields;
            if(inputs.contains("solver_directory"))performanceDialog_->useSolverDirectory(inputs["solver_directory"].toString());
            for(auto* w:performanceDialog_->findChildren<QWidget*>()){
                const auto key=w->property("performance_field").toString();if(key.isEmpty())continue;fields.append(QJsonObject{{"key",key},{"visible",w->isVisible()},{"enabled",w->isEnabled()}});if(!values.contains(key))continue;used.insert(key);
                if(auto* n=qobject_cast<QDoubleSpinBox*>(w))n->setValue(values[key].toDouble());else if(auto* n=qobject_cast<QCheckBox*>(w))n->setChecked(values[key].toBool());else if(auto* n=qobject_cast<QComboBox*>(w))n->setCurrentIndex(values[key].toInt());
            }
            workflowArtifacts_["discovered_fields"]=fields;
            if(performanceRequest_["verify_presentation"].toBool()){
                bool environmentControls=false;int seriesSelectors=0;bool selectorInDataPage=true;
                for(auto* w:performanceDialog_->findChildren<QWidget*>()){
                    environmentControls|=w->property("performance_directory").toBool()||w->property("performance_action")=="check_tools";
                    if(w->property("performance_series").toBool()){
                        ++seriesSelectors;bool inDataPage=false;
                        for(auto* p=w->parentWidget();p;p=p->parentWidget())if(p->layout())for(int i=0;i<p->layout()->count();++i){auto* child=p->layout()->itemAt(i)->widget();if(child&&child->property("performance_table").toBool())inDataPage=true;}
                        selectorInDataPage&=inDataPage&&!w->isVisible();
                    }
                }
                workflowCase("presentation.no-environment-controls-in-calculator",!environmentControls);
                const bool wind=performanceDialog_->inputs()["calculator"]=="wind";
                workflowCase("presentation.table-dataset-selector",seriesSelectors==1&&selectorInDataPage);
                if(wind)workflowCase("presentation.wind-has-no-extract-button",!button("extract"));
                for(auto* w:performanceDialog_->findChildren<QWidget*>())if(w->property("performance_chart").toBool())workflowCase("presentation.legend-only-visibility-controls",w->findChildren<QCheckBox*>().isEmpty());
            }
            if(performanceRequest_["verify_calculation_tools"].toBool()){
                auto* inspect=button("check_tools");
                if(!inspect||!inspect->isVisible()||!inspect->isEnabled()){unavailable("Tool check callback unavailable");return;}
                inspect->click();workflowArtifacts_["calculation_tools_inventory"]=performanceDialog_->property("calculation_tools_inventory").toJsonObject();
                for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())tabs->setCurrentIndex(0);
            }
            for(auto it=values.begin();it!=values.end();++it)if(!used.contains(it.key())){unavailable("Input missing or unavailable: "+it.key());return;}
            if(performanceRequest_.contains("water_temperature")) {
                auto* convert=button("water_temperature");if(!convert){unavailable("Temperature conversion unavailable");return;}
                const QJsonValue before=performanceDialog_->inputs()["values"];
                QTimer::singleShot(0,this,[this] {
                    auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                    if(!dialog){finishWorkflow("Temperature dialog did not open");return;}
                    dialog->setDoubleValue(performanceRequest_["water_temperature"].toDouble());
                    if(performanceRequest_["cancel_temperature"].toBool())dialog->reject();else dialog->accept();
                });
                convert->click();
                if(performanceRequest_["cancel_temperature"].toBool())workflowCase("temperature-cancel-no-change",performanceDialog_->inputs()["values"]==before);
            }
            // Allow the same queued dependent-choice update used during manual
            // editing to finish before invoking Calculate.
            *stage=6;return;
        }
        if(*stage==6){
            if((performanceRequest_.contains("load_development_sample")||performanceRequest_.contains("observe_development_sample"))&&
                !workflowArtifacts_.contains("development_sample_button")){
                auto* sample=button("load_development_sample");
                workflowArtifacts_["development_sample_button"]=sample&&sample->isVisible();
                if(performanceRequest_["load_development_sample"].toBool()){
                    if(!sample||!sample->isEnabled()){unavailable("Development sample button unavailable");return;}
                    workflowArtifacts_["development_sample_button_text"]=sample->text();
                    sample->click();
                    return;
                }
            }
            if(performanceRequest_["load_development_sample"].toBool()&&
                !workflowArtifacts_.contains("development_sample_loaded")){
                const auto* run=button("calculate");
                workflowArtifacts_["development_sample_loaded"]=performanceDialog_->usingDevelopmentSample();
                workflowArtifacts_["development_sample_can_calculate"]=run&&run->isEnabled();
            }
            if(performanceRequest_.contains("project_change")&&!workflowArtifacts_["project_change_applied"].toBool()){
                const auto method=performanceRequest_["inputs"].toObject()["calculator"].toString();
                if(!workflowArtifacts_["project_change_sent"].toBool()){
                    openCalculation("performance",method);
                    workflowArtifacts_["unchanged_project_reopen_inputs"]=performanceDialog_->inputs()["values"];
                    workflowArtifacts_["project_change_sent"]=true;
                    workflowCase("project-change-requested",session_.updateProject(performanceRequest_["project_change"].toObject()));return;
                }
                openCalculation("performance",method);workflowArtifacts_["project_change_applied"]=true;
            }
            if(performanceRequest_.contains("parameter_steps")&&!workflowArtifacts_["parameter_steps_done"].toBool()){
                const auto steps=performanceRequest_["parameter_steps"].toArray();
                const int index=workflowArtifacts_["parameter_step_index"].toInt();
                QJsonObject state;
                for(auto* w:performanceDialog_->findChildren<QWidget*>()){
                    const auto key=w->property("performance_field").toString();if(key.isEmpty())continue;
                    state[key]=QJsonObject{{"enabled",w->isEnabled()},{"declared_visible",w->property("calculation_declared_visible").toBool()}};
                }
                auto states=workflowArtifacts_["parameter_states"].toArray();
                QJsonArray labels;for(auto* label:performanceDialog_->findChildren<QLabel*>())if(!label->property("performance_label").toString().isEmpty())labels.append(QJsonObject{{"key",label->property("performance_label").toString()},{"html",label->text()}});
                QJsonObject observation{{"inputs",performanceDialog_->inputs()},{"fields",state},{"labels",labels}};
                if(performanceRequest_["observe_input_feedback"].toBool()){
                    QJsonObject markers,descriptions,pending;
                    for(auto* label:performanceDialog_->findChildren<QLabel*>()){
                        const auto key=label->property("calculation_input_status").toString();
                        if(!key.isEmpty()){
                            markers[key]=label->property("performance_field_valid").toBool();
                            descriptions[key]=label->toolTip();
                            pending[key]=label->property("performance_field_pending").toBool();
                        }
                    }
                    auto* run=button("calculate");
                    observation["validity"]=markers;
                    observation["validation_descriptions"]=descriptions;
                    observation["pending_validation"]=pending;
                    observation["can_calculate"]=run&&run->isEnabled();
                }
                states.append(observation);workflowArtifacts_["parameter_states"]=states;
                if(index==steps.size())workflowArtifacts_["parameter_steps_done"]=true;
                else{
                    const auto values=steps[index].toObject();QSet<QString> used;
                    for(auto* w:performanceDialog_->findChildren<QWidget*>()){
                        const auto key=w->property("performance_field").toString();if(!values.contains(key))continue;used.insert(key);
                        if(auto* n=qobject_cast<QDoubleSpinBox*>(w))n->setValue(values[key].toDouble());
                        else if(auto* n=qobject_cast<QComboBox*>(w))n->setCurrentIndex(values[key].toInt());
                        else if(auto* n=qobject_cast<QCheckBox*>(w))n->setChecked(values[key].toBool());
                    }
                    for(auto it=values.begin();it!=values.end();++it)if(!used.contains(it.key())){unavailable("Parameter step input unavailable: "+it.key());return;}
                    workflowArtifacts_["parameter_step_index"]=index+1;return;
                }
            }
            if(performanceRequest_["inspect_layout"].toBool()&&!workflowArtifacts_["layout_checked"].toBool()){
                *stage=9;workflowArtifacts_["layout_observations"]=observeCalculationLayout(performanceDialog_,workflowOutput_,performanceRequest_["layout_images"].toBool());
                workflowArtifacts_["layout_checked"]=true;*stage=6;return;
            }
            if(performanceRequest_["preview"].toBool()&&!workflowArtifacts_["preview_done"].toBool()){
                auto* preview=performanceDialog_->gridPreview();if(!preview){unavailable("Offset preview unavailable");return;}
                revealParameter(performanceDialog_,preview);
                preview->requestNow();*stage=8;return;
            }
            workflowArtifacts_["effective_inputs"]=performanceDialog_->inputs();
            if(performanceRequest_["observe_method_help"].toBool()){
                QJsonArray labels;
                QJsonArray notices;
                QJsonObject statusTooltips;
                for(auto* label:performanceDialog_->findChildren<QLabel*>()){
                    const auto key=label->property("performance_label").toString();
                    if(!key.isEmpty())labels.append(QJsonObject{{"key",key},{"html",label->text()},{"tooltip",label->toolTip()},
                        {"editor_tooltip",label->buddy()?label->buddy()->toolTip():QString{}}});
                    const auto statusKey=label->property("calculation_input_status").toString();
                    if(!statusKey.isEmpty()&&label->isVisible())statusTooltips[statusKey]=label->toolTip();
                    if(label->isVisible()&&!label->property("studio_message_role").toString().isEmpty()&&!label->text().isEmpty())
                        notices.append(label->text());
                }
                workflowArtifacts_["parameter_labels"]=labels;
                workflowArtifacts_["parameter_status_tooltips"]=statusTooltips;
                workflowArtifacts_["visible_input_notices"]=notices;
                for(auto* browser:performanceDialog_->findChildren<QTextBrowser*>())if(browser->objectName()=="PerformanceHelp"){
                    auto* pages=performanceDialog_->findChild<QTabWidget*>("PerformancePages");
                    const bool tabVisible=pages&&pages->isTabVisible(pages->indexOf(browser));
                    workflowArtifacts_["method_help_tab_visible"]=tabVisible;
                    workflowArtifacts_["method_help"]=browser->toPlainText();
                    QJsonArray tables;
                    for(auto* frame:browser->document()->rootFrame()->childFrames())
                        if(auto* table=qobject_cast<QTextTable*>(frame);table&&report::messageRole(table).isEmpty())
                            tables.append(QJsonObject{{"rows",table->rows()},{"columns",table->columns()}});
                    workflowArtifacts_["method_help_tables"]=tables;
                    workflowArtifacts_["method_help_math"]=reportSubscriptProbe(browser);
                    workflowArtifacts_["method_help_notation"]=notation::statistics();
                    if(tabVisible){
                        revealParameter(performanceDialog_,browser);QApplication::processEvents();
                        performanceDialog_->grab().save(workflowOutput_+"/views/method-help.png");
                    }
                }
            }
            if(performanceRequest_.contains("start_page"))for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())
                for(int i=0;i<tabs->count();++i)if(tabs->widget(i)->property("calculation_page").toString()==performanceRequest_["start_page"].toString()&&tabs->isTabVisible(i))tabs->setCurrentIndex(i);
            workflowCase("parameters-image",performanceDialog_->grab().save(workflowOutput_+"/views/parameters.png"));
            if(performanceRequest_["save_input_draft"].toBool()){
                const bool attached=performanceDialog_->actions().contains(save_)&&
                    save_->shortcuts().contains(QKeySequence("Ctrl+S"));
                workflowCase("draft-save-shortcut-attached",attached);
                *stage=10;
                if(attached)save_->trigger();
                return;
            }
            if(performanceRequest_["describe_only"].toBool()){timer->stop();timer->deleteLater();performanceDialog_->reject();return;}
            if(performanceRequest_["database_viewer"].toBool()){
                auto* viewer=button("database_viewer");if(!viewer){unavailable("Database viewer callback unavailable");return;}
                *stage=7;viewer->click();return;
            }
            auto* run=button("calculate");if(!run){unavailable("Calculate callback unavailable");return;}
            if(!run->isEnabled()){
                workflowCase("calculate-button-enabled",false,"The requested inputs leave Calculate disabled.");
                unavailable("Calculate is disabled");
                return;
            }
            if(performanceRequest_["measure_responsiveness"].toBool()){
                auto* pulse=new QTimer(this);pulse->setObjectName("PerformanceResponsePulse");pulse->setInterval(16);
                auto clock=std::make_shared<QElapsedTimer>();clock->start();auto last=std::make_shared<qint64>(0);
                connect(pulse,&QTimer::timeout,this,[this,clock,last]{
                    const auto now=clock->elapsed();const double gap=now-*last;*last=now;
                    auto m=workflowArtifacts_["responsiveness"].toObject();m["max_gap_ms"]=std::max(gap,m["max_gap_ms"].toDouble());m["ticks"]=m["ticks"].toInt()+1;m["elapsed_ms"]=double(now);workflowArtifacts_["responsiveness"]=m;
                });pulse->start();
                QElapsedTimer click;click.start();*stage=1;run->click();
                workflowArtifacts_["calculate_click_ms"]=click.nsecsElapsed()/1e6;
                const int cancel=performanceRequest_["cancel_after_ms"].toInt();
                if(cancel>0)QTimer::singleShot(cancel,this,[this]{
                    if(!performanceDialog_)return;for(auto* b:performanceDialog_->findChildren<QPushButton*>())if(b->property("performance_action")=="cancel"&&b->isEnabled()){b->click();workflowArtifacts_["timed_cancel_delivered"]=true;break;}
                });
            }else{*stage=1;run->click();}
            if(performanceRequest_["cancel"].toBool()){auto* stop=button("cancel");if(stop&&stop->isEnabled())stop->click();else workflowCase("cancel-callback",false);}return;
        }
        if(*stage==7){
            if(performanceRequest_["expect_error"].toBool()){
                if(performanceDialog_->property("performance_error").toString().isEmpty())return;
            }else if(!performanceDialog_->property("database_viewer_exit").isValid())return;
            else workflowCase("database-viewer-exit",performanceDialog_->property("database_viewer_exit").toInt()==0);
            *stage=1;
        }
        if(*stage==2||*stage==3){
            if(*stage==3){workflowCase("cancel-preserves-result",QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(performanceDialog_->result()).toJson(),QCryptographicHash::Sha256).toHex())==workflowArtifacts_["complete_result_sha256"].toString());workflowCase("cancel-reported",!performanceDialog_->property("performance_error").toString().isEmpty());}
            timer->stop();timer->deleteLater();performanceDialog_->reject();return;
        }
        if(!performanceDialog_->property("performance_finished").toBool())return;
        if(performanceRequest_["measure_responsiveness"].toBool()){
            if(!workflowArtifacts_["response_settle_started"].toBool()){
                workflowArtifacts_["response_settle_started"]=true;
                if(performanceRequest_["show_result_table"].toBool())for(auto* table:performanceDialog_->findChildren<QTableView*>())if(table->property("performance_table").toBool())revealParameter(performanceDialog_,table);
                QTimer::singleShot(250,this,[this]{workflowArtifacts_["response_settled"]=true;});return;
            }
            if(!workflowArtifacts_["response_settled"].toBool())return;
            for(auto* table:performanceDialog_->findChildren<QTableView*>())if(table->property("performance_table").toBool()){
                if(table->property("studio_table_measure_pending").toBool())return;
                workflowArtifacts_["table_measure_max_ms"]=table->property("studio_table_measure_max_ms").toDouble();
                workflowArtifacts_["table_measured_rows"]=table->property("studio_table_measured_rows").toInt();
            }
            if(auto* pulse=findChild<QTimer*>("PerformanceResponsePulse")){QMetaObject::invokeMethod(pulse,"timeout",Qt::DirectConnection);pulse->stop();pulse->deleteLater();}
            workflowArtifacts_["receive_timings"]=performanceDialog_->property("performance_receive_timings").toJsonObject();
            workflowArtifacts_["result_scalars"]=performanceDialog_->result()["scalars"];
            QJsonArray counts;for(auto c:performanceDialog_->result()["series"].toArray())counts.append(c.toObject()["points"].toArray().size());workflowArtifacts_["point_counts"]=counts;
            if(performanceRequest_["responsiveness_only"].toBool()){
                workflowArtifacts_["error"]=performanceDialog_->property("performance_error").toString();
                timer->stop();timer->deleteLater();performanceDialog_->reject();return;
            }
        }
        const auto error=performanceDialog_->property("performance_error").toString();workflowArtifacts_["error"]=error;
        if(performanceRequest_["cancel"].toBool()||performanceRequest_["expect_error"].toBool())workflowCase("failure-reported",!error.isEmpty());
        else{
            workflowCase("calculation-delivered",!performanceDialog_->result().isEmpty()&&error.isEmpty(),error);
            if(performanceDialog_->result()["development_sample"]==true)
                workflowCase("sample-calculation-does-not-write-model-parameters",
                    performanceDialog_->result()["parameters_recorded"]!=true&&
                    workflowArtifacts_["before_revision"].toDouble()==double(session_.model()->revision)&&
                    !calculationDrafts().contains(performanceDialog_->inputs()["calculator"].toString()));
            else
                workflowCase("successful-calculation-records-complete-inputs",performanceDialog_->result()["parameters_recorded"]==true&&performanceDialog_->result()["recorded_revision"].toDouble(performanceDialog_->result()["revision"].toDouble())==double(session_.model()->revision));
            workflowArtifacts_["parameters_recorded"]=performanceDialog_->result()["parameters_recorded"];
            workflowArtifacts_["dirty_after_calculation"]=session_.modified();
            bool choicesCurrent=true;
            for(auto v:performanceDialog_->result()["parameters"].toObject()["fields"].toArray()){
                auto f=v.toObject();if(f["kind"]!="choice")continue;
                for(auto* combo:performanceDialog_->findChildren<QComboBox*>())if(combo->property("performance_field")==f["key"].toString()){
                    const auto index=f["value"].toInt();const auto choices=f["choices"].toArray();
                    choicesCurrent &= combo->currentIndex()==index&&(index<0||combo->currentText()==uiText(choices[index].toString()));
                }
            }
            workflowCase("choice-labels-match-result",choicesCurrent);
            if(!performanceDialog_->result()["help"].toString().isEmpty()){
                bool helpFound=false;
                for(auto* text:performanceDialog_->findChildren<QTextEdit*>())if(text->property("calculation_page")=="help"){
                    helpFound=true;workflowArtifacts_["help_copy"]=QJsonObject{{"legacy_source",performanceDialog_->result()["help"].toString()},{"actual",text->toPlainText()}};
                }
                workflowCase("separate-help-available",helpFound);
            }
            if(performanceRequest_.contains("start_page"))for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())workflowCase("calculation-retains-page",tabs->currentWidget()->property("calculation_page").toString()==performanceRequest_["start_page"].toString());
            QComboBox *dataSeries=nullptr;
            for(auto* combo:performanceDialog_->findChildren<QComboBox*>()){
                if(combo->property("performance_series").toBool())dataSeries=combo;
            }
            if(dataSeries&&dataSeries->count()){
                revealParameter(performanceDialog_,dataSeries);dataSeries->setCurrentIndex(dataSeries->count()-1);
                workflowCase("table-data-selector-reachable",dataSeries->isVisible()==(dataSeries->count()>1));
                dataSeries->setCurrentIndex(0);
            }
            for(auto* combo:performanceDialog_->findChildren<QComboBox*>())if(combo->property("performance_series").toBool()){
                auto tables=performanceDialog_->result()["tables"].toArray();
                for(int i=0;i<tables.size();++i){combo->setCurrentIndex(i);auto t=tables[i].toObject();auto rows=t["values"].toArray();
                    for(auto* view:performanceDialog_->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                        bool matches=view->model()->rowCount()==rows.size()&&view->model()->columnCount()==t["columns"].toArray().size();
                        for(int row=0;row<rows.size();++row){auto values=rows[row].toArray();for(int col=0;col<values.size();++col)matches &= view->model()->data(view->model()->index(row,col),Qt::ToolTipRole).toString()==(values[col].isNull()?QStringLiteral("—"):QString::number(values[col].toDouble(),'g',17));}
                        workflowCase("matrix-table-values-"+QString::number(i),matches);
                        if(performanceRequest_["observe_added_mass_table"].toBool()&&t["name"]=="等效椭球"){
                            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                            const auto* heading=view->verticalHeader();
                            const auto font=view->font();
                            bool fits=true;
                            for(int row=0;row<view->model()->rowCount();++row){
                                const auto label=view->model()->headerData(row,Qt::Vertical).toString();
                                fits &= heading->width()>=notation::size(label,heading->font()).width()+12;
                            }
                            const auto label=view->model()->headerData(3,Qt::Vertical).toString();
                            workflowCase("ellipsoid-row-header-math-and-fit",fits&&notation::plain(label)=="al_{11}, al_{22}, al_{33}");
                            workflowCase("ellipsoid-readable-font",font.pointSizeF()>=13&&heading->font()==font&&view->horizontalHeader()->font()==font);
                            workflowCase("ellipsoid-table-image",performanceDialog_->grab().save(workflowOutput_+"/views/added-mass-ellipsoid.png"));
                            const auto width=heading->width();
                            const QPointF position(20,20);
                            QWheelEvent zoom(position,view->viewport()->mapToGlobal(position),{},QPoint(0,120),
                                             Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
                            QApplication::sendEvent(view->viewport(),&zoom);
                            workflowCase("ellipsoid-zoom-scales-row-header",view->font().pointSizeF()>font.pointSizeF()&&heading->width()>width);
                        }
                    }
                }if(!tables.isEmpty())combo->setCurrentIndex(0);
                auto curves=performanceDialog_->result()["series"].toArray();
                const auto method=performanceDialog_->inputs()["calculator"].toString();
                if(method=="propeller4"||method=="propeller5"){
                    for(qsizetype i=curves.size();i-->0;)if(curves[i].toObject()["source"]=="Series4")curves.removeAt(i);
                }
                if(tables.isEmpty()&&!curves.isEmpty())for(auto* view:performanceDialog_->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                    QSet<QString> axes;
                    for(const auto& series:curves)axes.insert(series.toObject()["x_title"].toString().simplified());
                    QList<QJsonValue> all;
                    for(const auto& series:curves)all.append(series);
                    auto remaining=all;
                    QJsonArray observed;
                    bool matches=axes.contains(QString{})||axes.size()==combo->count();
                    const auto* model=view->model();
                    for(int index=0;index<combo->count();++index){
                        combo->setCurrentIndex(index);
                        const auto group=combo->currentData().toJsonArray();
                        QStringList headings;
                        for(int column=0;column<model->columnCount();++column)headings<<model->headerData(column,Qt::Horizontal).toString();
                        const auto axis=group.first().toObject()["x_title"].toString().simplified();
                        matches &= model->columnCount()==group.size()+1;
                        const auto number=[](const QJsonValue& value){return QString::number(value.toDouble(),'g',17);};
                        for(int column=0;column<group.size();++column){
                            const auto curve=group[column].toObject();
                            const auto position=remaining.indexOf(group[column]);
                            matches &= position>=0&&curve["x_title"].toString().simplified()==axis;
                            if(position>=0)remaining.removeAt(position);
                            QMap<QString,int> expected,actual;
                            for(const auto& sample:curve["points"].toArray()){
                                const auto point=sample.toArray();++expected[number(point[0])+"|"+number(point[1])];
                            }
                            for(int row=0;row<model->rowCount();++row){
                                const auto cell=model->data(model->index(row,column+1),Qt::ToolTipRole).toString();
                                if(cell!=QStringLiteral("—"))++actual[model->data(model->index(row,0),Qt::ToolTipRole).toString()+"|"+cell];
                            }
                            workflowCase("table-full-series-"+QString::number(all.indexOf(group[column])),actual==expected);
                            matches &= actual==expected;
                        }
                        observed.append(QJsonObject{{"headers",QJsonArray::fromStringList(headings)},{"rows",model->rowCount()}});
                    }
                    workflowCase("table-groups-preserve-axis-and-all-values",matches&&remaining.isEmpty());
                    if(performanceDialog_->inputs()["calculator"]=="wind")
                        workflowCase("wind-table-shows-all-series-by-angle",matches&&combo->count()==1&&curves.size()==3);
                    workflowArtifacts_["curve_tables"]=observed;
                    combo->setCurrentIndex(0);
                }
            }
            if(performanceRequest_["verify_charts"].toBool()){
                *stage=9;const auto originalResult=performanceDialog_->result();QJsonArray charts;int chartIndex=0;
                for(auto* widget:performanceDialog_->findChildren<QWidget*>())if(widget->property("performance_chart").toBool()){
                    auto* chart=static_cast<CalculationPlot*>(widget);if(chart->evidence()["curves"].toArray().isEmpty())continue;
                    revealParameter(performanceDialog_,chart);QApplication::processEvents();chart->grab();auto before=chart->evidence();chart->grab();
                    workflowCase("chart-cache-"+QString::number(chartIndex),chart->evidence()["cache_builds"]==before["cache_builds"]);
                    QToolButton* toggle=nullptr;for(auto* b:chart->findChildren<QToolButton*>())if(b->property("chart_curve")==0)toggle=b;
                    if(toggle){
                        toggle->click();QApplication::processEvents();chart->grab();
                        const auto hidden=chart->evidence();
                        workflowCase("chart-legend-"+QString::number(chartIndex),!hidden["curves"].toArray()[0].toObject()["visible"].toBool());
                        if(performanceRequest_["verify_acceleration_layouts"].toBool()){
                            auto axesOnly=[](const QJsonObject& evidence){
                                auto panels=evidence["panels"].toArray();
                                for(qsizetype i=0;i<panels.size();++i){auto panel=panels[i].toObject();panel.remove("curves");panels[i]=panel;}
                                return panels;
                            };
                            workflowCase("acceleration-fixed-axes-"+QString::number(chartIndex),
                                axesOnly(before).size()==3&&axesOnly(before)==axesOnly(hidden));
                        }
                        toggle->click();
                    }
                    if(performanceRequest_["verify_acceleration_layouts"].toBool()){
                        auto* canvas=chart->findChild<QWidget*>("CalculationCanvas");
                        const auto suffix=QString::number(chartIndex);
                        const auto panels=before["panels"].toArray();
                        workflowCase("acceleration-only-legend-controls-"+suffix,
                            chart->findChildren<QToolButton*>().size()==5&&panels.size()==3);
                        if(canvas&&panels.size()==3){
                            const auto hover=[&](int index){
                                const auto rect=panels.at(index).toObject()["plot"].toArray();
                                if(rect.size()!=4)return QJsonObject{};
                                const QPointF position(rect[0].toDouble()+rect[2].toDouble()/2,rect[1].toDouble()+rect[3].toDouble()/2);
                                QMouseEvent move(QEvent::MouseMove,position,canvas->mapToGlobal(position.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                                QApplication::sendEvent(canvas,&move);
                                return chart->evidence()["readout"].toObject();
                            };
                            chart->grab().save(workflowOutput_+"/views/acceleration-grouped-"+suffix+".png");
                            const QList<QJsonArray> expected{{0,1},{2},{3,4}};
                            QJsonArray readouts;
                            const auto builds=chart->evidence()["cache_builds"].toDouble();
                            for(int panel=0;panel<panels.size();++panel){
                                const auto readout=hover(panel);
                                readouts.append(readout);
                                workflowCase("acceleration-hover-panel-"+suffix+"-"+QString::number(panel),
                                    readout["curves"].toArray()==expected[panel]&&readout["values"].toArray().size()==expected[panel].size());
                                chart->grab().save(workflowOutput_+"/views/acceleration-readout-"+suffix+"-"+QString::number(panel)+".png");
                            }
                            auto observations=workflowArtifacts_["acceleration_chart_readout"].toArray();
                            observations.append(readouts);workflowArtifacts_["acceleration_chart_readout"]=observations;
                            workflowCase("acceleration-readout-reuses-chart-render-"+suffix,chart->evidence()["cache_builds"].toDouble()==builds);
                            if(toggle){
                                toggle->click();QApplication::processEvents();chart->grab();
                                workflowCase("acceleration-hidden-curve-omitted-from-readout-"+suffix,hover(0)["curves"].toArray()==QJsonArray{1});
                                toggle->click();
                            }
                            for(auto* button:chart->findChildren<QToolButton*>())if(button->property("chart_curve")==2){
                                button->click();QApplication::processEvents();chart->grab();
                                workflowCase("acceleration-empty-panel-has-no-readout-"+suffix,hover(1).isEmpty());
                                button->click();
                                break;
                            }
                        }
                    }
                    workflowCase("chart-legend-only-controls-"+QString::number(chartIndex),chart->findChildren<QCheckBox*>().isEmpty());
                    QApplication::processEvents();chart->grab();charts.append(chart->evidence());
                    workflowCase("chart-picture-"+QString::number(chartIndex),performanceDialog_->grab().save(workflowOutput_+"/views/chart-"+QString::number(chartIndex)+".png"));++chartIndex;
                }
                workflowArtifacts_["charts"]=charts;workflowCase("chart-does-not-change-result",performanceDialog_->result()==originalResult);*stage=1;
                if(performanceRequest_["charts_only"].toBool()){timer->stop();timer->deleteLater();performanceDialog_->reject();return;}
            }
            for(const auto& format:{QString("json"),QString("csv"),QString("txt"),QString("pdf")}){const auto path=workflowOutput_+"/performance-report."+format;auto err=performanceDialog_->exportReport(path,format);workflowCase("export-"+format,err.isEmpty(),err);workflowArtifacts_["report_"+format]=path;}
            if(performanceRequest_["observe_report"].toBool()){
                auto* browser=performanceDialog_->findChild<QTextBrowser*>("PerformanceReport");QJsonArray tables;
                if(browser)for(auto* frame:browser->document()->rootFrame()->childFrames())
                    if(auto* table=qobject_cast<QTextTable*>(frame);table&&report::messageRole(table).isEmpty())tables.append(QJsonObject{{"rows",table->rows()},{"columns",table->columns()}});
                workflowArtifacts_["report_tables"]=tables;
                workflowArtifacts_["report_messages"]=reportMessageProbe(browser);workflowArtifacts_["report_subscripts"]=reportSubscriptProbe(browser);
                if(browser&&performanceRequest_["observe_report_navigation"].toBool()){
                    revealParameter(performanceDialog_,browser);
                    workflowArtifacts_["report_navigation"]=probeReportNavigation(*browser);
                }
                for(auto* text:performanceDialog_->findChildren<QTextEdit*>())if(text->property("calculation_page")=="help"){
                    workflowArtifacts_["help_messages"]=reportMessageProbe(text);QJsonArray headings;
                    for(auto block=text->document()->begin();block.isValid();block=block.next())if(block.blockFormat().headingLevel())headings.append(block.text());
                    workflowArtifacts_["help_headings"]=headings;workflowArtifacts_["help_text"]=text->toPlainText();
                }
            }
            *stage=9;
            for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)if(tabs->isTabVisible(i)){
                revealParameter(performanceDialog_,tabs->widget(i));const auto name=tabs->property("calculation_pages").toBool()?"page-"+tabs->widget(i)->property("calculation_page").toString():"results-"+QString::number(i);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                if(name=="results-0"&&performanceRequest_["verify_presentation"].toBool()){
                    for(auto* view:tabs->widget(i)->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                        const auto* model=view->model();
                        const QFontMetricsF metrics(view->font());
                        bool fits=view->font().pointSizeF()>=13;
                        for(int row=0;row<model->rowCount();++row)
                            for(int col=0;col<model->columnCount();++col)
                                fits &= view->columnWidth(col)>=metrics.horizontalAdvance(model->data(model->index(row,col)).toString())+8;
                        workflowCase("result-table-readable-without-truncated-numbers",fits);
                    }
                }
                workflowCase("result-image-"+name,performanceDialog_->grab().save(workflowOutput_+"/views/"+name+".png"));}
            *stage=1;
            if(performanceRequest_["observe_input_feedback"].toBool()||performanceRequest_["observe_report"].toBool()){
                if(auto* label=performanceDialog_->findChild<QLabel*>("PerformanceStatus")){
                    workflowArtifacts_["calculation_feedback"]=label->text();
                    workflowArtifacts_["calculation_feedback_role"]=label->property("studio_message_role").toString();
                    workflowArtifacts_["calculation_feedback_visible"]=label->isVisible();
                }
            }
            const auto protectedPath=workflowOutput_+"/protected-report.txt";QFile protectedFile(protectedPath);protectedFile.open(QIODevice::WriteOnly);protectedFile.write("existing report");protectedFile.close();
            const auto badExport=performanceDialog_->exportReport(protectedPath,"unsupported");protectedFile.open(QIODevice::ReadOnly);workflowCase("failed-export-preserves-file",!badExport.isEmpty()&&protectedFile.readAll()=="existing report");
        }
        if(performanceRequest_["preset_roundtrip"].toBool()){
            const auto path=workflowOutput_+"/parameters.json";workflowArtifacts_["preset_values"]=performanceDialog_->inputs()["values"];
            workflowCase("save-parameter-file",performanceDialog_->saveParameters(path).isEmpty());
            if(performanceRequest_["inputs"].toObject()["calculator"]=="acceleration"){
                QFile saved(path);saved.open(QIODevice::ReadOnly);auto parameters=QJsonDocument::fromJson(saved.readAll()).object();saved.close();
                workflowCase("inertia-preset-unit",parameters["engine_inertia_convention"].toObject()["unit"]=="kg*m^2"&&parameters["engine_inertia_convention"].toObject()["basis"]=="per_propeller_shaft");
                auto invalid=parameters;invalid["engine_inertia_convention"]=QJsonObject{{"unit","t*m^2"},{"basis","per_propeller_shaft"}};
                const auto invalidPath=workflowOutput_+"/parameters-wrong-unit.json";QFile bad(invalidPath);bad.open(QIODevice::WriteOnly);bad.write(QJsonDocument(invalid).toJson());bad.close();
                const QJsonValue before=performanceDialog_->inputs()["values"];
                workflowCase("inertia-preset-reject-wrong-unit",!performanceDialog_->loadParameters(invalidPath).isEmpty()&&performanceDialog_->inputs()["values"]==before);
                // Load an old preset without a unit tag through the same real callback.
                parameters.remove("engine_inertia_convention");saved.open(QIODevice::WriteOnly|QIODevice::Truncate);saved.write(QJsonDocument(parameters).toJson());saved.close();
            }
            QSet<QString> changedPages;
            for(auto* n:performanceDialog_->findChildren<QDoubleSpinBox*>()){
                const auto page=n->property("calculation_parameter_page").toString();
                if(!n->property("performance_field").toString().isEmpty()&&!changedPages.contains(page)&&n->value()+.1<=n->maximum()){
                    n->setValue(n->value()+.1);changedPages.insert(page);revealParameter(performanceDialog_,n);
                }
            }
            for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())workflowArtifacts_["preset_page"]=tabs->currentWidget()->property("calculation_page").toString();
            *stage=4;const auto err=performanceDialog_->loadParameters(path);workflowCase("load-parameter-file",err.isEmpty(),err);return;
        }
        if(performanceRequest_["repeat_cancel"].toBool()){auto* run=button("calculate");auto* stop=button("cancel");if(!run||!stop){unavailable("Repeat callbacks unavailable");return;}
            workflowArtifacts_["complete_result_sha256"]=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(performanceDialog_->result()).toJson(),QCryptographicHash::Sha256).toHex());*stage=3;run->click();stop->click();return;}
        // Retained request flag observes the new automatic behavior; it must
        // not resurrect the removed manual write-to-model control.
        if(performanceRequest_["save_settings"].toBool())workflowCase("manual-parameter-save-button-removed",button("save_settings")==nullptr);
        if(performanceRequest_["verify_grouping_fixture"].toBool()){
            auto sample=performanceDialog_->result();
            const auto point=[](double x,double y){return QJsonArray{x,y};};
            sample.remove("tables");
            sample["series"]=QJsonArray{
                QJsonObject{{"name","Rf"},{"x_title","航速 (kn)"},{"y_title","阻力 (kN)"},{"points",QJsonArray{point(1,5),point(1,6),point(3,7)}}},
                QJsonObject{{"name","Pe"},{"x_title","航速 (kn)"},{"y_title","功率 (kW)"},{"points",QJsonArray{point(1,8),point(2,9)}}},
                QJsonObject{{"name","Rt"},{"x_title","航速 (m/s)"},{"points",QJsonArray{point(1,10)}}},
                QJsonObject{{"name","扭矩"},{"x_title","时间 (s)"},{"points",QJsonArray{point(2,11)}}}
            };
            *stage=9;
            performanceDialog_->receive(sample,false);
            for(auto* combo:performanceDialog_->findChildren<QComboBox*>())if(combo->property("performance_series").toBool()){
                revealParameter(performanceDialog_,combo);
                workflowCase("different-axes-and-units-have-separate-tables",combo->isVisible()&&combo->count()==3);
                for(auto* view:performanceDialog_->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                    const auto* model=view->model();
                    const auto value=[&](int row,int col){return model->data(model->index(row,col)).toString();};
                    workflowCase("aligned-samples-preserve-duplicates-and-leave-gaps",
                        model->columnCount()==3&&model->rowCount()==4&&value(0,1)=="5"&&value(1,1)=="6"&&
                        value(1,2)==QStringLiteral("—")&&value(2,1)==QStringLiteral("—")&&value(2,2)=="9"&&value(3,1)=="7");
                    combo->setCurrentIndex(1);
                    workflowCase("switching-table-keeps-its-own-axis",model->columnCount()==2&&model->rowCount()==1&&value(0,1)=="10");
                }
            }
        }
        timer->stop();timer->deleteLater();performanceDialog_->reject();
    });timer->start();candidates.first()->trigger();
}
