// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QShortcut>
#include <QUndoStack>
#include <QWidget>
#include <functional>
#include <utility>

class AppliedDraftChange final : public QUndoCommand {
public:
    AppliedDraftChange(std::function<void()> undo,std::function<void()> redo)
        :undo_(std::move(undo)),redo_(std::move(redo)){}
    void undo()override{undo_();}
    void redo()override{
        if(std::exchange(first_,false))return;
        redo_();
    }
private:
    std::function<void()> undo_,redo_;
    bool first_=true;
};

inline void bindDraftHistory(QWidget* window,QUndoStack& history,std::function<bool()> available={}){
    auto* undo=new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_Z),window);
    auto* redo=new QShortcut(QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_Z),window);
    undo->setContext(Qt::WindowShortcut);redo->setContext(Qt::WindowShortcut);
    QObject::connect(undo,&QShortcut::activated,&history,[&history,available]{
        if(!available||available())history.undo();
    });
    QObject::connect(redo,&QShortcut::activated,&history,[&history,available]{
        if(!available||available())history.redo();
    });
}
