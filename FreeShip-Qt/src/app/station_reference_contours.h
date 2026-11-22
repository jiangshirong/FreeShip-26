// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include <QPolygonF>
#include <atomic>

namespace fs {
inline constexpr double stationMatchToleranceMm = 0.1;
inline constexpr double referenceFlattenToleranceMm = 0.01;

enum class ReferenceSide { Both, Positive, Negative };
struct StationReference {
    double xMm = 0;
    QVector<QPolygonF> linesMm;
    ReferenceSide side = ReferenceSide::Both;
};

QVector<StationReference> stationReferences(const QVector<ReferenceMarker>& markers,
                                           double mmPerUnit,
                                           const std::atomic_bool* cancel = nullptr);
QVector<QPolygonF> clipStationToReferenceSide(const QVector<QPolygonF>& modelMm,
                                              ReferenceSide side,
                                              const std::atomic_bool* cancel = nullptr);
}
