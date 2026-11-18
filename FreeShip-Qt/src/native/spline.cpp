/*
 * Adapted from ShipCAD, copyright 2015 Greg Green <ggreen@bit-builder.com>.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Current Plus source also credits Timoshenko V.F. (2006-2011) and the
 * FPC/Lazarus conversion by Mark Malakanov (2015).
 * gpgreen/ShipCAD commit 4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd,
 * ShipCADlib/spline.cpp; reconciled with current Plus Units/FreeSpline.inc.
 * See PROVENANCE.md. FreeShip_26 adaptation, 2026.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */
#include "geometry.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fs::native_geometry {
namespace {
constexpr std::size_t maximumItems = 1000000;
void requireFinite(Vec3 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("Spline point must be finite");
}
void requireParameter(double t) {
    if (!std::isfinite(t)) throw std::invalid_argument("Spline parameter must be finite");
}
float checked(float value) {
    if (!std::isfinite(value)) throw std::overflow_error("Spline result is not finite");
    return value;
}
Vec3 checked(Vec3 point) { checked(point.x); checked(point.y); checked(point.z); return point; }
float coordinate(const Vec3& point, unsigned axis) {
    return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
}
float& coordinate(Vec3& point, unsigned axis) {
    return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
}
float distance(Vec3 a, Vec3 b) {
    const float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
    return checked(std::sqrt(x * x + y * y + z * z));
}
}

Spline::Spline(std::vector<Vec3> points, std::vector<bool> knuckles, std::size_t fragments)
    : points_(std::move(points)), knuckles_(std::move(knuckles)), fragments_(fragments) {
    if (points_.size() > maximumItems || fragments > maximumItems)
        throw std::length_error("Spline input exceeds the supported count");
    for (const auto& point : points_) requireFinite(point);
    if (knuckles_.empty()) knuckles_.resize(points_.size(), false);
    if (knuckles_.size() != points_.size()) throw std::invalid_argument("Knuckle count does not match points");
}

void Spline::setPoint(std::size_t index, Vec3 point) {
    requireFinite(point);
    const auto old = points_.at(index);
    if (old.x == point.x && old.y == point.y && old.z == point.z) return;
    points_[index] = point; invalidate();
}

void Spline::setKnuckle(std::size_t index, bool knuckle) {
    if (knuckles_.at(index) == knuckle) return;
    knuckles_[index] = knuckle; invalidate();
}

void Spline::setFragments(std::size_t fragments) {
    if (fragments > maximumItems) throw std::length_error("Fragment count exceeds the supported count");
    // Sampling density is not an input to the cubic coefficient system.
    fragments_ = fragments;
}

void Spline::rebuild() const {
    if (built_) return;
    const auto count = points_.size();
    std::vector<float> parameters(count, 0);
    std::vector<Vec3> derivatives(count), u(count);
    float length = 0;
    for (std::size_t i = 1; i < count; ++i) length = checked(length + distance(points_[i - 1], points_[i]));
    if (count > 1) {
        if (std::abs(length) < 1e-5) {
            for (std::size_t i = 0; i < count; ++i) parameters[i] = float(double(i) / double(count - 1));
        } else {
            float accumulated = 0;
            for (std::size_t i = 1; i + 1 < count; ++i) {
                accumulated += distance(points_[i - 1], points_[i]);
                parameters[i] = accumulated / length;
            }
            parameters[count - 1] = 1;
        }
        for (std::size_t i = 1; i + 1 < count; ++i) {
            if (knuckles_[i]) continue;
            const float d1 = parameters[i + 1] - parameters[i - 1];
            const float d2 = parameters[i] - parameters[i - 1];
            const float d3 = parameters[i + 1] - parameters[i];
            if (std::abs(d1) < 1e-5 || std::abs(d2) < 1e-5 || std::abs(d3) < 1e-5) continue;
            const float sig = d2 / d1;
            for (unsigned axis = 0; axis < 3; ++axis) {
                // FPC evaluates these exact integer-valued real constants in
                // Single arithmetic. Unsuffixed C++ doubles alter U and the
                // back-substituted derivatives by several float ULPs.
                const float p = checked(sig * coordinate(derivatives[i - 1], axis) + 2.0f);
                coordinate(derivatives[i], axis) = checked((sig - 1.0f) / p);
                coordinate(u[i], axis) = checked((6.0f * (
                    (coordinate(points_[i + 1], axis) - coordinate(points_[i], axis)) / d3 -
                    (coordinate(points_[i], axis) - coordinate(points_[i - 1], axis)) / d2) / d1 -
                    sig * coordinate(u[i - 1], axis)) / p);
            }
        }
        // Natural endpoints have zero second derivatives. A knuckle resets
        // its derivative during back substitution in the current Plus kernel.
        for (std::size_t i = count - 1; i > 0; --i) {
            if (knuckles_[i - 1]) { derivatives[i - 1] = {}; continue; }
            for (unsigned axis = 0; axis < 3; ++axis)
                coordinate(derivatives[i - 1], axis) = checked(
                    coordinate(derivatives[i - 1], axis) * coordinate(derivatives[i], axis) + coordinate(u[i - 1], axis));
        }
    }
    parameters_ = std::move(parameters); derivatives_ = std::move(derivatives);
    totalLength_ = length; built_ = true; ++rebuildCount_;
}

