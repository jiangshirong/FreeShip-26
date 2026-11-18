/*
 * Calculation-only adaptation of current FREE!ship Plus subdivision rules.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Plus credits Timoshenko V.F. (2006-2011) and Mark Malakanov (2015).
 * ShipCAD by Greg Green (2015) was consulted as a comparison; see
 * SUBDIVISION-PROVENANCE.md for sources and differences.
 *
 * This program is free software under the GNU General Public License,
 * version 2 or (at your option) any later version. It is distributed WITHOUT
 * ANY WARRANTY, including MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See ../../COPYING for the complete license.
 */
#include "subdivision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <cstring>
#include <unordered_map>

namespace fs::native_geometry {
namespace {
using Index = std::size_t;
constexpr Index absent = std::numeric_limits<Index>::max();
constexpr Index maximumItems = 2000000;
constexpr unsigned maximumLevel = 4;
using EdgeKey = std::pair<Index, Index>;
EdgeKey key(Index a, Index b) { return std::minmax(a, b); }

void finite(Vec3 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("Subdivision coordinates must be finite");
}
float numeric(float x) {
    if (!std::isfinite(x)) throw std::overflow_error("Subdivision result is not finite");
    return x;
}
Vec3 numeric(Vec3 p) { numeric(p.x); numeric(p.y); numeric(p.z); return p; }
void limit(Index count) {
    if (count > maximumItems) throw std::length_error("Subdivision exceeds the supported item count");
}
float component(const Vec3& p, unsigned axis) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }
float& component(Vec3& p, unsigned axis) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }

struct Node {
    Vec3 position{};
    VertexType type = VertexType::Regular;
    std::vector<Index> edges, faces;
    std::uint32_t operation=0;
};
struct Edge {
    Index first = 0, second = 0;
    bool crease = false, control = false;
    int curve = -1;
    std::vector<Index> faces;
};
struct Face {
    std::vector<Index> points;
    Index root = 0, layer = 0;
};
struct Curve {
    std::vector<Index> points;
    bool visible = true, freeStanding = false;
};
struct Mesh {
    std::vector<Node> points;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Curve> curves;
    std::map<EdgeKey, Index> edgeIndex;
    Index addPoint(Vec3 p, VertexType type,std::uint32_t operation=0) {
        limit(points.size() + 1);
        points.push_back({numeric(p), type, {}, {},operation});
        return points.size() - 1;
    }
    Index edge(Index a, Index b) const {
        const auto found = edgeIndex.find(key(a, b));
        if (found == edgeIndex.end()) throw std::logic_error("Subdivision face/curve edge is missing");
        return found->second;
    }
    Index addEdge(Index a, Index b, bool crease, bool control, int curve) {
        if (a == b) throw std::invalid_argument("Subdivision edge repeats its point");
        const auto found = edgeIndex.find(key(a, b));
        if (found != edgeIndex.end()) return found->second;
        limit(edges.size() + 1);
        const Index i = edges.size();
        edges.push_back({a, b, crease, control, curve, {}});
        edgeIndex.emplace(key(a, b), i);
        points.at(a).edges.push_back(i); points.at(b).edges.push_back(i);
        return i;
    }
};

