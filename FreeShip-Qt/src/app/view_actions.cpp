#include "app/ui_language.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "studio_ui.h"
#include "preferences_store.h"
#include "light_dialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QStatusBar>

void MainWindow::viewAction(HullView* view,const QString& action){
    if(!session_.ready()||!view||view->snapshot()!=session_.model())return;
    if(action=="clear_selection"){modelingAction(action);return;}
    if(action=="light_settings"){openLightDialog(view);return;}
    // Capture the invoked view, with final labels, before the user can
    // change the current tab, focus or document. Never export all four cameras.
    if(action=="save_image"){
        const auto pixels=view->image();
        QFileDialog dialog(this,uiText("保存当前视图图片"),preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()),uiText("PNG 图片 (*.png)"));
        dialog.setProperty("view_operation","save_image_file");dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("png");
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(runStudioDialog(dialog)!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return;
        const auto path=dialog.selectedFiles().front();QSaveFile target(path);target.setDirectWriteFallback(false);
        if(!target.open(QIODevice::WriteOnly)||!pixels.save(&target,"PNG")||!target.commit())showError(uiText("图片保存失败，原文件未被替换：")+target.errorString());
        else showStatusMessage(uiText("图片已保存：")+path);
    }else if(action=="export_pdf"){
        constexpr int dpi=300;
        const QPageLayout layout(QPageSize(QPageSize::A4),QPageLayout::Landscape,
                                 QMarginsF(10,10,10,10),QPageLayout::Millimeter);
        const QSizeF source=view->size();
        if(source.isEmpty())return;
        const QSizeF pageSize=layout.paintRectPixels(dpi).size();
        const auto picture=view->printPicture(qMin(pageSize.width()/source.width(),pageSize.height()/source.height()));
        const auto title=view->projectionLabel();
        QFileDialog dialog(this,uiText("导出 ")+QStringLiteral("PDF"),
                           preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()),QStringLiteral("PDF (*.pdf)"));
        dialog.setProperty("view_operation","export_pdf_file");
        dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("pdf");
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(runStudioDialog(dialog)!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return;
        const auto path=dialog.selectedFiles().front();QSaveFile target(path);target.setDirectWriteFallback(false);
        if(!target.open(QIODevice::WriteOnly)){showError(uiText("无法写入文件：")+target.errorString());return;}
        {
            QPdfWriter pdf(&target);pdf.setResolution(dpi);pdf.setTitle(title);
            if(!pdf.setPageLayout(layout)){showError(uiText("无法设置 PDF 纸张尺寸。"));return;}
            QPainter painter;
            if(!painter.begin(&pdf)){showError(uiText("无法写入 PDF 页面。"));return;}
            painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing|QPainter::SmoothPixmapTransform);
            const QRectF page(QPointF(),pageSize);
            QSizeF size=source;size.scale(page.size(),Qt::KeepAspectRatio);
            painter.translate(page.center()-QPointF(size.width()/2,size.height()/2));
            painter.scale(size.width()/source.width(),size.height()/source.height());
            painter.setClipRect(QRectF(QPointF(),source));
            // QPicture compensates for device DPI. The page fit already uses PDF device pixels.
            painter.scale(qreal(picture.logicalDpiX())/pdf.logicalDpiX(),qreal(picture.logicalDpiY())/pdf.logicalDpiY());
            painter.drawPicture(QPointF(),picture);
            if(!painter.end()){showError(uiText("无法写入 PDF 页面。"));return;}
        }
        if(!target.commit())showError(uiText("无法写入文件：")+target.errorString());
        else showStatusMessage(uiText("已导出：")+path);
    }
}
void MainWindow::openLightDialog(HullView* view){
    auto* dialog=lightDialogs_.value(view,nullptr);
    if(!dialog){
        dialog=new LightDialog(this);lightDialogs_.insert(view,dialog);
        const auto refreshTitle=[dialog,view=QPointer<HullView>(view)]{if(view)dialog->setWindowTitle(uiText("光源 · ")+view->projectionLabel());};
        refreshTitle();bindUiRefresh(dialog,refreshTitle);
        dialog->setProperty("light_projection",int(view->projection()));dialog->onLightChanged=[view](const HullLight& light){view->setLight(light);};
    }
    dialog->setLight(view->light());dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);dialog->show();dialog->raise();dialog->activateWindow();
}
void MainWindow::refreshLightDialog(HullView* view){if(auto* dialog=lightDialogs_.value(view,nullptr))dialog->setLight(view->light());}
