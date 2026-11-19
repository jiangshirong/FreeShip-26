// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QStringList>
class QWidget;
class QTableView;
class QTableWidget;
// Presentation only: these functions never mutate model values or undo state.
namespace data_motion {
void beginTableUpdate(QTableView* table);
void finishTableUpdate(QTableView* table,const QString& domain);
void beginRowSplice(QTableWidget* table,int first,int count);
void beginRowUpdate(QTableWidget* table);
void finishRowUpdate(QTableWidget* table,const QString& domain,const QStringList& identities);
void installProperties(QWidget* panel);
void pulseProperties(QWidget* panel,const QString& identity);
}