Mesh initialMesh(const GeometryInput& input) {
    if (input.subdivisionMode != SubdivisionMode::QuadTriangle && input.subdivisionMode != SubdivisionMode::CatmullClark)
        throw std::invalid_argument("Unknown subdivision mode");
    if (input.desiredLevel > maximumLevel || input.precision < 0 || input.precision > 3)
        throw std::invalid_argument("Subdivision precision/level is outside the supported range");
    limit(input.points.size()); limit(input.edges.size()); limit(input.faces.size()); limit(input.curves.size()); limit(input.layers.size());
    Mesh mesh;
    mesh.points.reserve(input.points.size());
    for (const auto& p : input.points) {
        finite(p.position);
        if (int(p.type) < 0 || int(p.type) > 3) throw std::invalid_argument("Unknown vertex type");
        mesh.addPoint(p.position, p.type);
    }
    for (const auto& layer : input.layers) {
        if (!std::isfinite(layer.materialDensity) || !std::isfinite(layer.thickness) || layer.alphaBlend > 255)
            throw std::invalid_argument("Invalid layer metadata");
    }
    for (const auto& e : input.edges) {
        if (e.points[0] >= mesh.points.size() || e.points[1] >= mesh.points.size())
            throw std::invalid_argument("Control edge point index is out of range");
        if (e.curve < -1 || (e.curve >= 0 && Index(e.curve) >= input.curves.size()))
            throw std::invalid_argument("Control edge curve index is out of range");
        if (mesh.edgeIndex.count(key(e.points[0], e.points[1])))
            throw std::invalid_argument("Duplicate control edge");
        mesh.addEdge(e.points[0], e.points[1], e.crease, true, e.curve);
    }
    for (Index i = 0; i < input.faces.size(); ++i) {
        const auto& f = input.faces[i];
        if (f.points.size() < 3 || f.points.size() > 4096)
            throw std::invalid_argument("Control face requires 3..4096 points");
        if (f.layer >= input.layers.size()) throw std::invalid_argument("Control face layer index is out of range");
        std::set<Index> unique;
        for (auto p : f.points) {
            if (p >= mesh.points.size()) throw std::invalid_argument("Control face point index is out of range");
            if (!unique.insert(p).second) throw std::invalid_argument("Control face repeats a point");
            mesh.points[p].faces.push_back(i);
        }
        mesh.faces.push_back({f.points, i, f.layer});
        for (Index j = 0; j < f.points.size(); ++j) {
            const auto edge = mesh.edgeIndex.find(key(f.points[j], f.points[(j + 1) % f.points.size()]));
            if (edge == mesh.edgeIndex.end()) throw std::invalid_argument("Control face boundary edge is missing");
            auto& adjacent = mesh.edges[edge->second].faces;
            adjacent.push_back(i);
            if (adjacent.size() > 2) throw std::invalid_argument("Nonmanifold edge has more than two faces");
        }
    }
    for (Index i = 0; i < input.curves.size(); ++i) {
        const auto& curve = input.curves[i];
        if (curve.points.size() < 2) throw std::invalid_argument("Control curve requires at least two points");
        limit(curve.points.size());
        bool hasSurface = false, hasFree = false;
        std::set<EdgeKey> usedEdges;
        for (auto p : curve.points) {
            if (p >= mesh.points.size()) throw std::invalid_argument("Control curve point index is out of range");
            if (mesh.points[p].faces.empty()) hasFree = true; else hasSurface = true;
        }
        if (hasSurface && hasFree)
            throw std::domain_error("Mixed surface/free-standing control curves are not yet supported");
        for (Index j = 1; j < curve.points.size(); ++j) {
            const auto pair = key(curve.points[j - 1], curve.points[j]);
            const auto found = mesh.edgeIndex.find(pair);
            if (found == mesh.edgeIndex.end()) throw std::invalid_argument("Control curve edge is missing");
            const auto& e = mesh.edges[found->second];
            if (!usedEdges.insert(pair).second) throw std::domain_error("A control curve may not traverse the same edge twice");
            if (e.curve != int(i)) throw std::invalid_argument("Control curve and edge ownership disagree");
            if (hasSurface && e.faces.empty())
                throw std::domain_error("Control curves bridging disconnected surface points are not yet supported");
        }
        mesh.curves.push_back({curve.points, curve.visible, !hasSurface});
    }
    // An edge's curve tag must also be backed by that ordered curve chain.
    for (const auto& e : mesh.edges) if (e.curve >= 0) {
        const auto& chain = mesh.curves[Index(e.curve)].points;
        bool present = false;
        for (Index j = 1; j < chain.size(); ++j) if (key(chain[j - 1], chain[j]) == key(e.first, e.second)) present = true;
        if (!present) throw std::invalid_argument("Tagged control edge is absent from its curve");
    }
    return mesh;
}

