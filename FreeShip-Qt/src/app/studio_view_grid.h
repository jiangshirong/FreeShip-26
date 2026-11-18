// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QList>
class QGridLayout;
class QWidget;
namespace studio_view_grid {
// Child handles occupy only the gaps; neither views nor overlays are reparented.
void install(QGridLayout* grid,const QList<QWidget*>& frames);
void setVisibleCount(QGridLayout* grid,int count);
}
