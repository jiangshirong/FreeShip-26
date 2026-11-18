// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextTable>
#include <QWheelEvent>

inline QJsonObject probeReportNavigation(QTextBrowser& browser){
    const auto snapshot=[&]{
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        const auto* document=browser.document();
        auto* horizontal=browser.horizontalScrollBar();
        int tables=0;
        double right=0;
        for(auto* frame:document->rootFrame()->childFrames())if(qobject_cast<QTextTable*>(frame)){
            ++tables;
            right=qMax(right,document->documentLayout()->frameBoundingRect(frame).right());
        }
        return QJsonObject{{"font",document->defaultFont().pointSizeF()},{"tables",tables},
            {"viewport",browser.viewport()->width()},{"rightmost_table_edge",right},{"document_width",document->size().width()},
            {"horizontal_range",horizontal->maximum()},{"horizontal_visible",horizontal->isVisibleTo(browser.window())},
            {"vertical_range",browser.verticalScrollBar()->maximum()},{"scroll_y",browser.verticalScrollBar()->value()}};
    };
    const auto wheel=[&](int steps,Qt::KeyboardModifiers modifiers){
        const QPoint pos=browser.viewport()->rect().center();
        QWheelEvent event(pos,browser.viewport()->mapToGlobal(pos),{},QPoint(0,120*steps),
                          Qt::NoButton,modifiers,Qt::NoScrollPhase,false);
        QApplication::sendEvent(browser.viewport(),&event);
    };
    const auto original=snapshot();
    const auto text=browser.toPlainText();
    const auto minimum=browser.minimumWidth(),maximum=browser.maximumWidth();
    browser.setFixedWidth(qMin(browser.width(),420));
    const auto narrow=snapshot();
    browser.verticalScrollBar()->setValue(0);
    wheel(-1,Qt::NoModifier);
    const auto scroll=snapshot();
    for(int i=0;i<10;++i)wheel(1,Qt::ControlModifier);
    const auto enlarged=snapshot();
    QJsonArray shrinking;
    for(int i=0;i<32;++i){
        wheel(-1,Qt::ControlModifier);
        if(i==9||i==14||i==19||i==31)shrinking.append(snapshot());
    }
    const bool sameText=browser.toPlainText()==text;
    browser.setMinimumWidth(minimum);browser.setMaximumWidth(maximum);
    return {{"original",original},{"narrow",narrow},{"scroll",scroll},{"enlarged",enlarged},
        {"shrinking",shrinking},{"same_text",sameText}};
}
