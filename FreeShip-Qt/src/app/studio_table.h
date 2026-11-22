// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
class QTableView;
struct StudioHeaderGroup {
    QString title;
    int first;
    QStringList labels;
};
void setStudioHeaderGroups(QTableView* table,QList<StudioHeaderGroup> groups);
QString studioHeaderCaption(const QTableView* table,int column);

// Read-only data surfaces: content-sized columns, native text zoom and pan.
// Form editors and tables with embedded buttons keep their existing layouts.
void configureStudioDataTable(QTableView* table,bool matchHeaderFont=false,int wrapColumn=-1);
void configureStudioDataTable(QTableView* table,bool matchHeaderFont,int wrapColumn,bool fitRowHeaders);
void refreshStudioDataTable(QTableView* table);