const std::vector<float>& Spline::parameters() const { rebuild(); return parameters_; }
const std::vector<Vec3>& Spline::secondDerivatives() const { rebuild(); return derivatives_; }
float Spline::totalChordLength() const { rebuild(); return totalLength_; }

std::pair<std::size_t, std::size_t> Spline::span(double t) const {
    std::size_t lo = 0, hi = points_.size() - 1;
    while (hi - lo > 1) {
        const auto middle = (lo + hi) / 2;
        if (parameters_[middle] < t) lo = middle; else hi = middle;
    }
    return {lo, hi};
}

Vec3 Spline::value(double t) const {
    requireParameter(t); rebuild();
    if (points_.size() < 2) return {};
    const auto [lo, hi] = span(t);
    const float h = parameters_[hi] - parameters_[lo];
    if (std::abs(h) < 1e-6) return points_[hi];
    const float a = checked(float((parameters_[hi] - t) / h));
    const float b = checked(float((t - parameters_[lo]) / h));
    const auto& p1 = points_[lo]; const auto& p2 = points_[hi];
    if (knuckles_[lo] && knuckles_[hi])
        return checked({a * p1.x + b * p2.x, a * p1.y + b * p2.y, a * p1.z + b * p2.z});
    const float a3 = a * a * a - a, b3 = b * b * b - b;
    const float h2 = h * h / 6.0f;
    const auto& d1 = derivatives_[lo]; const auto& d2 = derivatives_[hi];
    return checked({a * p1.x + b * p2.x + (a3 * d1.x + b3 * d2.x) * h2,
                    a * p1.y + b * p2.y + (a3 * d1.y + b3 * d2.y) * h2,
                    a * p1.z + b * p2.z + (a3 * d1.z + b3 * d2.z) * h2});
}

Vec3 Spline::firstDerivative(float t) const {
    requireParameter(t);
    // In contrast to the exactly representable constants in Rebuild, FPC
    // promotes these 1e-3 operations to Double before assigning to Single.
    float t1 = float(t - 1e-3), t2 = float(t + 1e-3);
    if (t1 < 0) t1 = 0;
    if (t2 > 1) t2 = 1;
    const auto p1 = value(t1), p2 = value(t2);
    const float step = t2 - t1;
    if (step == 0) throw std::domain_error("Finite difference interval has zero length");
    return checked({(p2.x - p1.x) / step, (p2.y - p1.y) / step, (p2.z - p1.z) / step});
}

Vec3 Spline::secondDerivative(float t) const {
    requireParameter(t); rebuild();
    if (points_.size() < 2) return {};
    const auto [lo, hi] = span(t);
    const float h = parameters_[hi] - parameters_[lo];
    const float fraction = h <= 0 ? 0.5f : (t - parameters_[lo]) / h;
    const auto& d1 = derivatives_[lo]; const auto& d2 = derivatives_[hi];
    return checked({d1.x + fraction * (d2.x - d1.x), d1.y + fraction * (d2.y - d1.y), d1.z + fraction * (d2.z - d1.z)});
}

