// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
class QTableView;

// Read-only data surfaces: content-sized columns, native text zoom and pan.
// Form editors and tables with embedded buttons keep their existing layouts.
void configureStudioDataTable(QTableView* table,bool matchHeaderFont=false,int wrapColumn=-1);
void configureStudioDataTable(QTableView* table,bool matchHeaderFont,int wrapColumn,bool fitRowHeaders);
void refreshStudioDataTable(QTableView* table);
