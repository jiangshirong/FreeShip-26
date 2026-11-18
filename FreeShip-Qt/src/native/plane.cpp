/*
 * Adapted from ShipCAD, copyright 2015 Greg Green <ggreen@bit-builder.com>.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Current Plus source also credits Timoshenko V.F. (2006-2011) and the
 * FPC/Lazarus conversion by Mark Malakanov (2015).
 * gpgreen/ShipCAD commit 4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd,
 * ShipCADlib/plane.cpp; see PROVENANCE.md. FreeShip_26 adaptation, 2026.
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
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace fs::native_geometry {
namespace {
void requireFinite(Vec3 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("Plane input must be finite");
}
float result(float value) {
    if (!std::isfinite(value)) throw std::overflow_error("Plane result is not finite");
    return value;
}
Vec3 result(Vec3 value) {
    result(value.x); result(value.y); result(value.z); return value;
}
float normalSquared(const std::array<float, 4>& c) {
    return c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
}
double lineSum(std::initializer_list<double> terms) {
    // Products of two Single inputs fit exactly in Double, but a naive sum
    // can lose a tiny term between cancelling large ones (1 + tiny - 1).
    double sum = 0, correction = 0;
    for (double term : terms) {
        const double next = sum + term;
        correction += std::abs(sum) >= std::abs(term) ? (sum - next) + term : (term - next) + sum;
        sum = next;
    }
    return sum + correction;
}
float lineResult(double value) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        throw std::overflow_error("Line intersection result exceeds Single range");
    const float narrowed = static_cast<float>(value);
    if (value != 0 && narrowed == 0)
        throw std::underflow_error("Line intersection result is too small for Single");
    return narrowed;
}
}

Plane::Plane(float a, float b, float c, float d) : coefficients_{a, b, c, d} {
    for (float value : coefficients_) if (!std::isfinite(value))
        throw std::invalid_argument("Plane coefficients must be finite");
}

Plane Plane::fromPointNormal(Vec3 p, Vec3 n) {
    requireFinite(p); requireFinite(n);
    return Plane(n.x, n.y, n.z, result(-p.x * n.x - p.y * n.y - p.z * n.z));
}

Plane Plane::fromPoints(Vec3 p1, Vec3 p2, Vec3 p3) {
    requireFinite(p1); requireFinite(p2); requireFinite(p3);
    const Vec3 n = result({
        (p2.y - p1.y) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.y - p1.y),
        (p2.z - p1.z) * (p3.x - p1.x) - (p2.x - p1.x) * (p3.z - p1.z),
        (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x)});
    const float length = result(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z));
    // The old port normalizes and divides by the length a second time. A
    // collinear triple must never become NaN coefficients or a usable plane.
    if (length == 0) return {};
    return fromPointNormal(p1, {n.x / length, n.y / length, n.z / length});
}

bool Plane::valid() const noexcept {
    const float q = normalSquared(coefficients_);
    return std::isfinite(q) && q > 0;
}

float Plane::evaluate(Vec3 p) const {
    requireFinite(p);
    if (!valid()) throw std::domain_error("Plane normal is zero or not representable");
    const auto& c = coefficients_;
    return result(c[0] * p.x + c[1] * p.y + c[2] * p.z + c[3]);
}

std::optional<Vec3> Plane::project(Vec3 p) const {
    requireFinite(p);
    if (!valid()) return std::nullopt;
    const auto& c = coefficients_;
    const float q = normalSquared(c);
    const float r = result(evaluate(p) / q);
    return result({p.x - c[0] * r, p.y - c[1] * r, p.z - c[2] * r});
}

std::optional<Vec3> Plane::mirror(Vec3 p) const {
    const auto projection = project(p);
    if (!projection) return std::nullopt;
    return result({p.x + 2 * (projection->x - p.x), p.y + 2 * (projection->y - p.y),
                   p.z + 2 * (projection->z - p.z)});
}

bool Plane::intersectsBox(Vec3 minimum, Vec3 maximum) const {
    requireFinite(minimum); requireFinite(maximum);
    if (minimum.x > maximum.x || minimum.y > maximum.y || minimum.z > maximum.z)
        throw std::invalid_argument("Box bounds are reversed");
    float low = evaluate(minimum), high = low;
    for (unsigned corner = 1; corner < 8; ++corner) {
        const Vec3 point{corner & 1 ? maximum.x : minimum.x,
                         corner & 2 ? maximum.y : minimum.y,
                         corner & 4 ? maximum.z : minimum.z};
        const float value = evaluate(point);
        if (value < low) low = value;
        if (value > high) high = value;
    }
    return low <= 0 && high >= 0;
}

LineIntersection Plane::intersectLine(Vec3 p, Vec3 direction, float tolerance) const {
    requireFinite(p); requireFinite(direction);
    if (!std::isfinite(tolerance) || tolerance < 0)
        throw std::invalid_argument("Line distance tolerance must be finite and nonnegative");
    if (!valid() || (direction.x == 0 && direction.y == 0 && direction.z == 0)) return {};
    const auto& c = coefficients_;
    // This new API has no Pascal compatibility implementation. Keep its
    // classification independent of Single-product underflow: a tiny nonzero
    // dot product is still an intersecting line, not a parallel one.
    const double dot = lineSum({double(direction.x) * c[0], double(direction.y) * c[1], double(direction.z) * c[2]});
    const double distance = lineSum({double(c[0]) * p.x, double(c[1]) * p.y, double(c[2]) * p.z, c[3]});
    if (dot == 0) {
        // Absolute physical distance makes this test symmetric about the plane
        // and independent of a positive scaling of its coefficients.
        const double q = lineSum({double(c[0]) * c[0], double(c[1]) * c[1], double(c[2]) * c[2]});
        const double signedDistance = distance / std::sqrt(q);
        return {std::abs(signedDistance) <= tolerance ? LineKind::Coplanar : LineKind::Parallel, {}, 0};
    }
    const double parameter = -distance / dot;
    // Derive the point before narrowing the parameter, otherwise its rounding
    // error is amplified by the direction length.
    const Vec3 point{lineResult(double(p.x) + parameter * direction.x),
                     lineResult(double(p.y) + parameter * direction.y),
                     lineResult(double(p.z) + parameter * direction.z)};
    return {LineKind::Point, point, lineResult(parameter)};
}

} // namespace fs::native_geometry
