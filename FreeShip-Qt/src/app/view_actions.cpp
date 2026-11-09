// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "preferences_store.h"
#include "light_dialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QSaveFile>
#include <QStatusBar>

void MainWindow::viewAction(HullView* view,const QString& action){
    if(!session_.ready()||!view||view->snapshot()!=session_.model())return;
    if(action=="light_settings"){openLightDialog(view);return;}
    // Capture the invoked view, with final labels, before a modal dialog can
    // change the current tab, focus or document. Never export all four cameras.
    const auto pixels=view->image();
    if(action=="save_image"){
        QFileDialog dialog(this,"保存当前视图图片",preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()),"PNG 图片 (*.png)");
        dialog.setProperty("view_operation","save_image_file");dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("png");
        if(workflow_){dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setAttribute(Qt::WA_DontShowOnScreen);}
        if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return;
        const auto path=dialog.selectedFiles().front();QSaveFile target(path);target.setDirectWriteFallback(false);
        if(!target.open(QIODevice::WriteOnly)||!pixels.save(&target,"PNG")||!target.commit())showError("图片保存失败，原文件未被替换："+target.errorString());
        else statusBar()->showMessage("图片已保存："+path);
    }else if(action=="print"){
        QPrinter printer(QPrinter::HighResolution);printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageOrientation(QPageLayout::Landscape);
        QPrintPreviewDialog preview(&printer,this);preview.setProperty("view_operation","print_preview");preview.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        connect(&preview,&QPrintPreviewDialog::paintRequested,this,[pixels](QPrinter* device){
            QPainter painter(device);if(!painter.isActive())return;
            const QRectF page=device->pageLayout().paintRectPixels(device->resolution());QSizeF size=pixels.size();size.scale(page.size(),Qt::KeepAspectRatio);
            painter.drawImage(QRectF(page.center()-QPointF(size.width()/2,size.height()/2),size),pixels);
        });
        preview.exec();
    }
}
void MainWindow::openLightDialog(HullView* view){
    auto* dialog=lightDialogs_.value(view,nullptr);
    if(!dialog){
        dialog=new LightDialog(this);lightDialogs_.insert(view,dialog);dialog->setWindowTitle("光源 · "+view->projectionLabel());
        dialog->setProperty("light_projection",int(view->projection()));dialog->onLightChanged=[view](const HullLight& light){view->setLight(light);};
    }
    dialog->setLight(view->light());dialog->setAttribute(Qt::WA_DontShowOnScreen,workflow_);dialog->show();dialog->raise();dialog->activateWindow();
}
void MainWindow::refreshLightDialog(HullView* view){if(auto* dialog=lightDialogs_.value(view,nullptr))dialog->setLight(view->light());}