std::uint32_t record(SubdivisionProgram* program,SubdivisionOperation::Code code,std::uint32_t a,std::uint32_t b,float factor,Vec3 value){
    if(!program)return 0;
    if(program->operations.size()>=2000000)throw std::length_error("Subdivision trace exceeds supported size");
    program->operations.push_back({code,a,b,factor,value});return std::uint32_t(program->operations.size()-1);
}
Vec3 averagePoint(const Mesh& mesh, Index index,SubdivisionProgram* program=nullptr,std::uint32_t* operation=nullptr) {
    const auto& point = mesh.points[index];
    if(operation)*operation=point.operation;
    if (point.edges.empty() || point.type == VertexType::Corner) return point.position;
    Vec3 result{};
    std::uint32_t resultId=0;
    if (point.type == VertexType::Crease) {
        result = {point.position.x * .5f, point.position.y * .5f, point.position.z * .5f};
        resultId=record(program,SubdivisionOperation::Scale,point.operation,0,.5f,result);
        for (auto ei : point.edges) {
            const auto& e = mesh.edges[ei];
            if (e.faces.size() == 1 || e.crease) {
                const auto& p = mesh.points[e.first == index ? e.second : e.first].position;
                result.x += .25f * p.x; result.y += .25f * p.y; result.z += .25f * p.z;
                resultId=record(program,SubdivisionOperation::AddScaled,resultId,mesh.points[e.first == index ? e.second : e.first].operation,.25f,result);
            }
        }
        if(operation)*operation=resultId;
        return numeric(result);
    }
    if (point.faces.empty())
        throw std::domain_error("A non-corner/non-crease derived point has edges but no faces");
    constexpr double pi = 3.1415926535897932385;
    constexpr float triangleWeight = float(pi / 3.0), quadWeight = float(pi / 2.0);
    static const std::array<float, 8> triangleCoefficients = [] {
        std::array<float, 8> values{};
        for (unsigned n = 1; n <= 8; ++n) {
            const double term = 3.0 / 8 + 1.0 / 4 * std::cos(2 * pi / n);
            values[n - 1] = float(5.0 / 3 - (8.0 / 3) * (term * term));
        }
        return values;
    }();
    float totalWeight = 0;
    Index triangles = 0;
    for (auto fi : point.faces) {
        const auto& face = mesh.faces[fi];
        if (face.points.size() != 3 && face.points.size() != 4)
            throw std::logic_error("Averaging requires subdivided triangles or quads");
        Vec3 center{};
        std::uint32_t centerId=0;
        const bool triangle = face.points.size() == 3;
        if (triangle) ++triangles;
        for (auto p : face.points) {
            const float weight = triangle && p != index ? .375f : .25f;
            center.x += weight * mesh.points[p].position.x;
            center.y += weight * mesh.points[p].position.y;
            center.z += weight * mesh.points[p].position.z;
            centerId=record(program,SubdivisionOperation::AddScaled,centerId,mesh.points[p].operation,weight,center);
        }
        const float weight = triangle ? triangleWeight : quadWeight;
        result.x += weight * center.x; result.y += weight * center.y; result.z += weight * center.z;
        resultId=record(program,SubdivisionOperation::AddScaled,resultId,centerId,weight,result);
        totalWeight += weight;
    }
    if (totalWeight != 0) { result.x /= totalWeight; result.y /= totalWeight; result.z /= totalWeight;resultId=record(program,SubdivisionOperation::Divide,resultId,0,totalWeight,result); }
    const Index count = point.faces.size(), quads = count - triangles;
    float a;
    if (triangles == count) {
        if (count <= 8) a = triangleCoefficients[count - 1];
        else {
            const double term = 3.0 / 8 + 1.0 / 4 * std::cos(2 * pi / double(count));
            a = float(5.0 / 3 - (8.0 / 3) * (term * term));
        }
    } else if (quads == count) a = float(4.0 / double(count));
    else a = float(12.0 / double(3 * quads + 2 * triangles));
    if (a != 1.0f) {
        result.x = point.position.x + a * (result.x - point.position.x);
        result.y = point.position.y + a * (result.y - point.position.y);
        result.z = point.position.z + a * (result.z - point.position.z);
        resultId=record(program,SubdivisionOperation::Relax,point.operation,resultId,a,result);
    }
    if(operation)*operation=resultId;
    return numeric(result);
}

Vec3 midpoint(Vec3 a, Vec3 b) { return numeric({.5f * (a.x + b.x), .5f * (a.y + b.y), .5f * (a.z + b.z)}); }

