// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pdf_export_dialog.h"
#include <QApplication>
#include <QEventLoop>
#include <QTimer>

inline void waitForPdfPreview(int ms) {
    QEventLoop turn;
    QTimer::singleShot(ms,&turn,&QEventLoop::quit);
    turn.exec(QEventLoop::ExcludeUserInputEvents);
}
inline bool observePdfPreviewMotion(QDialog* dialog,const std::function<void()>& change) {
    auto* page=dynamic_cast<PdfPagePreview*>(dialog->findChild<QWidget*>("PdfFirstPagePreview"));
    if(!page||!page->isVisible())return false;
    const auto previous=qApp->property("studio_motion_enabled");
    qApp->setProperty("studio_motion_enabled",true);
    waitForPdfPreview(340);
    const auto from=page->currentFactor();const auto before=page->grab().toImage();
    change();waitForPdfPreview(120);
    const auto middle=page->currentFactor();const auto frame=page->grab().toImage();
    waitForPdfPreview(240);
    const auto to=page->currentFactor();const auto after=page->grab().toImage();
    qApp->setProperty("studio_motion_enabled",previous);
    return middle>qMin(from,to)&&middle<qMax(from,to)&&before!=frame&&frame!=after;
}
