/*
 * Adapted from ShipCAD, copyright 2015 Greg Green <ggreen@bit-builder.com>.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Current Plus source also credits Timoshenko V.F. (2006-2011) and the
 * FPC/Lazarus conversion by Mark Malakanov (2015).
 * Source: gpgreen/ShipCAD, commit 4d89e1498cbcea7b2c0c6b26aa7c8964b42279cd,
 * ShipCADlib/plane.h and spline.h. Current Plus behavior is identified in
 * PROVENANCE.md in this directory. FreeShip_26 adaptation, 2026.
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
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace fs::native_geometry {

// The legacy geometric storage type is Single, not the UI snapshot's double.
struct Vec3 { float x = 0, y = 0, z = 0; };

enum class LineKind { Invalid, Parallel, Coplanar, Point };
struct LineIntersection {
    LineKind kind = LineKind::Invalid;
    Vec3 point{};
    float parameter = 0;
};

class Plane {
public:
    Plane() = default;
    Plane(float a, float b, float c, float d);
    static Plane fromPointNormal(Vec3 point, Vec3 normal);
    static Plane fromPoints(Vec3 first, Vec3 second, Vec3 third);
    bool valid() const noexcept;
    const std::array<float, 4>& coefficients() const noexcept { return coefficients_; }
    // Algebraic plane value a*x+b*y+c*z+d, as used by the original kernel.
    // It is a physical signed distance only when the normal has unit length.
    float evaluate(Vec3 point) const;
    std::optional<Vec3> project(Vec3 point) const;
    std::optional<Vec3> mirror(Vec3 point) const;
    bool intersectsBox(Vec3 minimum, Vec3 maximum) const;
    LineIntersection intersectLine(Vec3 point, Vec3 direction,
                                   float distanceTolerance = 1e-5f) const;
private:
    std::array<float, 4> coefficients_{};
};

struct IntersectionData {
    std::vector<Vec3> points;
    std::vector<float> parameters;
};

// A calculation-only candidate. It is not connected to the editing model,
// renderer, FBM writer or DXF exporter. Mutating points/knuckles invalidates
// the derived arrays; read operations build them once on demand.
class Spline {
public:
    explicit Spline(std::vector<Vec3> points = {}, std::vector<bool> knuckles = {},
                    std::size_t fragments = 0);
    const std::vector<Vec3>& points() const noexcept { return points_; }
    const std::vector<bool>& knuckles() const noexcept { return knuckles_; }
    const std::vector<float>& parameters() const;
    const std::vector<Vec3>& secondDerivatives() const;
    float totalChordLength() const;
    std::size_t fragments() const noexcept { return fragments_; }
    void setPoint(std::size_t index, Vec3 point);
    void setKnuckle(std::size_t index, bool knuckle);
    void setFragments(std::size_t fragments);
    bool built() const noexcept { return built_; }
    std::size_t rebuildCount() const noexcept { return rebuildCount_; }
    // FPC 3.2.2 Win64 uses an 8-byte Extended. Keep that parameter precision
    // here; MinGW's wider long double would change intermediate rounding.
    Vec3 value(double parameter) const;
    // Keep the inherited finite-difference first derivative, including its
    // one-sided endpoint behavior; it is not an analytic cubic derivative.
    Vec3 firstDerivative(float parameter) const;
    Vec3 secondDerivative(float parameter) const;
    bool intersectPlane(const Plane& plane, IntersectionData& output) const;
    // Explicit opt-in weighted removal. Initializes its own length, preserves
    // endpoints/knuckles and leaves the object intact on failure. False means
    // a zero-length curve with at least three points.
    bool simplify(float criterion);
private:
    void rebuild() const;
    void invalidate() noexcept { built_ = false; }
    std::pair<std::size_t, std::size_t> span(double parameter) const;
    std::vector<Vec3> points_;
    std::vector<bool> knuckles_;
    std::size_t fragments_ = 0;
    mutable bool built_ = false;
    mutable std::size_t rebuildCount_ = 0;
    mutable std::vector<float> parameters_;
    mutable std::vector<Vec3> derivatives_;
    mutable float totalLength_ = 0;
};

} // namespace fs::native_geometry