Mesh refine(const Mesh& previous, SubdivisionMode mode, unsigned nextLevel,SubdivisionProgram* program=nullptr) {
    Mesh mesh;
    mesh.curves.resize(previous.curves.size());
    std::vector<Index> vertices(previous.points.size(), absent), edgePoints(previous.edges.size(), absent), centers(previous.faces.size(), absent);
    for (Index i = 0; i < previous.points.size(); ++i) if (!previous.points[i].faces.empty())
        vertices[i] = mesh.addPoint(previous.points[i].position, previous.points[i].type,previous.points[i].operation);
    for (Index i = 0; i < previous.edges.size(); ++i) if (!previous.edges[i].faces.empty()) {
        const auto& e = previous.edges[i];
        const auto value=midpoint(previous.points[e.first].position, previous.points[e.second].position);
        edgePoints[i] = mesh.addPoint(value,e.crease ? VertexType::Crease : VertexType::Regular,
            record(program,SubdivisionOperation::Midpoint,previous.points[e.first].operation,previous.points[e.second].operation,0,value));
    }
    for (Index i = 0; i < previous.faces.size(); ++i) {
        const auto& face = previous.faces[i];
        if (face.points.size() > 3 || mode == SubdivisionMode::CatmullClark) {
            Vec3 center{};
            std::uint32_t centerId=0;
            for (auto p : face.points) {
                center.x += previous.points[p].position.x;
                center.y += previous.points[p].position.y;
                center.z += previous.points[p].position.z;
                centerId=record(program,SubdivisionOperation::Add,centerId,previous.points[p].operation,0,center);
            }
            const float count = float(face.points.size());
            center.x /= count; center.y /= count; center.z /= count;
            centerId=record(program,SubdivisionOperation::Divide,centerId,0,count,center);
            centers[i] = mesh.addPoint(center, VertexType::Regular,centerId);
        }
    }
    auto makeFace = [&](std::vector<Index> points, const Face& parent,
                        const std::vector<std::tuple<Index, Index, bool, bool, int>>& edges) {
        limit(mesh.faces.size() + 1);
        const Index face = mesh.faces.size();
        mesh.faces.push_back({std::move(points), parent.root, parent.layer});
        for (const auto& [a, b, crease, control, curve] : edges) {
            const Index edge = mesh.addEdge(a, b, crease, control, curve);
            auto& e = mesh.edges[edge];
            if (control) e.curve = curve;
            e.faces.push_back(face);
        }
        for (auto p : mesh.faces.back().points) mesh.points[p].faces.push_back(face);
    };
    for (Index fi = 0; fi < previous.faces.size(); ++fi) {
        const auto& face = previous.faces[fi];
        const auto count = face.points.size();
        for (Index i = 0; i < count; ++i) {
            const Index original = face.points[i];
            const Index before = previous.edge(original, face.points[(i + count - 1) % count]);
            const Index after = previous.edge(original, face.points[(i + 1) % count]);
            const auto& ep = previous.edges[before]; const auto& en = previous.edges[after];
            const Index p = vertices[original], prior = edgePoints[before], next = edgePoints[after];
            if (count != 3 || mode == SubdivisionMode::CatmullClark) {
                std::vector<Index> points(4);
                const Index start = i % 4, center = centers[fi];
                points[start] = p; points[(start + 1) % 4] = next;
                points[(start + 2) % 4] = center; points[(start + 3) % 4] = prior;
                makeFace(std::move(points), face, {{prior, p, ep.crease, ep.control, ep.curve},
                    {p, next, en.crease, en.control, en.curve}, {next, center, false, false, -1}, {prior, center, false, false, -1}});
            } else {
                makeFace({prior, p, next}, face, {{prior, p, ep.crease, ep.control, ep.curve},
                    {p, next, en.crease, en.control, en.curve}, {next, prior, false, false, -1}});
            }
        }
        if (count == 3 && mode == SubdivisionMode::QuadTriangle) {
            const Index a = edgePoints[previous.edge(face.points[0], face.points[2])];
            const Index b = edgePoints[previous.edge(face.points[1], face.points[0])];
            const Index c = edgePoints[previous.edge(face.points[2], face.points[1])];
            makeFace({a, b, c}, face, {{a, b, false, false, -1}, {b, c, false, false, -1}, {c, a, false, false, -1}});
        }
    }
    // Free-standing curves are copied at the first requested level; only
    // subsequent levels split their segments. Surface-attached chains follow
    // the actual vertex and edge descendants, preserving their direction.
    std::vector<VertexType> controlTypes;
    std::vector<bool> controlCreases;
    if (nextLevel == 1) {
        for (const auto& p : previous.points) controlTypes.push_back(p.type);
        for (const auto& e : previous.edges) controlCreases.push_back(e.crease);
    }
    for (Index ci = 0; ci < previous.curves.size(); ++ci) {
        const auto& old = previous.curves[ci];
        auto& curve = mesh.curves[ci]; curve.visible = old.visible; curve.freeStanding = old.freeStanding;
        if (!old.freeStanding) {
            curve.points.push_back(vertices.at(old.points.front()));
            for (Index j = 1; j < old.points.size(); ++j) {
                curve.points.push_back(edgePoints.at(previous.edge(old.points[j - 1], old.points[j])));
                curve.points.push_back(vertices.at(old.points[j]));
            }
        } else if (nextLevel == 1) {
            for (Index j = 1; j < old.points.size(); ++j) {
                const Index a = old.points[j - 1], b = old.points[j];
                const Index segment = previous.edge(a, b);
                if (!controlCreases[segment]) {
                    controlCreases[segment] = true;
                    // The legacy free-curve initializer uses the public edge
                    // setter before applying its explicit endpoint rules.
                    // That setter can turn a shared multi-edge point into a
                    // corner; later curve segments must observe the change.
                    for (Index endpoint : {a, b}) if (controlTypes[endpoint] != VertexType::Corner) {
                        Index creaseCount = 0;
                        for (Index ei : previous.points[endpoint].edges) if (controlCreases[ei]) ++creaseCount;
                        controlTypes[endpoint] = creaseCount == 0 ? VertexType::Regular : creaseCount == 1 ? VertexType::Dart :
                                                 creaseCount == 2 ? VertexType::Crease : VertexType::Corner;
                    }
                }
                if (j == 1) controlTypes[a] = VertexType::Corner;
                if (j + 1 == old.points.size()) controlTypes[b] = VertexType::Corner;
                if (controlTypes[a] != VertexType::Corner) controlTypes[a] = VertexType::Crease;
                if (controlTypes[b] != VertexType::Corner) controlTypes[b] = VertexType::Crease;
                if (j == 1) curve.points.push_back(mesh.addPoint(previous.points[a].position, controlTypes[a],previous.points[a].operation));
                curve.points.push_back(mesh.addPoint(previous.points[b].position, controlTypes[b],previous.points[b].operation));
            }
        } else {
            for (Index j = 0; j < old.points.size(); ++j) {
                const auto& p = previous.points[old.points[j]];
                if (j) {const auto& prior=previous.points[old.points[j-1]];const auto value=midpoint(prior.position,p.position);
                    curve.points.push_back(mesh.addPoint(value,VertexType::Crease,record(program,SubdivisionOperation::Midpoint,prior.operation,p.operation,0,value)));}
                curve.points.push_back(mesh.addPoint(p.position, p.type,p.operation));
            }
        }
        if (old.freeStanding) {
            for (Index j = 1; j < curve.points.size(); ++j)
                mesh.addEdge(curve.points[j - 1], curve.points[j], true, false, int(ci));
        }
    }
    // Simultaneous averaging: no point observes already averaged neighbors.
    std::vector<Vec3> averaged; averaged.reserve(mesh.points.size());
    std::vector<std::uint32_t> averagedIds(mesh.points.size());
    for (Index i = 0; i < mesh.points.size(); ++i) averaged.push_back(averagePoint(mesh, i,program,&averagedIds[i]));
    for (Index i = 0; i < mesh.points.size(); ++i) {mesh.points[i].position = averaged[i];mesh.points[i].operation=averagedIds[i];}
    return mesh;
}

