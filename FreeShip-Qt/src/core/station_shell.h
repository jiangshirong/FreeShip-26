// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPolygonF>
#include <array>
#include <vector>
#include <atomic>

namespace fs::frames {
struct ShellPoint { double x; QPointF yz; };
using ShellTriangle = std::array<ShellPoint,3>;
// Closed section polygons, bounded by the outer skin and (for an open boat)
// the explicitly chosen sheer chord. Measure thickness against real 3D faces only.
QList<QPolygonF> innerContours(const QList<QPolygonF>& exterior, const std::vector<ShellTriangle>& nearby,
                              double stationX, double thickness, const std::atomic_bool& cancelled);
}

