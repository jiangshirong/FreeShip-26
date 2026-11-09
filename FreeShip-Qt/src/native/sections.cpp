/*
 * Calculation-only adaptation of current FREE!ship Plus section rules.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Plus credits Timoshenko V.F. (2006-2011) and Mark Malakanov (2015).
 * See SECTIONS-PROVENANCE.md for sources and intentional safety changes.
 * GPL version 2 or (at your option) any later version, WITHOUT ANY WARRANTY,
 * including MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See ../../COPYING for the complete license.
 */
#include "sections.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace fs::native_geometry {
namespace {
using Index = std::size_t;
using Key = std::pair<Index, Index>;
constexpr Index maximumItems = 2000000;
Key edgeKey(Index a, Index b) {
    return std::minmax(a, b);
}
void checkCount(Index n) {
    if (n > maximumItems)
        throw std::length_error("Section input/result exceeds the supported count");
}
void finite(Vec3 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("Section coordinates must be finite");
}
float number(float x) {
    if (!std::isfinite(x))
        throw std::overflow_error("Section result is not finite");
    return x;
}
Vec3 number(Vec3 p) {
    number(p.x);
    number(p.y);
    number(p.z);
    return p;
}
float squaredDistance(Vec3 a, Vec3 b) {
    const float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
    return number(x * x + y * y + z * z);
}
float distance(Vec3 a, Vec3 b) {
    return number(std::sqrt(squaredDistance(a, b)));
}
bool closed(const SectionCurve &curve) {
    return curve.points.size() > 1 && distance(curve.points.front(), curve.points.back()) < 1e-5;
}
struct Bounds {
    Vec3 minimum{}, maximum{};
    bool empty = true;
    void add(Vec3 p) {
        if (empty) {
            minimum = maximum = p;
            empty = false;
            return;
        }
        minimum.x = std::min(minimum.x, p.x);
        maximum.x = std::max(maximum.x, p.x);
        minimum.y = std::min(minimum.y, p.y);
        maximum.y = std::max(maximum.y, p.y);
        minimum.z = std::min(minimum.z, p.z);
        maximum.z = std::max(maximum.z, p.z);
    }
};

struct MeshView {
    explicit MeshView(const SubdivisionLevel &value) : mesh(value) {
        if (mesh.level > 4)
            throw std::invalid_argument("Section mesh level is out of range");
        checkCount(mesh.points.size());
        checkCount(mesh.edges.size());
        checkCount(mesh.faces.size());
        checkCount(mesh.layers.size());
        for (const auto &p : mesh.points) {
            finite(p.position);
            if (int(p.type) < 0 || int(p.type) > 3)
                throw std::invalid_argument("Unknown section mesh vertex type");
        }
        std::vector<std::vector<Index>> expected(mesh.edges.size());
        for (Index i = 0; i < mesh.edges.size(); ++i) {
            const auto &edge = mesh.edges[i];
            if (edge.points[0] >= mesh.points.size() || edge.points[1] >= mesh.points.size() ||
                edge.points[0] == edge.points[1])
                throw std::invalid_argument("Section mesh edge point index is invalid");
            if (!edges.emplace(edgeKey(edge.points[0], edge.points[1]), i).second)
                throw std::invalid_argument("Duplicate section mesh edge");
        }
        for (Index i = 0; i < mesh.faces.size(); ++i) {
            const auto &face = mesh.faces[i];
            if (face.points.size() < 3 || face.points.size() > 4096 || face.rootControlFace >= maximumItems ||
                face.layer >= mesh.layers.size())
                throw std::invalid_argument("Section face shape/root/layer is invalid");
            auto &root = roots[face.rootControlFace];
            if ((mesh.level == 0 && face.childIndex != -1) ||
                (mesh.level > 0 && face.childIndex != int(root.size())))
                throw std::invalid_argument("Section child order does not match its root face");
            root.push_back(i);
            std::set<Index> unique;
            for (auto p : face.points) {
                if (p >= mesh.points.size() || !unique.insert(p).second)
                    throw std::invalid_argument("Section face contains an invalid or repeated point");
            }
            for (Index j = 0; j < face.points.size(); ++j) {
                const auto found =
                    edges.find(edgeKey(face.points[j], face.points[(j + 1) % face.points.size()]));
                if (found == edges.end())
                    throw std::invalid_argument("Section face boundary edge is missing");
                expected[found->second].push_back(i);
            }
        }
        for (Index i = 0; i < mesh.edges.size(); ++i) {
            const auto &actual = mesh.edges[i].faces;
            if (actual.size() > 2)
                throw std::invalid_argument("Section mesh contains a nonmanifold edge");
            auto ordered = actual;
            std::sort(ordered.begin(), ordered.end());
            if (ordered != expected[i])
                throw std::invalid_argument("Section edge/face adjacency is inconsistent");
        }
    }
    Index edge(Index a, Index b) const { return edges.at(edgeKey(a, b)); }
    bool adjacentEdges(Index first, Index second) const {
        const auto &a = mesh.edges.at(first).points;
        const auto &b = mesh.edges.at(second).points;
        return a[0] == b[0] || a[0] == b[1] || a[1] == b[0] || a[1] == b[1];
    }
    const SubdivisionLevel &mesh;
    std::map<Key, Index> edges;
    std::map<Index, std::vector<Index>> roots;
};

void validateCurve(const SectionCurve &curve) {
    checkCount(curve.points.size());
    checkCount(curve.fragments);
    checkCount(curve.sourceSegments.size());
    if (curve.points.size() != curve.knuckles.size())
        throw std::invalid_argument("Section curve point/knuckle counts disagree");
    for (auto p : curve.points)
        finite(p);
    if (!curve.sourceSegments.empty() &&
        (curve.points.empty() || curve.sourceSegments.size() != curve.points.size() - 1))
        throw std::invalid_argument("Section source segment count is inconsistent");
    for (auto id : curve.sourceSegments)
        if (id == 0 || id == std::numeric_limits<std::int64_t>::min())
            throw std::invalid_argument("Section source segment ID is invalid");
}

void mergeCurve(SectionCurve &fixed, const SectionCurve &source, unsigned direction) {
    const bool prepend = direction < 2;
    const bool reverse = direction == 0 || direction == 3;
    SectionCurve match = source;
    if (reverse) {
        std::reverse(match.points.begin(), match.points.end());
        std::reverse(match.knuckles.begin(), match.knuckles.end());
        std::reverse(match.sourceSegments.begin(), match.sourceSegments.end());
        for (auto &segment : match.sourceSegments)
            segment = -segment;
    }
    const Index size = fixed.points.size() + match.points.size() - 1;
    checkCount(size);
    // If caller-supplied curves omit provenance, do not invent missing IDs.
    const bool hasSources = !fixed.sourceSegments.empty() && !match.sourceSegments.empty();
    if (prepend) {
        const bool knuckle = fixed.knuckles.front() || match.knuckles.back();
        match.points.pop_back();
        match.knuckles.pop_back();
        fixed.points.insert(fixed.points.begin(), match.points.begin(), match.points.end());
        fixed.knuckles.insert(fixed.knuckles.begin(), match.knuckles.begin(), match.knuckles.end());
        // Deliberate fix: write to the shared point after it has moved, and OR
        // the actual matching endpoint. The old InsertSpline overwrote this.
        fixed.knuckles[match.points.size()] = knuckle;
        if (hasSources)
            fixed.sourceSegments.insert(fixed.sourceSegments.begin(), match.sourceSegments.begin(),
                                        match.sourceSegments.end());
    } else {
        fixed.knuckles.back() = fixed.knuckles.back() || match.knuckles.front();
        fixed.points.insert(fixed.points.end(), match.points.begin() + 1, match.points.end());
        fixed.knuckles.insert(fixed.knuckles.end(), match.knuckles.begin() + 1, match.knuckles.end());
        if (hasSources)
            fixed.sourceSegments.insert(fixed.sourceSegments.end(), match.sourceSegments.begin(),
                                        match.sourceSegments.end());
    }
    if (!hasSources)
        fixed.sourceSegments.clear();
    fixed.fragments = std::max(fixed.fragments, fixed.points.size() - 1);
    fixed.closed = closed(fixed);
}

std::array<float, 4> endpointDistances(const SectionCurve &a, const SectionCurve &b) {
    return {squaredDistance(a.points.front(), b.points.front()),
            squaredDistance(a.points.front(), b.points.back()),
            squaredDistance(a.points.back(), b.points.front()),
            squaredDistance(a.points.back(), b.points.back())};
}
std::size_t joinValidated(std::vector<SectionCurve> &curves, float error, bool force) {
    for (auto &curve : curves)
        curve.closed = closed(curve);
    Index joined = 0, i = 0;
    while (i + 1 < curves.size()) {
        const auto &fixed = curves[i];
        if (fixed.points.size() < 2 || fixed.closed) {
            ++i;
            continue;
        }
        float nearest = 1e30f;
        Index match = curves.size();
        for (Index j = 0; j < curves.size(); ++j)
            if (j != i && curves[j].points.size() > 1 && !curves[j].closed) {
                const auto distances = endpointDistances(fixed, curves[j]);
                const float minimum = *std::min_element(distances.begin(), distances.end());
                if (minimum < nearest) {
                    nearest = minimum;
                    match = j;
                    if (minimum < 1e-5)
                        break;
                }
            }
        if (match < curves.size() && (nearest < error || force)) {
            const auto distances = endpointDistances(curves[i], curves[match]);
            unsigned direction = 0;
            while (direction < 3 && distances[direction] != nearest)
                ++direction;
            mergeCurve(curves[i], curves[match], direction);
            curves.erase(curves.begin() + std::ptrdiff_t(match));
            ++joined;
            i = 0;
        } else
            ++i;
    }
    return joined;
}

void addRawSegments(const MeshView &view, const Plane &plane, const std::vector<Index> &roots,
                    SectionResult &output) {
    if (view.mesh.level == 0)
        return; // Current Plus iterates root.Child, not control faces.
    const auto &mesh = view.mesh;
    std::set<Index> coplanarEdges;
    auto append = [&](SectionEndpoint first, SectionEndpoint second, Index face) {
        checkCount(output.segments.size() + 1);
        output.segments.push_back(
            {std::move(first), std::move(second), face, mesh.faces[face].rootControlFace});
    };
    for (auto root : roots)
        for (auto faceId : view.roots.at(root)) {
            const auto &face = mesh.faces[faceId];
            std::vector<SectionEndpoint> hits;
            Index first = face.points.back();
            float side1 = plane.evaluate(mesh.points[first].position);
            for (Index second : face.points) {
                const auto &p1 = mesh.points[first];
                const auto &p2 = mesh.points[second];
                const float side2 = plane.evaluate(p2.position);
                const Index edgeId = view.edge(first, second);
                const auto &edge = mesh.edges[edgeId];
                if ((side1 < -1e-5 && side2 > 1e-5) || (side1 > 1e-5 && side2 < -1e-5)) {
                    const float t = number(-side1 / (side2 - side1));
                    const Vec3 p = number({p1.position.x + t * (p2.position.x - p1.position.x),
                                           p1.position.y + t * (p2.position.y - p1.position.y),
                                           p1.position.z + t * (p2.position.z - p1.position.z)});
                    hits.push_back({p, edge.crease, edgeId, std::nullopt});
                } else if (std::abs(side1) <= 1e-5 && std::abs(side2) <= 1e-5) {
                    bool add = edge.faces.size() == 1;
                    if (!add)
                        for (Index other : edge.faces) {
                            for (Index point : mesh.faces[other].points)
                                if (std::abs(plane.evaluate(mesh.points[point].position)) > 1e-5) {
                                    add = true;
                                    break;
                                }
                            if (add)
                                break;
                        }
                    if (add && coplanarEdges.insert(edgeId).second) {
                        const bool a =
                            edge.crease ? p1.type == VertexType::Corner : p1.type != VertexType::Regular;
                        const bool b =
                            edge.crease ? p2.type == VertexType::Corner : p2.type != VertexType::Regular;
                        append({p1.position, a, edgeId, first}, {p2.position, b, edgeId, second}, faceId);
                    }
                } else if (std::abs(side2) < 1e-5)
                    hits.push_back({p2.position, p2.type != VertexType::Regular, edgeId, second});
                first = second;
                side1 = side2;
            }
            if (hits.size() > 1) {
                if (hits.front().edge == hits.back().edge &&
                    distance(hits.front().position, hits.back().position) < 1e-4) {
                    hits.pop_back();
                    ++output.nearDuplicateEndpointsRemoved;
                }
                Index i = 1;
                while (i < hits.size()) {
                    if (hits[i - 1].edge == hits[i].edge &&
                        distance(hits[i - 1].position, hits[i].position) < 1e-4) {
                        // Erase a complete record, unlike the old byte Move with
                        // SizeOf(T3DCoordinate) for an IntersectionData array.
                        hits.erase(hits.begin() + std::ptrdiff_t(i - 1));
                        ++output.nearDuplicateEndpointsRemoved;
                    } else
                        ++i;
                }
                for (Index i = 1; i < hits.size(); ++i)
                    append(hits[i - 1], hits[i], faceId);
            }
        }
}

void connectSegments(const MeshView &mesh, SectionResult &result) {
    std::vector<Index> remaining;
    for (Index i = 0; i < result.segments.size(); ++i)
        remaining.push_back(i);
    while (!remaining.empty()) {
        const Index seed = remaining.back();
        remaining.pop_back();
        const auto &initial = result.segments[seed];
        SectionCurve curve;
        curve.points = {initial.start.position, initial.end.position};
        curve.knuckles = {initial.start.knuckle, initial.end.knuckle};
        curve.sourceSegments = {std::int64_t(seed + 1)};
        curve.fragments = 1;
        SectionEndpoint first = initial.start, last = initial.end;
        Index i = 0;
        while (i < remaining.size()) {
            const Index id = remaining[i];
            const auto &s = result.segments[id];
            int direction = -1;
            auto near = [](const SectionEndpoint &a, const SectionEndpoint &b) {
                return distance(a.position, b.position) < 1e-4;
            };
            if (last.edge == s.start.edge) {
                if (near(last, s.start))
                    direction = 0;
            } else if (last.edge == s.end.edge) {
                if (near(last, s.end))
                    direction = 1;
            } else if (first.edge == s.start.edge) {
                if (near(first, s.start))
                    direction = 2;
            } else if (first.edge == s.end.edge) {
                if (near(first, s.end))
                    direction = 3;
            } else if (s.start.edge == s.end.edge && (mesh.adjacentEdges(first.edge, s.start.edge) ||
                                                      mesh.adjacentEdges(last.edge, s.start.edge))) {
                if (near(last, s.start))
                    direction = 0;
                else if (near(last, s.end))
                    direction = 1;
                else if (near(first, s.start))
                    direction = 2;
                else if (near(first, s.end))
                    direction = 3;
            }
            if (direction >= 0) {
                checkCount(curve.points.size() + 1);
                const bool backwards = direction == 1 || direction == 2;
                const auto &attached = backwards ? s.end : s.start;
                const auto &other = backwards ? s.start : s.end;
                if (direction < 2) {
                    curve.knuckles.back() = curve.knuckles.back() || attached.knuckle;
                    curve.points.push_back(other.position);
                    curve.knuckles.push_back(other.knuckle);
                    last = other;
                    curve.sourceSegments.push_back((backwards ? -1 : 1) * std::int64_t(id + 1));
                } else {
                    // Prepend traverses the candidate toward the existing start.
                    const auto &shared = direction == 2 ? s.start : s.end;
                    const auto &newStart = direction == 2 ? s.end : s.start;
                    curve.knuckles.front() = curve.knuckles.front() || shared.knuckle;
                    curve.points.insert(curve.points.begin(), newStart.position);
                    curve.knuckles.insert(curve.knuckles.begin(), newStart.knuckle);
                    first = newStart;
                    curve.sourceSegments.insert(curve.sourceSegments.begin(),
                                                (direction == 2 ? -1 : 1) * std::int64_t(id + 1));
                }
                curve.fragments = curve.points.size() - 1;
                remaining.erase(remaining.begin() + std::ptrdiff_t(i));
                i = 0;
            } else
                ++i;
        }
        curve.closed = closed(curve);
        result.connectedCurves.push_back(std::move(curve));
    }
}

void calculate(const MeshView &mesh, const Plane &plane, const std::vector<Index> &roots,
               SectionResult &result) {
    std::set<Index> unique;
    for (auto root : roots)
        if (!mesh.roots.count(root) || !unique.insert(root).second)
            throw std::invalid_argument("Selected section root face is absent or repeated");
    result.selectedControlFaces = roots;
    addRawSegments(mesh, plane, roots, result);
    connectSegments(mesh, result);
    result.joinedCurves = result.connectedCurves;
    const bool cleanTiny = result.joinedCurves.size() > 1;
    if (cleanTiny)
        result.joinCount = joinValidated(result.joinedCurves, .01f, false);
    result.curves = result.joinedCurves;
    if (cleanTiny)
        for (Index i = result.curves.size(); i > 0; --i) {
            const auto &curve = result.curves[i - 1];
            if (curve.points.size() < 2)
                continue;
            Bounds bounds;
            for (auto p : curve.points)
                bounds.add(p);
            if (squaredDistance(bounds.minimum, bounds.maximum) < 1e-3) {
                result.curves.erase(result.curves.begin() + std::ptrdiff_t(i - 1));
                ++result.discardedTinyCurves;
            }
        }
    result.found = !result.curves.empty();
}
} // namespace

