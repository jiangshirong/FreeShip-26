// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native/document.h"
#include <QList>
#include <QPolygonF>
#include <QSet>
#include <atomic>

namespace fs::frames {
// All exported coordinates are millimetres. A closed outline repeats its first point.
struct Station {
    int number = 0;
    double x = 0;
    QList<QPolygonF> outlines;
    QString error;
};
struct Options {
    QSet<int> indices;          // Revision-local, zero-based station numbers.
    int layer = -1;             // -1 uses all layers participating in intersections.
    double thickness = 0;       // Zero requests the original outer lines.
    bool closeSheer = false;    // Explicit straight top for an open mould.
};
QList<Station> sectionTemplates(const native_geometry::DocumentInput& input, const Options& options,
                                const std::atomic_bool& cancelled);
}

