// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui_language.h"
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QToolTip>
#include <QCursor>

// Session-only UI switch. Independent of installer role and updater permissions.
class DeveloperMode final : public QObject {
    QAction toggle_{this};
    DeveloperMode():QObject(qApp){toggle_.setCheckable(true);qApp->installEventFilter(this);}
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()!=QEvent::KeyPress)return false;
        auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()!=Qt::Key_F12 || key->modifiers()!=(Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier))return false;
        if(!key->isAutoRepeat()){
            toggle_.toggle();
            QToolTip::showText(QCursor::pos(),toggle_.isChecked()?uiText("开发者模式已开启"):uiText("开发者模式已关闭"));
        }
        return true;
    }
public:
    static QAction& toggle(){static auto* mode=new DeveloperMode;return mode->toggle_;}
};
