/*
 * Derived from current FREE!ship Plus GetValues, DrawSpline and AddSpline.
 * Original FREE!ship copyright 2005 Martijn van Engeland; Plus contributions
 * by Timoshenko V.F. and Mark Malakanov. See LINESPLAN-PROVENANCE.md.
 * GNU General Public License, version 2 or (at your option) any later version.
 * WITHOUT ANY WARRANTY. See ../../COPYING for the complete license.
 */
#include "linesplan.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs::native_geometry {
namespace {
float finite(float value) {
    if (!std::isfinite(value)) throw std::overflow_error("Lines-plan coordinate is not finite");
    return value;
}
}

CurvePath samplePolyline(const Spline& spline, bool preserveAllKnuckles) {
    const auto& points = spline.points();
    if (points.size() < 2) return {};
    const auto count = spline.fragments();
    if (count < 2) throw std::invalid_argument("Polyline requires at least two samples");
    const auto& knots = spline.parameters();
    CurvePath result;
    result.reserve(count + points.size());
    std::size_t lo = 0, lastKnuckle = 0, nextKnuckle = 1;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = double(i) / double(count - 1);
        // Increasing sample parameters allow a forward walk instead of a second
        // binary search. Equality stays in the preceding interval, as in Value.
        while (lo + 2 < knots.size() && knots[lo + 1] < t) ++lo;
        const auto point = spline.value(t);
        if (preserveAllKnuckles) {
            while (nextKnuckle <= lo) {
                if (spline.knuckles()[nextKnuckle]) result.push_back(points[nextKnuckle]);
                ++nextKnuckle;
            }
        } else if (lastKnuckle != lo && spline.knuckles()[lo]) {
            lastKnuckle = lo;
            result.push_back(points[lo]);
        }
        result.push_back(point);
    }
    return result;
}

std::vector<CurvePath> linesplanCurve(const Spline& spline, LinesplanView view,
                                    const LinesplanPlacement& placement, CurvePurpose purpose) {
    if (view < LinesplanView::Profile || view > LinesplanView::Plan ||
        purpose < CurvePurpose::Display || purpose > CurvePurpose::DXF)
        throw std::invalid_argument("Unknown lines-plan projection");
    const auto origin = placement.origin;
    finite(origin.x); finite(origin.y); finite(origin.z); finite(placement.splitLocation);
    if (spline.points().size() < 2) return {};
    if (purpose == CurvePurpose::Display) {
        const auto bounds = std::minmax_element(spline.points().begin(), spline.points().end(),
            [](Vec3 a, Vec3 b) { return a.x < b.x; });
        if ((view == LinesplanView::AftBody && bounds.second->x > placement.splitLocation) ||
            (view == LinesplanView::FrontBody && bounds.first->x < placement.splitLocation)) return {};
    }
    // Export's density is local; the old AddSpline mutated the live spline.
    const auto samples = purpose == CurvePurpose::DXF
        ? samplePolyline(Spline(spline.points(), spline.knuckles(), 100)) : samplePolyline(spline);
    std::vector<CurvePath> result;
    auto append = [&](int sign) {
        CurvePath path;
        path.reserve(samples.size());
        for (const auto p : samples) {
            Vec3 q;
            if (view == LinesplanView::Profile) q = {finite(origin.x + p.x), finite(origin.y + p.z), 0};
            else if (view == LinesplanView::Plan) q = {finite(origin.x + p.x), finite(origin.y + sign * p.y), 0};
            else q = {finite(origin.x + sign * p.y), finite(origin.y + p.z), 0};
            path.push_back(q);
        }
        result.push_back(std::move(path));
    };
    if (view == LinesplanView::AftBody || view == LinesplanView::FrontBody) {
        append(-1); append(1);
    } else {
        append(1);
        if (view == LinesplanView::Plan && placement.mirrorPlan && !placement.hasDiagonals) append(-1);
    }
    return result;
}

} // namespace fs::native_geometry