bool Spline::intersectPlane(const Plane& plane, IntersectionData& output) const {
    output.points.clear(); output.parameters.clear();
    if (!plane.valid()) throw std::domain_error("Cannot intersect an invalid plane");
    rebuild();
    if (points_.size() < 2) return false;
    if (fragments_ == 0)
        throw std::domain_error("Plane intersection requires at least one sampling fragment");
    // Assemble a temporary result so a rejected numeric overflow cannot expose
    // a partially computed crossing list to its caller.
    IntersectionData next;
    auto append = [&next](Vec3 point, float t) { next.points.push_back(point); next.parameters.push_back(t); };
    float t1 = 0;
    auto p1 = value(t1); float s1 = plane.evaluate(p1);
    if (std::abs(s1) < 1e-6) append(p1, t1);
    for (std::size_t i = 1; i <= fragments_; ++i) {
        const float t2 = float(double(i) / double(fragments_));
        const auto p2 = value(t2); const float s2 = plane.evaluate(p2);
        if (std::abs(s2) < 1e-6) append(p2, t2);
        if ((s1 < 0 && s2 > 0) || (s2 < 0 && s1 > 0)) {
            float t = -s1 / (s2 - s1);
            t = t1 + t * (t2 - t1);
            append(value(t), t);
        }
        t1 = t2; s1 = s2;
    }
    output = std::move(next);
    return !output.points.empty();
}

bool Spline::simplify(float criterion) {
    if (!std::isfinite(criterion) || criterion < 0)
        throw std::invalid_argument("Simplification criterion must be finite and nonnegative");
    if (points_.size() < 3) return true;
    // Current Plus reads a lazily populated FTotalLength. Do not let prior
    // viewing/sampling choose the result, or a failed operation change input.
    Spline next = *this;
    // Only the chord sum is needed. Do not solve cubic coefficients that will
    // immediately be invalidated by removing a point.
    float length = next.totalLength_;
    if (!next.built_) {
        length = 0;
        for (std::size_t i = 1; i < next.points_.size(); ++i)
            length = checked(length + distance(next.points_[i - 1], next.points_[i]));
    }
    const float lengthSquared = checked(length * length);
    if (lengthSquared == 0) return false;
    auto weight = [&](std::size_t i) -> float {
        if (i == 0 || i + 1 == next.points_.size() || next.knuckles_[i])
            return std::numeric_limits<float>::infinity();
        const auto a = next.points_[i - 1], p = next.points_[i], b = next.points_[i + 1];
        const float spanLength = distance(a, b);
        if (spanLength < 1e-5) return 0;
        const Vec3 v{a.x-b.x,a.y-b.y,a.z-b.z}, u{p.x-a.x,p.y-a.y,p.z-a.z};
        const Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const float deviation = checked(std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z)/spanLength);
        float w;
        if (deviation < 1e-2 && spanLength * spanLength / lengthSquared > .01) w = 1e10f;
        else w = checked(1e8f * deviation * deviation * spanLength);
        return checked(w / lengthSquared);
    };
    std::vector<float> weights(next.points_.size());
    for (std::size_t i=0;i<weights.size();++i) weights[i]=weight(i);
    bool changed=false;
    while (next.points_.size()>=3) {
        std::size_t index=1;
        for (std::size_t i=2;i+1<weights.size() && weights[index]>0;++i)
            if (weights[i]<weights[index]) index=i;
        if (!(weights[index]<criterion)) break;
        next.points_.erase(next.points_.begin()+index);
        next.knuckles_.erase(next.knuckles_.begin()+index);
        weights.erase(weights.begin()+index);
        for (std::size_t i=index-1;i<weights.size() && i<=index+1;++i) weights[i]=weight(i);
        changed=true;
    }
    if (changed) { next.invalidate(); *this=std::move(next); }
    return true;
}

} // namespace fs::native_geometry
