// Isolated actual view-action / PrintSupport callback inspection. No printer jobs.
#include "main_window.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "print_preview_style.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QPainter>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPrintPreviewWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QGraphicsView>
#include <QScrollBar>
#include <QMouseEvent>
#include <QEventLoop>
#include <QLineEdit>
#include <stdexcept>

void MainWindow::auditPrintPreview(){
    if(!startupProbe_||session_.busy()||!session_.ready()){finishWorkflow("Print observer requires a private idle document");return;}
    const bool oldWorkflow=workflow_;workflow_=true;const auto oldLanguage=uiLanguage();
    QDir().mkpath(workflowOutput_+"/print");const auto model=session_.model();QJsonArray observations;
    auto actions=[](QPrintPreviewDialog* dialog){
        QJsonArray list;for(auto* a:dialog->findChildren<QAction*>())if(!a->isSeparator()&&!a->text().isEmpty())
            list.append(QJsonObject{{"text",a->text()},{"tooltip",a->toolTip()},{"operation",a->property("print_operation").toString()},
                {"icon",a->icon().name()},{"enabled",a->isEnabled()},{"checked",a->isChecked()}});
        return list;
    };
    const auto button=[](QPrintPreviewDialog* preview,const QString& role)->QToolButton*{
        QList<QToolButton*> found;for(auto* b:preview->findChildren<QToolButton*>())if(b->property("print_operation")==role)found.append(b);
        if(found.size()!=1)throw std::runtime_error(("Print control missing or ambiguous: "+role).toStdString());return found.front();
    };
    const auto click=[&](QPrintPreviewDialog* preview,const QString& role){auto* control=button(preview,role);if(!control->isEnabled())throw std::runtime_error(("Disabled print control: "+role).toStdString());control->click();};
    const auto toolbarState=[](QPrintPreviewDialog* preview){
        QJsonObject state;auto* bar=preview->findChild<QToolBar*>("StudioPrintToolbar");if(!bar)return state;
        QJsonArray buttons,controls;
        for(auto* b:bar->findChildren<QToolButton*>())if(b->property("print_operation").isValid()){
            const auto position=b->mapTo(bar,QPoint());
            buttons.append(QJsonObject{{"operation",b->property("print_operation").toString()},{"width",b->width()},{"height",b->height()},
                {"visible",b->isVisibleTo(preview)},{"x",position.x()},{"right",position.x()+b->width()-1},{"icon",b->icon().name()}});
        }
        for(auto* control:bar->findChildren<QWidget*>())if(control->property("print_control").isValid())
            controls.append(QJsonObject{{"role",control->property("print_control").toString()},{"visible",control->isVisibleTo(preview)}});
        auto* widget=preview->findChild<QPrintPreviewWidget*>();
        return QJsonObject{{"buttons",buttons},{"controls",controls},{"width",bar->width()},
            {"pages",widget?widget->pageCount():0},{"multipage_tools",preview->property("studio_print_multipage_tools").toBool()}};
    };
    try{
        for(int sample=0;sample<5;++sample){
            auto* view=views_[sample%4];const bool filled=sample==4;
            view->resize(960,540);view->fit();view->finishPresentation();view->setFillVisible(filled);setUiLanguage("zh");
            bool captured=false;QString failure;QTimer timer;timer.setSingleShot(true);
            connect(&timer,&QTimer::timeout,this,[&]{
                auto* preview=qobject_cast<QPrintPreviewDialog*>(frontStudioDialog());
                if(!preview){failure="Production print preview did not open";return;}
                try{
                QJsonObject row{{"projection",int(view->projection())},{"filled",filled},{"projection_label",view->projectionLabel()},{"renderer",preview->property("print_renderer").toString()},
                    {"title_zh",preview->windowTitle()},{"actions_zh",actions(preview)}};
                auto* widget=preview->findChild<QPrintPreviewWidget*>();
                if(widget){widget->fitInView();preview->grab().save(workflowOutput_+"/print/preview-"+QString::number(int(view->projection()))+".png");}
                if(sample==0){
                    auto* bar=preview->findChild<QToolBar*>("StudioPrintToolbar");auto state=toolbarState(preview);
                    state["image"]=workflowOutput_+"/print/toolbar.png";state["style"]=bar->styleSheet();state["styled"]=preview->property("studio_print_preview").toBool();
                    workflowArtifacts_["print_toolbar"]=state;
                    bar->grab().save(workflowOutput_+"/print/toolbar.png");
                    const auto zoom=widget->zoomFactor();click(preview,"ZoomIn");const auto larger=widget->zoomFactor();click(preview,"ZoomOut");
                    workflowCase("print-toolbar-zoom-buttons",larger>zoom&&widget->zoomFactor()<larger);
                    click(preview,"FitWidth");workflowCase("print-toolbar-fit-width",widget->zoomMode()==QPrintPreviewWidget::FitToWidth);
                    click(preview,"FitPage");workflowCase("print-toolbar-fit-page",widget->zoomMode()==QPrintPreviewWidget::FitInView);
                    bool setupNonModal=false,printNonModal=false;
                    QTimer::singleShot(0,preview,[&]{
                        auto* setup=preview->findChild<QDialog*>("PrintPageSetup");
                        if(!setup){if(auto* fallback=frontStudioDialog();fallback&&fallback!=preview)fallback->reject();return;}
                        setupNonModal=setup->isVisible()&&!setup->isModal()&&!QApplication::activeModalWidget();
                        setup->findChild<QComboBox*>("PrintPageOrientation")->setCurrentIndex(0);
                        setup->accept();
                    });
                    click(preview,"Setup");
                    workflowCase("print-page-setup-is-nonmodal-and-updates-the-preview",setupNonModal&&widget->orientation()==QPageLayout::Portrait);
                    widget->setLandscapeOrientation();workflowCase("print-orientation-api-landscape",widget->orientation()==QPageLayout::Landscape);
                    QTimer::singleShot(0,preview,[&]{
                        auto* setup=preview->findChild<QDialog*>("PrintDeviceSetup");
                        if(!setup){if(auto* fallback=frontStudioDialog();fallback&&fallback!=preview)fallback->reject();return;}
                        printNonModal=setup->isVisible()&&!setup->isModal()&&!QApplication::activeModalWidget();
                        setup->reject();
                    });
                    click(preview,"Print");
                    workflowCase("print-device-choice-is-nonmodal-and-can-cancel",printNonModal);
                    workflowCase("print-single-page-navigation-hidden",widget->pageCount()==1&&!button(preview,"Previous")->isVisibleTo(preview)&&!button(preview,"Next")->isVisibleTo(preview));
                    click(preview,"FitPage");
                    QJsonArray navigation;widget->setZoomFactor(widget->zoomFactor()*4);
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    for(auto* canvas:preview->findChildren<QGraphicsView*>()){
                        auto* viewport=canvas->viewport();const auto start=viewport->rect().center();const auto point=canvas->mapToScene(start);
                        auto mouse=[&](QEvent::Type type,QPoint at,Qt::MouseButton button,Qt::MouseButtons buttons){
                            QMouseEvent event(type,QPointF(at),QPointF(viewport->mapToGlobal(at)),button,buttons,Qt::NoModifier);QApplication::sendEvent(viewport,&event);
                        };
                        mouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                        mouse(QEvent::MouseMove,start+QPoint(26,18),Qt::NoButton,Qt::LeftButton);
                        mouse(QEvent::MouseButtonRelease,start+QPoint(26,18),Qt::LeftButton,Qt::NoButton);
                        const auto delta=canvas->mapFromScene(point)-start;
                        navigation.append(QJsonObject{{"horizontal_hidden",!canvas->horizontalScrollBar()->isVisibleTo(canvas)},
                            {"vertical_hidden",!canvas->verticalScrollBar()->isVisibleTo(canvas)},
                            {"horizontal_policy_off",canvas->horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff},
                            {"vertical_policy_off",canvas->verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff},
                            {"dx",delta.x()},{"dy",delta.y()},{"hand_drag",canvas->dragMode()==QGraphicsView::ScrollHandDrag}});
                    }
                    workflowArtifacts_["print_canvas_navigation"]=navigation;click(preview,"FitPage");
                }
                setUiLanguage("en");row["actions_en"]=actions(preview);row["title_en"]=preview->windowTitle();
                setUiLanguage("zh");row["actions_zh_again"]=actions(preview);
                const auto prefix=workflowOutput_+"/print/view-"+QString::number(int(view->projection()))+(filled?"-filled":"");
                auto pdf=[&](const QString& path){QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(path);
                    printer.setResolution(600);printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageOrientation(QPageLayout::Landscape);
                    preview->paintRequested(&printer);};
                pdf(prefix+".pdf");row["pdf"]=prefix+".pdf";
                view->panBy({37,19});pdf(prefix+"-frozen.pdf");view->panBy({-37,-19});row["frozen_pdf"]=prefix+"-frozen.pdf";
                // Negative control: the old whole-view screenshot route. The
                // PDF inspection must distinguish this from vector print data.
                if(sample==0){
                    const auto path=prefix+"-raster-control.pdf";QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);
                    printer.setOutputFileName(path);printer.setResolution(600);QPainter painter(&printer);const auto pixels=view->image();
                    painter.drawImage(QRectF(printer.pageLayout().paintRectPixels(600)),pixels);row["raster_control"]=path;
                }
                observations.append(row);captured=true;preview->reject();
                }catch(const std::exception& error){failure=QString::fromUtf8(error.what());preview->reject();}
            });timer.start(0);viewAction(view,"print");
            if(!captured)throw std::runtime_error(failure.toStdString());
        }
        workflowArtifacts_["print_previews"]=observations;
        // Multi-page preview is shared by board printing. Use generated pages,
        // never a physical printer job, to exercise the real Qt navigation.
        {
            QPrinter printer(QPrinter::HighResolution);printer.setPageSize(QPageSize(QPageSize::A4));
            QPrintPreviewDialog preview(&printer,this);preview.setAttribute(Qt::WA_DontShowOnScreen);styleStudioPrintPreview(&preview,true);
            int pages=3;connect(&preview,&QPrintPreviewDialog::paintRequested,&preview,[&](QPrinter* device){QPainter painter(device);for(int i=0;i<pages;++i){if(i)device->newPage();painter.drawText(QPointF(100,200),QString("Preview test page %1").arg(i+1));}});
            preview.show();auto* widget=preview.findChild<QPrintPreviewWidget*>();widget->updatePreview();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);QJsonArray modes;
            auto record=[&](const QString& name){auto state=toolbarState(&preview);state["state"]=name;modes.append(state);
                preview.findChild<QToolBar*>("StudioPrintToolbar")->grab().save(workflowOutput_+"/print/toolbar-"+name+".png");};
            record("multi");
            workflowCase("print-multipage-count",widget->pageCount()==3);
            click(&preview,"Next");workflowCase("print-multipage-next",widget->currentPage()==2);
            click(&preview,"Previous");workflowCase("print-multipage-previous",widget->currentPage()==1);
            for(auto* edit:preview.findChildren<QLineEdit*>())if(edit->property("print_control")=="page"){
                edit->setText("3");QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection);
            }
            workflowCase("print-multipage-page-edit",widget->currentPage()==3);
            click(&preview,"Overview");workflowCase("print-overview-enters",widget->viewMode()==QPrintPreviewWidget::AllPagesView);
            click(&preview,"Overview");workflowCase("print-overview-returns-to-single",widget->viewMode()==QPrintPreviewWidget::SinglePageView);
            click(&preview,"Overview");pages=1;widget->updatePreview();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            record("became-single");workflowCase("print-shrinking-pages-leaves-overview",widget->viewMode()==QPrintPreviewWidget::SinglePageView);
            pages=3;widget->updatePreview();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);record("multi-again");
            workflowArtifacts_["print_toolbar_modes"]=modes;
            preview.close();
        }
        workflowCase("print-keeps-document",session_.model()==model);
    }catch(const std::exception& error){workflow_=oldWorkflow;setUiLanguage(oldLanguage);finishWorkflow(QString::fromUtf8(error.what()));return;}
    workflow_=oldWorkflow;setUiLanguage(oldLanguage);
}
