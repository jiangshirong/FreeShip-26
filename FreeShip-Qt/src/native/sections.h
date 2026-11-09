/*
 * Calculation-only adaptation of current FREE!ship Plus section rules.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Plus credits Timoshenko V.F. (2006-2011) and Mark Malakanov (2015).
 * See SECTIONS-PROVENANCE.md for sources, scope and intentional safety changes.
 *
 * This program is free software under the GNU General Public License,
 * version 2 or (at your option) any later version, WITHOUT ANY WARRANTY,
 * including MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See ../../COPYING for the complete license.
 */
#pragma once
#include "subdivision.h"
#include <cstdint>
#include <optional>

namespace fs::native_geometry {

struct SectionEndpoint {
    Vec3 position{};
    bool knuckle = false;
    std::size_t edge = 0;                   // SubdivisionLevel edge ID.
    std::optional<std::size_t> vertex;     // Present when using a mesh vertex.
};
struct SectionSegment {
    SectionEndpoint start, end;
    std::size_t face = 0;                   // SubdivisionLevel face ID.
    std::size_t rootControlFace = 0;
};
struct SectionCurve {
    std::vector<Vec3> points;
    std::vector<bool> knuckles;
    // Signed one-based raw segment IDs, in traversal order. A negative ID
    // means that segment was traversed backwards. Empty for direct join input.
    std::vector<std::int64_t> sourceSegments;
    std::size_t fragments = 0;
    bool closed = false;                    // Legacy endpoint distance < 1e-5.
};
struct SectionOptions {
    bool hydrostaticsLayersOnly = false;
};
struct SectionResult {
    bool found = false;
    bool surfaceBoxIntersects = false;
    std::vector<std::size_t> selectedControlFaces;
    std::vector<SectionSegment> segments;
    std::vector<SectionCurve> connectedCurves;
    std::vector<SectionCurve> joinedCurves;   // Before conditional tiny filtering.
    std::vector<SectionCurve> curves;
    std::size_t nearDuplicateEndpointsRemoved = 0;
    std::size_t joinCount = 0;
    std::size_t discardedTinyCurves = 0;
};

// Reuse the checked adjacency and per-root bounds for an immutable revision.
// The input and mesh must outlive the index and must not be modified meanwhile.
class SectionIndex {
public:
    SectionIndex(const GeometryInput& input, const SubdivisionLevel& mesh);
    ~SectionIndex();
    SectionIndex(const SectionIndex&) = delete;
    SectionIndex& operator=(const SectionIndex&) = delete;
    SectionResult intersect(const Plane& plane, SectionOptions options = {}) const;
    // Exact ordered roots used by intersect, including its global-bound filter.
    std::vector<std::size_t> selectedRoots(const Plane& plane, SectionOptions options = {}) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Current Surface.IntersectPlane route: control bounds, layer flags/order,
// root-face child bounds, then raw cuts and the inherited two join stages.
// Level 0 intentionally has no child faces and produces no section curves.
// Input and mesh must describe the same static document revision.
// Output overloads replace the entire result on success (including an empty
// result), and leave the previous output unchanged on any exception.
void intersectSections(const GeometryInput& input, const SubdivisionLevel& mesh,
                       const Plane& plane, SectionOptions options,
                       SectionResult& output);
SectionResult intersectSections(const GeometryInput& input, const SubdivisionLevel& mesh,
                                const Plane& plane, SectionOptions options = {});

// CalculateIntersections route: process these root faces in the supplied order,
// with no layer/global-box filtering. Empty selection means no faces.
void calculateSections(const SubdivisionLevel& mesh, const Plane& plane,
                       const std::vector<std::size_t>& rootControlFaces,
                       SectionResult& output);

// JoinSplineSegments threshold/order compatibility, with the documented fix
// that both matched endpoint knuckles survive prepend as well as append.
// The threshold is SQUARED distance. No Surface conditional tiny filtering.
// Validation failure leaves the supplied curves unchanged.
// closed is recomputed from coordinates; fragments is retained for an unchanged
// curve and raised to at least new point count minus one after a merge.
std::size_t joinSectionCurves(std::vector<SectionCurve>& curves,
                             float joinSquaredError = .01f,
                             bool forceToOneSegment = false);

} // namespace fs::native_geometry
