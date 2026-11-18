// Persistent hidden production workflow; native desktop acceptance is separate.
#include "main_window.h"
#include "plates_dialog.h"
#include "layer_panel.h"
#include "ui_language.h"
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
#include <QLabel>
#include <QTableWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QPrintPreviewDialog>
#include <QPrintPreviewWidget>
#include <QToolButton>
#include <QApplication>
#include <QEventLoop>
#include <algorithm>
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
    if(platesRequest_.contains("render_fixture")){
        QFile fixture(platesRequest_["render_fixture"].toString());if(!fixture.open(QIODevice::ReadOnly)){finishWorkflow("Cannot read private plate fixture");return;}
        PlatesDialog dialog(session_,true,this);dialog.resize(1500,1000);dialog.show();dialog.receive(QJsonDocument::fromJson(fixture.readAll()).object());
        if(!dialog.hasResult()){finishWorkflow(dialog.failure());return;}
        workflowArtifacts_["drag_measurement"]=dialog.measureDrag();
        for(auto* canvas:dialog.findChildren<QGraphicsView*>()){
            canvas->scale(8,8);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const auto image=workflowOutput_+"/views/plate-canvas-zoomed.png";
            workflowCase("drag.zoomed-canvas-picture",dialog.grab().save(image));workflowArtifacts_["canvas_zoom_image"]=image;
        }
        QDoubleSpinBox* printScale=nullptr;for(auto* field:dialog.findChildren<QDoubleSpinBox*>())if(field->property("plate_field")=="print_scale")printScale=field;
        QPushButton* printButton=nullptr;for(auto* control:dialog.findChildren<QPushButton*>())if(control->property("plates_action")=="print")printButton=control;
        if(printScale&&printButton){
            const double savedScale=printScale->value();printScale->setValue(200);
            QTimer::singleShot(0,&dialog,[&]{
                auto* preview=qobject_cast<QPrintPreviewDialog*>(QApplication::activeModalWidget());
                if(!preview){workflowCase("drag.plate-print-preview-found",false);return;}
                auto* widget=preview->findChild<QPrintPreviewWidget*>();if(widget&&widget->pageCount()==0)widget->updatePreview();
                QJsonArray buttons;for(auto* control:preview->findChildren<QToolButton*>())if(control->property("print_operation").isValid()&&control->isVisibleTo(preview))buttons.append(control->property("print_operation").toString());
                workflowArtifacts_["plate_print_toolbar"]=QJsonObject{{"pages",widget?widget->pageCount():0},
                    {"allow_multipage",preview->property("studio_print_allow_multipage").toBool()},
                    {"multipage_tools",preview->property("studio_print_multipage_tools").toBool()},{"visible_buttons",buttons}};
                workflowCase("drag.real-plate-print-preview-multipage",widget&&widget->pageCount()>1&&preview->property("studio_print_multipage_tools").toBool());
                preview->grab().save(workflowOutput_+"/views/plate-print-multipage.png");preview->reject();
            });
            printButton->click();printScale->setValue(savedScale);
        }else workflowCases_.append(QJsonObject{{"name","drag.plate-print-entry"},{"status","not_run"}});
        for(const auto* format:{"json","dxf","txt","png","pdf"})workflowCase("drag.export."+QString(format),dialog.exportFile(workflowOutput_+"/drag."+format,format).isEmpty());
        dialog.reject();captureWorkflow("after");workflowCase("drag.document-unchanged",!session_.modified());finishWorkflow();return;
    }

    QList<QAction*> candidates;for(auto*a:findChildren<QAction*>())if(a->property("operation")=="develop_plates")candidates<<a;
    if(candidates.size()!=1||!candidates[0]->isEnabled()){workflowCases_.append(QJsonObject{{"name","plate-entry"},{"status","not_run"}});finishWorkflow();return;}
    auto* timer=new QTimer(this);timer->setInterval(10);auto stage=std::make_shared<int>(0);
    connect(timer,&QTimer::timeout,this,[this,timer,stage]{
        if(!platesDialog_||session_.busy())return;
        if(auto* task=calculationSession("plates:develop"))if(task->busy())return;
        auto button=[this](QString name){QList<QPushButton*> found;for(auto*b:platesDialog_->findChildren<QPushButton*>())if(b->property("plates_action")==name)found<<b;return found.size()==1?found[0]:nullptr;};
        auto end=[this,timer]{timer->stop();timer->deleteLater();platesDialog_->reject();};
        if(*stage==0){auto*b=button("calculate");if(!b){workflowCases_.append(QJsonObject{{"name","plate-calculate-entry"},{"status","not_run"}});end();return;}*stage=1;b->click();return;}
        if(*stage==2){workflowCase("plates.cancel-preserves-result",!platesDialog_->failure().isEmpty()&&platesDialog_->result()==workflowArtifacts_["plate_data"].toObject());end();return;}
        if(!platesDialog_->failure().isEmpty()){
            const auto failure=platesDialog_->failure();workflowCase("plates.expected-failure",platesRequest_["expect_failure"].toBool(),failure);
            auto* label=platesDialog_->findChild<QLabel*>("PlateStatus");workflowCase("plates.failure-warning-style",label&&label->property("studio_message_role")=="warning");
            if(platesRequest_["expect_failure"].toBool()){
                const auto previousLanguage=uiLanguage();QJsonArray localized;
                for(const auto& language:QStringList{"zh","en"}){
                    if(!setUiLanguage(language).isEmpty()){finishWorkflow("Plate language catalog unavailable");return;}
                    platesDialog_->showFailure(failure);QCheckBox* checkbox=nullptr;
                    for(auto* box:layers_->findChildren<QCheckBox*>())if(box->property("modelField")=="developable")checkbox=box;
                    const QString caption=checkbox?checkbox->text():QString{};bool explanation=false;
                    for(auto* note:platesDialog_->findChildren<QLabel*>())if(note->property("studio_message_role")=="explanation"&&note->text().contains(caption)&&!caption.isEmpty())explanation=true;
                    workflowCase("plates."+language+"-error-refers-to-actual-checkbox",checkbox&&label&&label->text().contains(caption)&&explanation);
                    workflowCase("plates."+language+"-developable-caption",caption==(language=="zh"?QString::fromUtf8("可展开曲面"):QString("Developable surface")));
                    localized.append(QJsonObject{{"language",language},{"checkbox",caption},{"message",label?label->text():QString{}},{"explanation_matches",explanation}});
                }
                workflowArtifacts_["plate_developable_labels"]=localized;setUiLanguage(previousLanguage);
            }
            end();return;
        }
        if(!platesDialog_->hasResult())return;
        if(platesRequest_["verify_names"].toBool()){
            const auto originalLanguage=uiLanguage();
            QJsonObject observed;
            for(const auto& language:QStringList{"en","zh","ru","ja","de","fr","es"}){
                if(!setUiLanguage(language).isEmpty()){finishWorkflow("Plate language catalog unavailable");return;}
                QJsonArray rows,canvas;
                if(auto* list=platesDialog_->findChild<QListWidget*>())
                    for(int i=0;i<list->count();++i)rows.append(list->item(i)->text());
                if(auto* view=platesDialog_->findChild<QGraphicsView*>())
                    for(auto* item:view->scene()->items())
                        if(auto* label=dynamic_cast<QGraphicsSimpleTextItem*>(item);label&&label->isVisible())canvas.append(label->text());
                observed[language]=QJsonObject{{"list",rows},{"canvas",canvas}};
            }
            setUiLanguage(originalLanguage);
            workflowArtifacts_["plate_names_by_language"]=observed;
            end();return;
        }
        if(!workflowArtifacts_.contains("plate_message_styles")){
            QJsonArray messages;for(auto* label:platesDialog_->findChildren<QLabel*>())if(label->property("studio_message_role").isValid())messages.append(QJsonObject{{"role",label->property("studio_message_role").toString()},{"text",label->text()}});
            auto* status=platesDialog_->findChild<QLabel*>("PlateStatus");auto* table=platesDialog_->findChild<QTableWidget*>("PlateMetrics");QJsonArray cells;
            if(table)for(int row=0;row<table->rowCount();++row){QJsonArray values;for(int col=0;col<table->columnCount();++col)values.append(table->item(row,col)->text());cells.append(values);}
            workflowArtifacts_["plate_message_styles"]=QJsonObject{{"messages",messages},{"metrics",cells}};
            workflowCase("plates.success-feedback-style",status&&status->property("studio_message_role")=="feedback");
            workflowCase("plates.metrics-are-result-table",table&&table->rowCount()==6&&table->columnCount()==3&&table->property("studio_content_role")=="result_data");
            workflowCase("plates.explanation-separate-from-feedback",std::any_of(messages.begin(),messages.end(),[](const QJsonValue& value){return value.toObject()["role"]=="explanation";}));
        }
        if(platesRequest_["drag_benchmark"].toBool()){
            workflowArtifacts_["drag_measurement"]=platesDialog_->measureDrag();
            for(const auto* format:{"json","dxf","txt","png","pdf"})workflowCase("drag.export."+QString(format),platesDialog_->exportFile(workflowOutput_+"/drag."+format,format).isEmpty());
            end();return;
        }
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
        // Show the result/notice area, not just the controls at the top of the
        // sidebar. Only this private dialog's scroll position is changed.
        if(auto* table=platesDialog_->findChild<QTableWidget*>("PlateMetrics"))for(auto* scroll:platesDialog_->findChildren<QScrollArea*>())
            if(scroll->widget()&&scroll->widget()->isAncestorOf(table)){
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                const int previous=scroll->verticalScrollBar()->value();scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                platesDialog_->grab().save(workflowOutput_+"/views/plate-messages.png");
                QJsonArray widths;for(int col=0;col<table->columnCount();++col)widths.append(table->columnWidth(col));
                workflowArtifacts_["plate_metric_layout"]=QJsonObject{{"width",table->width()},{"height",table->height()},{"columns",widths},
                    {"horizontal_scroll",table->horizontalScrollBar()->maximum()},{"vertical_scroll",table->verticalScrollBar()->maximum()}};
                scroll->verticalScrollBar()->setValue(previous);break;
            }
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
                const QDir workspace(QFileInfo(QFileInfo(calculationSession("plates:develop")?calculationSession("plates:develop")->modelPath():session_.modelPath()).absolutePath()).absolutePath());
                const auto previous=workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot);
                *stage=2;run->click();
                if(platesRequest_["late_cancel"].toBool()){
                    QElapsedTimer deadline;deadline.start();bool produced=false;
                    while(!produced && deadline.elapsed()<3000){
                        for(const auto &name:workspace.entryList({"request-*"},QDir::Dirs|QDir::NoDotAndDotDot))
                            if(!previous.contains(name) && QFileInfo::exists(workspace.filePath(name+"/plates.json")))produced=true;
                        if(!produced)QThread::msleep(1);
                    }
                    workflowCase("plates.late-result-produced-before-cancel",produced && (calculationSession("plates:develop")?calculationSession("plates:develop")->busy():session_.busy()));
                }
                stop->click();return;
            }
        }
        end();
    });timer->start();candidates[0]->trigger();
}
