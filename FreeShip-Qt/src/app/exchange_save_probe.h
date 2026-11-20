// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QTimer>
#include <functional>

// Answer the real save dialog after the isolated workflow clicks Export.
inline void answerExchangeSaveDialog(QWidget* parent,const QString& target,bool cancel=false,
                                     std::function<void(const QString&)> observe={}) {
    QTimer::singleShot(0,parent,[parent,target,cancel,observe]{
        auto* picker=parent->findChild<QFileDialog*>(QString{},Qt::FindDirectChildrenOnly);
        if(!picker||picker->acceptMode()!=QFileDialog::AcceptSave)return;
        if(observe)observe(picker->selectedFiles().value(0));
        if(cancel){picker->reject();return;}
        const QFileInfo file(target);
        picker->setDirectory(file.absolutePath());picker->selectFile(file.fileName());
        if(auto* edit=picker->findChild<QLineEdit*>("fileNameEdit"))edit->setText(file.fileName());
        QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
    });
}