std::size_t joinSectionCurves(std::vector<SectionCurve> &curves, float error, bool force) {
    if (!std::isfinite(error) || error < 0)
        throw std::invalid_argument("Join squared-distance threshold must be finite and nonnegative");
    checkCount(curves.size());
    for (const auto &curve : curves)
        validateCurve(curve);
    auto next = curves;
    const auto count = joinValidated(next, error, force);
    curves = std::move(next);
    return count;
}

void calculateSections(const SubdivisionLevel &mesh, const Plane &plane, const std::vector<Index> &roots,
                       SectionResult &output) {
    if (!plane.valid())
        throw std::domain_error("Cannot cut a surface with an invalid plane");
    checkCount(roots.size());
    const MeshView view(mesh);
    SectionResult result;
    calculate(view, plane, roots, result);
    output = std::move(result);
}

struct SectionIndex::Impl {
    const GeometryInput &input;
    const SubdivisionLevel &mesh;
    MeshView view;
    Bounds global;
    std::vector<Bounds> rootBounds;
    Impl(const GeometryInput &source, const SubdivisionLevel &level)
        : input(source), mesh(level), view(level), rootBounds(source.faces.size()) {
        checkCount(input.points.size());
        checkCount(input.edges.size());
        checkCount(input.faces.size());
        checkCount(input.layers.size());
        if (mesh.layers.size() != input.layers.size())
            throw std::invalid_argument("Section input and mesh layer counts disagree");
        std::vector<bool> used(input.points.size(), false);
        for (const auto &p : input.points)
            finite(p.position);
        for (const auto &edge : input.edges) {
            if (edge.points[0] >= used.size() || edge.points[1] >= used.size())
                throw std::invalid_argument("Section control edge index is invalid");
            used[edge.points[0]] = true;
            used[edge.points[1]] = true;
        }
        for (Index i = 0; i < input.faces.size(); ++i) {
            const auto &face = input.faces[i];
            if (face.layer >= input.layers.size() || face.points.size() < 3)
                throw std::invalid_argument("Section control face is invalid");
            for (auto p : face.points) {
                if (p >= used.size())
                    throw std::invalid_argument("Section control face point index is invalid");
                used[p] = true;
            }
            if (!view.roots.count(i))
                throw std::invalid_argument("Section mesh is missing a control face");
        }
        for (const auto &face : mesh.faces)
            if (face.rootControlFace >= input.faces.size() ||
                face.layer != input.faces[face.rootControlFace].layer)
                throw std::invalid_argument("Section mesh root face/layer does not match the control input");
        for (Index i = 0; i < used.size(); ++i)
            if (used[i])
                global.add(input.points[i].position);
        if (global.empty) {
            global.minimum = {};
            // Fresh Plus entities start at zero bounds. With isolated control
            // points CalcPointsExtents leaves those bounds untouched; only the
            // truly empty surface takes Rebuild's explicit [0,1]^3 fallback.
            global.maximum = input.points.empty() ? Vec3{1, 1, 1} : Vec3{};
        }
        for (Index i = 0; i < input.faces.size(); ++i)
            for (auto face : view.roots.at(i))
                for (auto p : mesh.faces[face].points)
                    rootBounds[i].add(mesh.points[p].position);
    }
    std::vector<Index> selectedRoots(const Plane &plane, SectionOptions options) const {
        if (!plane.valid())
            throw std::domain_error("Cannot cut a surface with an invalid plane");
        std::vector<Index> roots;
        if (plane.intersectsBox(global.minimum, global.maximum)) {
            for (Index layer = 0; layer < input.layers.size(); ++layer) {
                if (!(options.hydrostaticsLayersOnly ? input.layers[layer].useInHydrostatics
                                                     : input.layers[layer].useForIntersections))
                    continue;
                for (Index i = 0; i < input.faces.size(); ++i)
                    if (input.faces[i].layer == layer) {
                        const auto &bounds = rootBounds[i];
                        if (!bounds.empty && plane.intersectsBox(bounds.minimum, bounds.maximum))
                            roots.push_back(i);
                    }
            }
        }
        return roots;
    }
    SectionResult intersect(const Plane &plane, SectionOptions options) const {
        const auto roots=selectedRoots(plane,options);
        SectionResult result;
        result.surfaceBoxIntersects=plane.intersectsBox(global.minimum,global.maximum);
        if(result.surfaceBoxIntersects)calculate(view,plane,roots,result);
        return result;
    }
};
SectionIndex::SectionIndex(const GeometryInput &input, const SubdivisionLevel &mesh)
    : impl_(std::make_unique<Impl>(input, mesh)) {}
SectionIndex::~SectionIndex() = default;
SectionResult SectionIndex::intersect(const Plane &plane, SectionOptions options) const {
    return impl_->intersect(plane, options);
}
std::vector<std::size_t> SectionIndex::selectedRoots(const Plane &plane, SectionOptions options) const {
    return impl_->selectedRoots(plane, options);
}
void intersectSections(const GeometryInput &input, const SubdivisionLevel &mesh, const Plane &plane,
                       SectionOptions options, SectionResult &output) {
    output = SectionIndex(input, mesh).intersect(plane, options);
}
SectionResult intersectSections(const GeometryInput &input, const SubdivisionLevel &mesh, const Plane &plane,
                                SectionOptions options) {
    SectionResult result;
    intersectSections(input, mesh, plane, options, result);
    return result;
}

} // namespace fs::native_geometry
