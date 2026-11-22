// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_reference_contours.h"
#include "render/auxiliary_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fs {
namespace {
bool cancelled(const std::atomic_bool* flag) {
    return flag && flag->load(std::memory_order_relaxed);
}

bool coincident(QPointF a, QPointF b) {
    return a.x() == b.x() && a.y() == b.y();
}

void appendPoint(QPolygonF& run, QPointF p) {
    if (run.isEmpty() || !coincident(run.back(), p))
        run.append(p);
}

void finishRun(QVector<QPolygonF>& lines, QPolygonF& run) {
    if (run.size() >= 2)
        lines.append(std::move(run));
    run.clear();
}

double distanceToChord(QPointF p, QPointF a, QPointF b) {
    const auto chord = b - a;
    const double length = std::hypot(chord.x(), chord.y());
    const auto along = length > 0 ? std::clamp(QPointF::dotProduct(p-a, chord/length), 0., length) : 0.;
    const auto foot = length > 0 ? a + (chord/length)*along : a;
    return std::hypot(p.x()-foot.x(), p.y()-foot.y());
}

using Cubic2d = std::array<QPointF, 4>;

void appendReferencePoint(QPolygonF& run, QPointF p, qsizetype& remaining) {
    if (!run.isEmpty() && coincident(run.back(), p))
        return;
    if (remaining == 0)
        throw std::length_error("Reference spline subdivision exceeds the supported point count");
    --remaining;
    run.append(p);
}

void flatten(const Cubic2d& c, QPolygonF& run, int depth, qsizetype& remaining,
             const std::atomic_bool* cancel) {
    if (cancelled(cancel))
        return;
    // The control polygon bounds the Bezier. Its distance to the finite chord
    // bounds the deviation even for cubics that turn back past an endpoint.
    if (distanceToChord(c[1], c[0], c[3]) <= referenceFlattenToleranceMm &&
        distanceToChord(c[2], c[0], c[3]) <= referenceFlattenToleranceMm) {
        appendReferencePoint(run, c[3], remaining);
        return;
    }
    if (depth == 32)
        throw std::runtime_error("Reference spline flattening precision exhausted");
    const auto a=(c[0]+c[1])*0.5, b=(c[1]+c[2])*0.5, d=(c[2]+c[3])*0.5;
    const auto e=(a+b)*0.5, f=(b+d)*0.5, g=(e+f)*0.5;
    flatten({c[0], a, e, g}, run, depth+1, remaining, cancel);
    flatten({g, f, d, c[3]}, run, depth+1, remaining, cancel);
}

struct PositionedMarker {
    double xMm;
    const SectionSpline* spline;
};

bool validInNativeSpline(Vec3 p, double mmPerUnit) {
    for (double v : {p.x, p.y, p.z}) {
        if (!std::isfinite(v * mmPerUnit) || !std::isfinite(float(v)))
            return false;
    }
    return true;
}

QVector<QPolygonF> flattenedMarker(const PositionedMarker& marker, double mmPerUnit, qsizetype& remaining,
                                   const std::atomic_bool* cancel) {
    const auto cubics = presentation::splineCubics(*marker.spline);
    QVector<QPolygonF> lines;
    QPolygonF run;
    for (const auto& c : cubics) {
        if (cancelled(cancel))
            return {};
        Cubic2d yz;
        for (size_t i=0; i<c.size(); ++i) {
            const auto p = c[i];
            if (!validInNativeSpline(p, mmPerUnit) ||
                std::abs(p.x * mmPerUnit-marker.xMm) > stationMatchToleranceMm)
                return {};
            yz[i] = {p.y * mmPerUnit, p.z * mmPerUnit};
        }
        if (!run.isEmpty() && !coincident(run.back(), yz[0]))
            finishRun(lines, run);
        appendReferencePoint(run, yz[0], remaining);
        flatten(yz, run, 0, remaining, cancel);
    }
    finishRun(lines, run);
    return lines;
}
} // namespace

QVector<StationReference> stationReferences(const QVector<ReferenceMarker>& markers,
                                           double mmPerUnit, const std::atomic_bool* cancel) {
    if (!std::isfinite(mmPerUnit) || mmPerUnit <= 0)
        return {};
    QVector<PositionedMarker> ordered;
    for (const auto& marker : markers) {
        if (cancelled(cancel))
            return {};
        const auto& spline = marker.spline;
        if (!marker.visible || spline.points.size()<2 || spline.knuckles.size()!=spline.points.size())
            continue;
        double lo = std::numeric_limits<double>::infinity(), hi = -lo;
        for (const auto p : spline.points) {
            if (cancelled(cancel))
                return {};
            if (!validInNativeSpline(p, mmPerUnit)) {
                lo = std::numeric_limits<double>::quiet_NaN();
                break;
            }
            lo = std::min(lo, p.x * mmPerUnit);
            hi = std::max(hi, p.x * mmPerUnit);
            if (hi-lo > stationMatchToleranceMm)
                break;
        }
        if (hi-lo <= stationMatchToleranceMm)
            ordered.append({lo+(hi-lo)*0.5, &spline});
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.xMm < b.xMm;
    });
    QVector<StationReference> result;
    qsizetype remaining = 1'000'000;
    for (const auto& marker : ordered) {
        if (cancelled(cancel))
            return {};
        auto lines = flattenedMarker(marker, mmPerUnit, remaining, cancel);
        if (lines.isEmpty())
            continue;
        // Select the nearest actual model station before grouping near (but
        // different) marker planes. Only coincide truly equal planes here.
        if (result.isEmpty() || marker.xMm-result.back().xMm > 1e-9)
            result.append({marker.xMm, {}, ReferenceSide::Both});
        result.back().linesMm.append(std::move(lines));
    }
    for (auto& group : result) {
        bool hasPositive = false, hasNegative = false;
        for (const auto& line : group.linesMm) {
            for (auto p : line) {
                if (cancelled(cancel))
                    return {};
                hasPositive |= p.x() > referenceFlattenToleranceMm;
                hasNegative |= p.x() < -referenceFlattenToleranceMm;
            }
        }
        if (hasPositive != hasNegative)
            group.side = hasPositive ? ReferenceSide::Positive : ReferenceSide::Negative;
    }
    return result;
}

QVector<QPolygonF> clipStationToReferenceSide(const QVector<QPolygonF>& modelMm,
                                              ReferenceSide side, const std::atomic_bool* cancel) {
    if (side == ReferenceSide::Both)
        return cancelled(cancel) ? QVector<QPolygonF>{} : modelMm;
    QVector<QPolygonF> result;
    const auto inside = [side](QPointF p){
        return side == ReferenceSide::Positive ? p.x() >= 0 : p.x() <= 0;
    };
    for (const auto& line : modelMm) {
        QPolygonF run;
        for (qsizetype i=1; i<line.size(); ++i) {
            if (cancelled(cancel))
                return {};
            const auto a=line[i-1], b=line[i];
            const bool keepA=inside(a), keepB=inside(b);
            if (keepA)
                appendPoint(run, a);
            if (keepA != keepB) {
                const double fraction = a.x() / (a.x()-b.x());
                appendPoint(run, {0, a.y()+(b.y()-a.y())*fraction});
            }
            if (keepB)
                appendPoint(run, b);
            else
                finishRun(result, run);
        }
        finishRun(result, run);
    }
    return result;
}
} // namespace fs
