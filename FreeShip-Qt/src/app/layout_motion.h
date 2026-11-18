// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QList>
#include <functional>
class QWidget;
class QSplitter;
namespace layout_motion {
// Call once for each feature dialog, while it is still hidden.
void installFeatureWindow(QWidget* window);
// applyTargetLayout must synchronously establish visibility and final layout.
// Frames retain their real widgets and renderers; no scaled snapshots are used.
void animateViewLayout(QWidget* container,const QList<QWidget*>& frames,const std::function<void()>& applyTargetLayout);
void installSplitter(QSplitter* splitter);
}
