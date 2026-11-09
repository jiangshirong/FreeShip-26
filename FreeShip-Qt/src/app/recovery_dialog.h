// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
class QListWidget;
class RecoveryDialog final:public QDialog {
public:
    explicit RecoveryDialog(const QString& root,QWidget* parent=nullptr);
    QString selectedId()const;
private:
    QListWidget* list_;
};
