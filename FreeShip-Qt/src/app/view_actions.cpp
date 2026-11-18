#include "app/ui_language.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "studio_ui.h"
#include "preferences_store.h"
#include "light_dialog.h"
#include "print_preview_style.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QSaveFile>
#include <QStatusBar>

void MainWindow::viewAction(HullView* view,const QString& action){
    if(!session_.ready()||!view||view->snapshot()!=session_.model())return;
    if(action=="clear_selection"){modelingAction(action);return;}
    if(action=="light_settings"){openLightDialog(view);return;}
    // Capture the invoked view, with final labels, before a modal dialog can
    // change the current tab, focus or document. Never export all four cameras.
    if(action=="save_image"){
        const auto pixels=view->image();
        QFileDialog dialog(this,uiText("保存当前视图图片"),preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()),uiText("PNG 图片 (*.png)"));
        dialog.setProperty("view_operation","save_image_file");dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("png");
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return;
        const auto path=dialog.selectedFiles().front();QSaveFile target(path);target.setDirectWriteFallback(false);
        if(!target.open(QIODevice::WriteOnly)||!pixels.save(&target,"PNG")||!target.commit())showError(uiText("图片保存失败，原文件未被替换：")+target.errorString());
        else showStatusMessage(uiText("图片已保存：")+path);
    }else if(action=="print"){
        QPrinter printer(QPrinter::HighResolution);printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageOrientation(QPageLayout::Landscape);
        const auto rasterPage=printer.pageLayout().paintRectPixels(300);
        const auto source=view->size();
        const auto picture=view->printPicture(qMin(qreal(rasterPage.width())/source.width(),qreal(rasterPage.height())/source.height()));
        QPrintPreviewDialog preview(&printer,this);preview.setProperty("view_operation","print_preview");preview.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        preview.setWindowTitle(uiText("打印预览"));setStudioWindowIcon(&preview,"Print");
        styleStudioPrintPreview(&preview);
        preview.setProperty("print_renderer","vector-picture");preview.setProperty("print_source_size",source);
        connect(&preview,&QPrintPreviewDialog::paintRequested,this,[picture,source](QPrinter* device){
            QPainter painter(device);if(!painter.isActive())return;
            painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing|QPainter::SmoothPixmapTransform);
            // QPrinter's painter origin is already at the printable area's
            // top-left. Adding paintRect's margin again offsets/crops the page.
            const QRectF page(QPointF(),device->pageLayout().paintRectPixels(device->resolution()).size());QSizeF size=source;size.scale(page.size(),Qt::KeepAspectRatio);
            painter.translate(page.center()-QPointF(size.width()/2,size.height()/2));painter.scale(size.width()/source.width(),size.height()/source.height());
            painter.setClipRect(QRectF(QPointF(),source));
            // QPicture itself compensates for device DPI. Our page-fit scale
            // is already in printer pixels, so cancel that extra conversion.
            painter.scale(qreal(picture.logicalDpiX())/device->logicalDpiX(),qreal(picture.logicalDpiY())/device->logicalDpiY());
            painter.drawPicture(QPointF(),picture);
        });
        preview.exec();
    }
}
void MainWindow::openLightDialog(HullView* view){
    auto* dialog=lightDialogs_.value(view,nullptr);
    if(!dialog){
        dialog=new LightDialog(this);lightDialogs_.insert(view,dialog);dialog->setWindowTitle(uiText("光源 · ")+view->projectionLabel());
        dialog->setProperty("light_projection",int(view->projection()));dialog->onLightChanged=[view](const HullLight& light){view->setLight(light);};
    }
    dialog->setLight(view->light());dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);dialog->show();dialog->raise();dialog->activateWindow();
}
void MainWindow::refreshLightDialog(HullView* view){if(auto* dialog=lightDialogs_.value(view,nullptr))dialog->setLight(view->light());}
