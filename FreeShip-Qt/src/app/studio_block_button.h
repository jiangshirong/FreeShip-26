// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QApplication>
#include <QMouseEvent>
#include <QToolButton>
#include <functional>

class StudioBlockButton : public QToolButton {
public:
    explicit StudioBlockButton(QWidget* parent=nullptr):QToolButton(parent){
        setContextMenuPolicy(Qt::PreventContextMenu);
    }
    std::function<void()> onRemove;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()==Qt::RightButton){
            rightPressed_=true;
            event->accept();
            return;
        }
        if(event->button()==Qt::LeftButton){
            pressPosition_=event->position().toPoint();
            leftDragged_=false;
        }
        QToolButton::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(event->buttons().testFlag(Qt::LeftButton)){
            leftDragged_|=(event->position().toPoint()-pressPosition_).manhattanLength()>=QApplication::startDragDistance();
            if(leftDragged_){setDown(false);event->accept();return;}
        }
        QToolButton::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==Qt::RightButton){
            const bool remove=rightPressed_&&rect().contains(event->position().toPoint());
            rightPressed_=false;
            event->accept();
            if(remove&&onRemove)onRemove();
            return;
        }
        if(event->button()==Qt::LeftButton&&leftDragged_){
            leftDragged_=false;
            setDown(false);
            QToolButton::mouseReleaseEvent(event);
            event->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }

private:
    QPoint pressPosition_;
    bool leftDragged_=false,rightPressed_=false;
};
