// Hidden production callbacks and evidence; never desktop automation.
#include "main_window.h"
#include "performance_dialog.h"
#include "grid_preview_panel.h"
#include "calculation_plot.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCryptographicHash>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QLayout>
#include <QInputDialog>
#include <QApplication>
#include <QTableView>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>
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
        QJsonArray fields,pages,actions;
        for(auto* w:dialog->findChildren<QWidget*>()){
            const auto key=w->property("performance_field").toString();if(key.isEmpty())continue;
            const bool declared=w->property("calculation_declared_visible").toBool();
            QJsonObject item{{"key",key},{"page",w->property("calculation_parameter_page").toString()},{"declared_visible",declared}};
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
                for(auto* label:dialog->findChildren<QLabel*>())if(label->buddy()==w){item["label_fits"]=label->height()>=label->heightForWidth(label->width());break;}
            }
            fields.append(item);
        }
        for(auto* tabs:dialog->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool()){
            for(int i=0;i<tabs->count();++i){const auto page=tabs->widget(i)->property("calculation_page").toString();pages.append(QJsonObject{{"page",page},{"visible",tabs->isTabVisible(i)}});
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
            const QRect rect(button->mapTo(dialog,QPoint{}),button->size());actions.append(QJsonObject{{"action",button->property("performance_action").toString()},{"text",button->text()},{"contained",dialog->rect().contains(rect)}});
        }
        observations.append(QJsonObject{{"font",pointSize},{"compact",compact},{"requested_size",QJsonArray{requested.width(),requested.height()}},{"actual_size",QJsonArray{dialog->width(),dialog->height()}},{"fields",fields},{"pages",pages},{"actions",actions},{"inputs_unchanged",dialog->inputs()==originalInputs}});
    }
    dialog->setFont(originalFont);dialog->resize(originalSize);settle();return observations;
}
}
void MainWindow::verifyPerformance(const QString& request,const QString& output){
    workflow_=true;performanceWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();
    QFile f(request);if(!f.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read performance workflow");return;}
    performanceRequest_=QJsonDocument::fromJson(f.readAll()).object();
    if(!QDir().mkpath(workflowOutput_+"/views")){finishWorkflow("Cannot create performance evidence");return;}
    ensurePolished();if(centralWidget()->layout())centralWidget()->layout()->activate();openModel(performanceRequest_["model"].toString());
}
void MainWindow::performanceStep(const QString& operation){
    if(operation=="closed"){captureWorkflow("after");workflowCase("performance.document-revision",session_.model()->revision==quint64(workflowArtifacts_["before_revision"].toDouble())+(performanceRequest_["save_settings"].toBool()?1:0));finishWorkflow();return;}
    if(operation=="performance"&&workflowArtifacts_["preparing_performance"].toBool()){
        workflowArtifacts_["preparing_performance"]=false;workflowArtifacts_["prepared_performance"]=true;
        QTimer::singleShot(0,this,[this]{performanceStep("load");});return;
    }
    if(operation!="load")return;
    if(performanceRequest_["prepare"].isObject()&&!workflowArtifacts_["prepared_performance"].toBool()){
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
        if(!performanceDialog_||!performanceDialog_->isVisible()||session_.busy())return;
        if(*stage==9)return; // Layout observation may process queued Qt events.
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
            const auto inputs=performanceRequest_["inputs"].toObject(),values=inputs["values"].toObject();QSet<QString> used;QJsonArray fields;
            for(auto* w:performanceDialog_->findChildren<QWidget*>()){
                if(w->property("performance_directory").toBool()&&inputs.contains("solver_directory"))qobject_cast<QLineEdit*>(w)->setText(inputs["solver_directory"].toString());
                const auto key=w->property("performance_field").toString();if(key.isEmpty())continue;fields.append(QJsonObject{{"key",key},{"visible",w->isVisible()},{"enabled",w->isEnabled()}});if(!values.contains(key))continue;used.insert(key);
                if(auto* n=qobject_cast<QDoubleSpinBox*>(w))n->setValue(values[key].toDouble());else if(auto* n=qobject_cast<QCheckBox*>(w))n->setChecked(values[key].toBool());else if(auto* n=qobject_cast<QComboBox*>(w))n->setCurrentIndex(values[key].toInt());
            }
            workflowArtifacts_["discovered_fields"]=fields;
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
            if(performanceRequest_["extract_before"].toBool()&&!workflowArtifacts_["extract_requested"].toBool()){
                auto* extract=button("extract");if(!extract){unavailable("Extract callback unavailable");return;}
                workflowArtifacts_["before_extract"]=performanceDialog_->inputs();workflowArtifacts_["extract_requested"]=true;extract->click();return;
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
            if(performanceRequest_.contains("start_page"))for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())
                for(int i=0;i<tabs->count();++i)if(tabs->widget(i)->property("calculation_page").toString()==performanceRequest_["start_page"].toString()&&tabs->isTabVisible(i))tabs->setCurrentIndex(i);
            workflowCase("parameters-image",performanceDialog_->grab().save(workflowOutput_+"/views/parameters.png"));
            if(performanceRequest_["describe_only"].toBool()){timer->stop();timer->deleteLater();performanceDialog_->reject();return;}
            if(performanceRequest_["database_viewer"].toBool()){
                auto* viewer=button("database_viewer");if(!viewer){unavailable("Database viewer callback unavailable");return;}
                *stage=7;viewer->click();return;
            }
            auto* run=button("calculate");if(!run){unavailable("Calculate callback unavailable");return;}*stage=1;run->click();
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
        const auto error=performanceDialog_->property("performance_error").toString();workflowArtifacts_["error"]=error;
        if(performanceRequest_["cancel"].toBool()||performanceRequest_["expect_error"].toBool())workflowCase("failure-reported",!error.isEmpty());
        else{
            workflowCase("calculation-delivered",!performanceDialog_->result().isEmpty()&&error.isEmpty(),error);
            bool choicesCurrent=true;
            for(auto v:performanceDialog_->result()["parameters"].toObject()["fields"].toArray()){
                auto f=v.toObject();if(f["kind"]!="choice")continue;
                for(auto* combo:performanceDialog_->findChildren<QComboBox*>())if(combo->property("performance_field")==f["key"].toString()){
                    const auto index=f["value"].toInt();const auto choices=f["choices"].toArray();
                    choicesCurrent &= combo->currentIndex()==index&&(index<0||combo->currentText()==choices[index].toString());
                }
            }
            workflowCase("choice-labels-match-result",choicesCurrent);
            if(!performanceDialog_->result()["help"].toString().isEmpty()){
                bool helpFound=false;
                for(auto* text:performanceDialog_->findChildren<QPlainTextEdit*>())if(text->property("calculation_page")=="help"){
                    helpFound=true;workflowCase("separate-help-complete",text->toPlainText().remove('\r')==performanceDialog_->result()["help"].toString().remove('\r'));
                }
                workflowCase("separate-help-available",helpFound);
            }
            if(performanceRequest_.contains("start_page"))for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())if(tabs->property("calculation_pages").toBool())workflowCase("calculation-retains-page",tabs->currentWidget()->property("calculation_page").toString()==performanceRequest_["start_page"].toString());
            QComboBox *generalSeries=nullptr,*resultSeries=nullptr;
            for(auto* combo:performanceDialog_->findChildren<QComboBox*>()){
                if(combo->property("performance_series").toBool())generalSeries=combo;
                if(combo->property("performance_result_series").toBool())resultSeries=combo;
            }
            if(generalSeries&&resultSeries&&resultSeries->count()){
                revealParameter(performanceDialog_,resultSeries);const auto i=resultSeries->count()-1;resultSeries->setCurrentIndex(i);
                workflowCase("results-selector-reachable",resultSeries->isVisible());
                workflowCase("results-selector-synchronized",generalSeries->currentIndex()==i&&generalSeries->currentText()==resultSeries->currentText());
                generalSeries->setCurrentIndex(0);workflowCase("general-selector-synchronized",resultSeries->currentIndex()==0);
            }
            for(auto* combo:performanceDialog_->findChildren<QComboBox*>())if(combo->property("performance_series").toBool()){
                auto tables=performanceDialog_->result()["tables"].toArray();
                for(int i=0;i<tables.size();++i){combo->setCurrentIndex(i);auto t=tables[i].toObject();auto rows=t["values"].toArray();
                    for(auto* view:performanceDialog_->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                        bool matches=view->model()->rowCount()==rows.size()&&view->model()->columnCount()==t["columns"].toArray().size();
                        for(int row=0;row<rows.size();++row){auto values=rows[row].toArray();for(int col=0;col<values.size();++col)matches &= view->model()->data(view->model()->index(row,col),Qt::ToolTipRole).toString()==(values[col].isNull()?QString("未定义"):QString::number(values[col].toDouble(),'g',17));}
                        workflowCase("matrix-table-values-"+QString::number(i),matches);
                    }
                }if(!tables.isEmpty())combo->setCurrentIndex(0);
                auto curves=performanceDialog_->result()["series"].toArray();
                for(int i=0;i<curves.size();++i){combo->setCurrentIndex(i);const auto points=curves[i].toObject()["points"].toArray();
                    for(auto* view:performanceDialog_->findChildren<QTableView*>())if(view->property("performance_table").toBool()){
                        bool matches=view->model()->rowCount()==points.size();
                        if(!points.isEmpty())for(int row:{0,int(points.size()/2),int(points.size()-1)})for(int col=0;col<2;++col)matches &= view->model()->data(view->model()->index(row,col),Qt::ToolTipRole).toString()==QString::number(points[row].toArray()[col].toDouble(),'g',17);
                        workflowCase("table-full-series-"+QString::number(i),matches);
                    }
                }if(!curves.isEmpty())combo->setCurrentIndex(0);
            }
            if(performanceRequest_["verify_charts"].toBool()){
                *stage=9;const auto originalResult=performanceDialog_->result();QJsonArray charts;int chartIndex=0;
                for(auto* widget:performanceDialog_->findChildren<QWidget*>())if(widget->property("performance_chart").toBool()){
                    auto* chart=static_cast<CalculationPlot*>(widget);if(chart->evidence()["curves"].toArray().isEmpty())continue;
                    revealParameter(performanceDialog_,chart);QApplication::processEvents();chart->grab();auto before=chart->evidence();chart->grab();
                    workflowCase("chart-cache-"+QString::number(chartIndex),chart->evidence()["cache_builds"]==before["cache_builds"]);
                    QToolButton* toggle=nullptr;for(auto* b:chart->findChildren<QToolButton*>())if(b->property("chart_curve")==0)toggle=b;
                    if(toggle){toggle->click();QApplication::processEvents();chart->grab();workflowCase("chart-legend-"+QString::number(chartIndex),!chart->evidence()["curves"].toArray()[0].toObject()["visible"].toBool());toggle->click();}
                    auto* only=chart->findChild<QCheckBox*>();if(only){only->click();QApplication::processEvents();chart->grab();const auto observed=chart->evidence();const auto index=observed["selected"].toInt();workflowCase("chart-single-"+QString::number(chartIndex),observed["points"]==observed["curves"].toArray()[index].toObject()["points"]);only->click();}
                    QApplication::processEvents();chart->grab();charts.append(chart->evidence());
                    workflowCase("chart-picture-"+QString::number(chartIndex),performanceDialog_->grab().save(workflowOutput_+"/views/chart-"+QString::number(chartIndex)+".png"));++chartIndex;
                }
                workflowArtifacts_["charts"]=charts;workflowCase("chart-does-not-change-result",performanceDialog_->result()==originalResult);*stage=1;
                if(performanceRequest_["charts_only"].toBool()){timer->stop();timer->deleteLater();performanceDialog_->reject();return;}
            }
            for(const auto& format:{QString("json"),QString("csv"),QString("txt"),QString("pdf")}){const auto path=workflowOutput_+"/performance-report."+format;auto err=performanceDialog_->exportReport(path,format);workflowCase("export-"+format,err.isEmpty(),err);workflowArtifacts_["report_"+format]=path;}
            for(auto* tabs:performanceDialog_->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)if(tabs->isTabVisible(i)){
                revealParameter(performanceDialog_,tabs->widget(i));const auto name=tabs->property("calculation_pages").toBool()?"page-"+tabs->widget(i)->property("calculation_page").toString():"results-"+QString::number(i);
                workflowCase("result-image-"+name,performanceDialog_->grab().save(workflowOutput_+"/views/"+name+".png"));}
            const auto protectedPath=workflowOutput_+"/protected-report.txt";QFile protectedFile(protectedPath);protectedFile.open(QIODevice::WriteOnly);protectedFile.write("existing report");protectedFile.close();
            const auto badExport=performanceDialog_->exportReport(protectedPath,"unsupported");protectedFile.open(QIODevice::ReadOnly);workflowCase("failed-export-preserves-file",!badExport.isEmpty()&&protectedFile.readAll()=="existing report");
        }
        if(performanceRequest_["preset_roundtrip"].toBool()){
            const auto path=workflowOutput_+"/parameters.json";workflowArtifacts_["preset_values"]=performanceDialog_->inputs()["values"];
            workflowCase("save-parameter-file",performanceDialog_->saveParameters(path).isEmpty());
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
        if(performanceRequest_["save_settings"].toBool()){auto* save=button("save_settings");if(!save){unavailable("Save callback unavailable");return;}*stage=2;save->click();return;}
        timer->stop();timer->deleteLater();performanceDialog_->reject();
    });timer->start();candidates.first()->trigger();
}