SubdivisionLevel canonical(const Mesh& mesh, const Mesh& control, const GeometryInput& input, unsigned level,SubdivisionProgram* program=nullptr) {
    SubdivisionLevel out; out.level = level; out.layers = input.layers;
    std::vector<Index> mapped(mesh.points.size(), absent);
    std::vector<bool> referenced(mesh.points.size(), false);
    if (level == 0) for (Index i = 0; i < mesh.points.size(); ++i) {
        mapped[i] = out.points.size();
        out.points.push_back({mesh.points[i].position, mesh.points[i].type});
    }
    auto point = [&](Index i) {
        if (i >= mesh.points.size()) throw std::logic_error("Invalid generated point reference");
        referenced[i] = true;
        if (mapped[i] == absent) {
            mapped[i] = out.points.size();
            const auto& p = mesh.points[i]; out.points.push_back({p.position, p.type});
        }
        return mapped[i];
    };
    std::vector<Index> children(input.faces.size(), 0);
    for (const auto& face : mesh.faces) {
        SubdivisionFace f; f.rootControlFace = face.root;
        f.childIndex = level == 0 ? -1 : int(children[face.root]++); f.layer = face.layer;
        for (auto p : face.points) f.points.push_back(point(p));
        out.faces.push_back(std::move(f));
    }
    for (const auto& curve : mesh.curves) {
        SubdivisionCurve c; c.visible = curve.visible; c.freeStanding = curve.freeStanding;
        for (auto p : curve.points) { c.points.push_back(point(p)); c.knuckles.push_back(mesh.points[p].type == VertexType::Corner); }
        out.curves.push_back(std::move(c));
    }
    std::set<Index> edges;
    auto edge = [&](Index a, Index b) {
        const Index id = mesh.edge(a, b);
        if (!edges.insert(id).second) return;
        const auto& e = mesh.edges[id];
        const auto ends = key(point(e.first), point(e.second));
        out.edges.push_back({{ends.first, ends.second}, e.crease, e.curve, e.control, e.faces, point(e.first) != ends.first});
    };
    for (const auto& f : mesh.faces) for (Index i = 0; i < f.points.size(); ++i) edge(f.points[i], f.points[(i + 1) % f.points.size()]);
    for (const auto& c : mesh.curves) for (Index i = 1; i < c.points.size(); ++i) edge(c.points[i - 1], c.points[i]);
    std::vector<bool> used(input.points.size(), false);
    for (const auto& face : input.faces) for (auto p : face.points) used[p] = true;
    for (const auto& curve : input.curves) for (auto p : curve.points) used[p] = true;
    for (Index i = 0; i < used.size(); ++i) if (!used[i])
        out.looseControlPoints.push_back({i, input.points[i].position, input.points[i].type});
    for (Index i = 0; i < control.edges.size(); ++i) if (control.edges[i].faces.empty() && control.edges[i].curve < 0) {
        const auto& e = control.edges[i]; out.looseControlEdges.push_back({i, {e.first, e.second}, e.crease, e.curve});
    }
    for (Index i = 0; i < mesh.points.size(); ++i) if (referenced[i]) {
        if(program)program->boundOrder.push_back(std::uint32_t(mapped[i]));
        const auto& p = mesh.points[i];
        if (out.empty) { out.minimum = out.maximum = p.position; out.empty = false; }
        else for (unsigned axis = 0; axis < 3; ++axis) {
            component(out.minimum, axis) = std::min(component(out.minimum, axis), component(p.position, axis));
            component(out.maximum, axis) = std::max(component(out.maximum, axis), component(p.position, axis));
        }
    }
    if(program){program->outputs.resize(out.points.size());for(Index i=0;i<mapped.size();++i)if(mapped[i]!=absent)program->outputs[mapped[i]]=mesh.points[i].operation;}
    return out;
}
} // namespace

