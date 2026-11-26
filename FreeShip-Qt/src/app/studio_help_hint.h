// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QEnterEvent>
#include <QToolButton>
#include <QToolTip>

class StudioHelpHint final : public QToolButton {
public:
    explicit StudioHelpHint(QWidget* parent, int size = 32) : QToolButton(parent) {
        setText(QStringLiteral("?"));
        setAutoRaise(false);
        setFixedSize(size, size);
        setCursor(Qt::WhatsThisCursor);
        setStyleSheet(
            "QToolButton{background:#f7faf7;color:#245c4b;border:1px solid #c4d6cc;border-radius:6px;font-size:16px;font-weight:600}"
            "QToolButton:hover{background:#e6f1ea;border-color:#8eb6a1}"
            "QToolButton:pressed{background:#d4e8dc;border-color:#6f9e88}");
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        QToolButton::enterEvent(event);
        QToolTip::showText(mapToGlobal(rect().bottomRight()), toolTip(), this, rect(), 12000);
    }

    void leaveEvent(QEvent* event) override {
        if (QToolTip::text() == toolTip())
            QToolTip::hideText();
        QToolButton::leaveEvent(event);
    }

    bool event(QEvent* event) override {
        if (event->type() == QEvent::ToolTip)
            return true;
        return QToolButton::event(event);
    }
};