namespace {
void compactProgram(SubdivisionProgram& p){
    // Repeated face averages have exactly the same operands and operation
    // order. Share those expressions; never flatten/reassociate float sums.
    using Key=std::array<std::uint32_t,4>;
    struct Hash {std::size_t operator()(const Key& key)const noexcept{
        std::size_t h=1469598103934665603ULL;for(auto n:key){h^=n;h*=1099511628211ULL;}return h;
    }};
    std::unordered_map<Key,std::uint32_t,Hash> known;known.reserve(p.operations.size()/2);
    std::vector<std::uint32_t> mapped(p.operations.size());
    std::size_t end=p.controls.size()+1;
    for(std::size_t i=0;i<end;++i)mapped[i]=std::uint32_t(i);
    for(std::size_t i=end;i<p.operations.size();++i){
        auto op=p.operations[i];op.a=mapped[op.a];op.b=mapped[op.b];
        std::uint32_t bits;std::memcpy(&bits,&op.factor,sizeof(bits));
        const Key key{op.code,op.a,op.b,bits};const auto [at,inserted]=known.emplace(key,std::uint32_t(end));
        mapped[i]=at->second;if(inserted)p.operations[end++]=op;
    }
    p.operations.resize(end);for(auto& i:p.outputs)i=mapped[i];
    // Some temporary seeds/averages have no canonical output descendant.
    std::vector<bool> live(end);for(auto i:p.outputs)live[i]=true;
    for(std::size_t i=end;i-->p.controls.size()+1;)if(live[i]){live[p.operations[i].a]=true;live[p.operations[i].b]=true;}
    end=p.controls.size()+1;
    for(std::size_t i=end;i<p.operations.size();++i)if(live[i]){
        auto op=p.operations[i];op.a=mapped[op.a];op.b=mapped[op.b];mapped[i]=std::uint32_t(end);p.operations[end++]=op;
    }
    for(auto& i:p.outputs)i=mapped[i];p.operations.resize(end);
}
}
SubdivisionProgram compileSubdivision(const GeometryInput& input,unsigned level){
    if(level>maximumLevel)throw std::invalid_argument("Subdivision level must be between zero and four");
    SubdivisionProgram program;program.operations.push_back({});auto control=initialMesh(input);
    for(auto& p:control.points){p.operation=record(&program,SubdivisionOperation::Input,0,0,0,p.position);program.controls.push_back(p.operation);}
    auto mesh=control;for(unsigned n=1;n<=level;++n)mesh=refine(mesh,input.subdivisionMode,n,&program);
    program.mesh=canonical(mesh,control,input,level,&program);compactProgram(program);program.changedRoots.assign(input.faces.size(),true);
    program.evaluated=program.operations.size()-program.controls.size()-1;program.changedPoints=program.mesh.points.size();return program;
}
void SubdivisionProgram::update(const GeometryInput& input){
    if(controls.size()!=input.points.size()||outputs.size()!=mesh.points.size())throw std::invalid_argument("Subdivision trace dimensions differ");
    auto same=[](Vec3 a,Vec3 b){return std::memcmp(&a.x,&b.x,sizeof(float))==0&&std::memcmp(&a.y,&b.y,sizeof(float))==0&&std::memcmp(&a.z,&b.z,sizeof(float))==0;};
    std::vector<bool> dirty(operations.size());evaluated=0;changedPoints=0;
    for(Index i=0;i<controls.size();++i){const auto p=input.points[i].position;finite(p);auto& op=operations.at(controls[i]);dirty[controls[i]]=!same(op.value,p);op.value=p;}
    for(Index i=controls.size()+1;i<operations.size();++i){auto& op=operations[i];
        if(!dirty[op.a]&&!dirty[op.b])continue;
        const auto a=operations[op.a].value,b=operations[op.b].value;Vec3 value{};
        switch(op.code){
        case SubdivisionOperation::Scale:value={a.x*op.factor,a.y*op.factor,a.z*op.factor};break;
        case SubdivisionOperation::AddScaled:value={a.x+op.factor*b.x,a.y+op.factor*b.y,a.z+op.factor*b.z};break;
        case SubdivisionOperation::Divide:value={a.x/op.factor,a.y/op.factor,a.z/op.factor};break;
        case SubdivisionOperation::Relax:value={a.x+op.factor*(b.x-a.x),a.y+op.factor*(b.y-a.y),a.z+op.factor*(b.z-a.z)};break;
        case SubdivisionOperation::Midpoint:value=midpoint(a,b);break;
        case SubdivisionOperation::Add:value={a.x+b.x,a.y+b.y,a.z+b.z};break;
        default:throw std::logic_error("Invalid subdivision arithmetic operation");
        }
        numeric(value);++evaluated;dirty[i]=!same(op.value,value);op.value=value;
    }
    std::vector<bool> moved(mesh.points.size());
    for(Index i=0;i<mesh.points.size();++i){auto p=operations.at(outputs[i]).value;moved[i]=!same(mesh.points[i].position,p);if(moved[i])++changedPoints;mesh.points[i].position=p;}
    changedRoots.assign(input.faces.size(),false);for(const auto& f:mesh.faces)for(auto p:f.points)if(moved[p])changedRoots.at(f.rootControlFace)=true;
    mesh.empty=true;mesh.minimum=mesh.maximum={};
    for(auto i:boundOrder){const auto p=mesh.points.at(i).position;if(mesh.empty){mesh.minimum=mesh.maximum=p;mesh.empty=false;}else for(unsigned axis=0;axis<3;++axis){component(mesh.minimum,axis)=std::min(component(mesh.minimum,axis),component(p,axis));component(mesh.maximum,axis)=std::max(component(mesh.maximum,axis),component(p,axis));}}
    for(auto& p:mesh.looseControlPoints)p.position=input.points.at(p.controlPoint).position;
}

struct SubdivisionSurface::Impl {
    explicit Impl(GeometryInput value) : input(std::move(value)), control(initialMesh(input)) {}
    GeometryInput input;
    Mesh control;
    mutable Mesh latest;
    mutable std::vector<std::unique_ptr<SubdivisionLevel>> levels;
    mutable Index builds = 0;
};
SubdivisionSurface::SubdivisionSurface(GeometryInput input) : impl_(std::make_unique<Impl>(std::move(input))) {}
SubdivisionSurface::~SubdivisionSurface() = default;
SubdivisionSurface::SubdivisionSurface(SubdivisionSurface&&) noexcept = default;
SubdivisionSurface& SubdivisionSurface::operator=(SubdivisionSurface&&) noexcept = default;
const GeometryInput& SubdivisionSurface::input() const noexcept { return impl_->input; }
const SubdivisionLevel& SubdivisionSurface::level(unsigned level) const {
    if (level > maximumLevel) throw std::invalid_argument("Subdivision level must be between zero and four");
    if (level < impl_->levels.size()) return *impl_->levels[level];
    // Publish only after the requested expansion succeeds, retaining prior
    // valid levels if a later refinement overflows or exceeds its limit.
    std::vector<std::unique_ptr<SubdivisionLevel>> levels;
    Mesh current = impl_->levels.empty() ? impl_->control : impl_->latest;
    unsigned next = unsigned(impl_->levels.size());
    for (; next <= level; ++next) {
        if (next > 0) current = refine(current, impl_->input.subdivisionMode, next);
        levels.push_back(std::make_unique<SubdivisionLevel>(canonical(current, impl_->control, impl_->input, next)));
    }
    impl_->levels.reserve(impl_->levels.size() + levels.size());
    impl_->latest = std::move(current);
    for (auto& value : levels) impl_->levels.push_back(std::move(value));
    ++impl_->builds;
    return *impl_->levels[level];
}
void SubdivisionSurface::setPoint(Index index, Vec3 position) {
    finite(position);
    const auto old = impl_->input.points.at(index).position;
    if (old.x == position.x && old.y == position.y && old.z == position.z) return;
    impl_->input.points[index].position = position;
    impl_->control.points[index].position = position;
    impl_->latest = {}; impl_->levels.clear();
}
bool SubdivisionSurface::built(unsigned level) const noexcept { return level < impl_->levels.size(); }
std::size_t SubdivisionSurface::rebuildCount() const noexcept { return impl_->builds; }

float hydrostaticKeel(const GeometryInput& input) {
    float z=0;bool first=true;
    auto take=[&](size_t p){const auto v=input.points.at(p).position.z;if(first||v<z)z=v;first=false;};
    for(const auto& f:input.faces)if(input.layers.at(f.layer).useInHydrostatics)for(auto p:f.points)take(p);
    if(first){for(const auto& f:input.faces)for(auto p:f.points)take(p);for(const auto& e:input.edges)for(auto p:e.points)take(p);}
    return z;
}
} // namespace fs::native_geometry
