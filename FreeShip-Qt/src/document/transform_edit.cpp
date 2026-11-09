// GPL-2.0-or-later. Semantics from bridge/AdvancedEdits.inc, FreeEdit.Model_Scale,
// FreeProjectSettings setters and FreeGeometry.RotateVector. Each arithmetic
// expression deliberately retains the original Single order (no fast-math/FMA).
#include "transform_edit.h"
#include "../../third_party/fpc/trig.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void invalid(const char *message) { throw std::invalid_argument(message); }
quint64 id(const QJsonValue &v, quint64 minimum = 1) {
    const double n = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(n) || n < double(minimum) ||
        n > Fbm::MaximumCollectionItems || std::floor(n) != n) invalid("Invalid transform entity ID");
    return quint64(n);
}
void check(float n) {
    if (!std::isfinite(n) || std::abs(n) > 1e9f) invalid("Transformed coordinate exceeds document limits");
}
void check(FbmPosition p) { check(p.x); check(p.y); check(p.z); }
FbmPosition vector(const QJsonValue &value) {
    if (!value.isArray() || value.toArray().size() != 3) invalid("Transform vector requires three numbers");
    const auto a = value.toArray(); float p[3];
    for (int i = 0; i < 3; ++i) {
        const double n = a[i].toDouble();
        if (!a[i].isDouble() || !std::isfinite(n) || std::abs(n) > 1e9) invalid("Invalid transform coordinate");
        p[i] = float(n);
    }
    return {p[0], p[1], p[2]};
}
bool differs(FbmPosition a, FbmPosition b) { return a.x != b.x || a.y != b.y || a.z != b.z; }

using fpc::trig;
struct Transform {
    QString operation;
    FbmPosition factor, pivot;
    std::array<float, 9> rotation{};
    Transform(QString op, FbmPosition v, FbmPosition p) : operation(std::move(op)), factor(v), pivot(p) {
        if (operation == "scale" && (v.x <= 0 || v.y <= 0 || v.z <= 0)) invalid("Scale factors must be positive");
        if (operation == "rotate") {
            for (float n : {v.x, v.y, v.z}) if (std::abs(n) > 360000) invalid("Rotation angle limit exceeded");
            const float cx = trig(v.x, true), sx = trig(v.x, false);
            const float cy = trig(v.y, true), sy = trig(v.y, false);
            const float cz = trig(v.z, true), sz = trig(v.z, false);
            rotation = {cy*cz, cy*sz, -sy,
                        sx*sy*cz-cx*sz, sx*sy*sz+cx*cz, sx*cy,
                        cx*sy*cz+sx*sz, cx*sy*sz-sx*cz, cx*cy};
        }
    }
    FbmPosition operator()(FbmPosition p) const {
        FbmPosition result;
        if (operation == "translate") result = {p.x+factor.x, p.y+factor.y, p.z+factor.z};
        else if (operation == "scale") result = {p.x*factor.x, p.y*factor.y, p.z*factor.z};
        else {
            p = {p.x-pivot.x, p.y-pivot.y, p.z-pivot.z};
            const auto &r = rotation;
            result = {p.x*r[0]+p.y*r[1]+p.z*r[2], p.x*r[3]+p.y*r[4]+p.z*r[5], p.x*r[6]+p.y*r[7]+p.z*r[8]};
            result = {result.x+pivot.x, result.y+pivot.y, result.z+pivot.z};
        }
        check(result); return result;
    }
};

void scaleFields(FbmTransformFields &fields, FbmPosition v) {
    const auto dimension = [&](float &value, float factor) {
        const float next = std::abs(value * factor);
        if (!std::isfinite(next)) invalid("Scaled project dimension is not finite");
        if (std::abs(next - value) > 1e-7) { value = next; fields.particularsSet = true; }
    };
    dimension(fields.dimensions.x, v.x); dimension(fields.dimensions.y, v.y); dimension(fields.dimensions.z, v.z);
    if (!fields.defaultSplit) fields.split = fields.split * v.x;
    for (auto &[name, p] : fields.calculations) {
        // Record layouts are the persisted Single prefixes from FreeTypes.pas.
        // Speeds, coefficients, unused fields, flags and record padding stay put.
        if (name == "kaper" || name == "planing") {
            p.at(0) = p[0]*v.z; p.at(1) = p[1]*v.x; p.at(2) = p[2]*v.y;
            p.at(4) = p[4]*v.x*v.y*v.z; p.at(6) = p[6]*v.x*v.z;
        } else {
            p.at(3) = p[3]*v.y; p.at(5) = p[5]*v.x*v.y*v.z;
            p.at(6) = p[6]*v.z; p.at(7) = p[7]*v.z; p.at(8) = p[8]*v.x;
            p.at(9) = p[9]*v.x*v.z; p.at(11) = p[11]*v.x;
            p.at(12) = p[12]*v.x; p.at(13) = p[13]*v.x*v.z;
            p.at(15) = p[15]*v.x*v.z;
            p.at(16) = (name == "hollenbach" || name == "oortmerssen" || name == "fung") ? p[16]*v.x : p[16]*v.x*v.y;
        }
    }
    // Model_Scale's section-plane scaling and propeller scaling are commented
    // out. HydrostaticCalculation Draft/Trim are runtime calculation objects;
    // persistent project hydro/cross-curve settings are not scaled.
}
} // namespace

bool nativeTransformCommand(const QJsonObject &request) {
    const auto op = request["operation"].toString();
    return request["method"] == "advanced" && (op == "translate" || op == "scale" || op == "rotate");
}

Fbm editTransforms(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request, bool *changedResult) {
    if (changedResult) *changedResult = false;
    if (!nativeTransformCommand(request)) invalid("Unsupported native transform");
    const FbmPosition pivot = request.contains("pivot") ? vector(request["pivot"]) : FbmPosition{};
    const auto net = source.controlNet();
    const std::array<QString, 4> kinds{"point", "edge", "face", "curve"};
    const auto foundKind = std::find(kinds.begin(), kinds.end(), request["kind"].toString());
    if (foundKind == kinds.end()) invalid("Unsupported transform selection");
    const int kind = int(foundKind - kinds.begin());
    const std::array<size_t, 4> sizes{net.points.size(), net.edges.size(), net.faces.size(), net.curves.size()};
    std::array<QHash<quint64, int>, 4> lookup;
    const auto high = bindings["high_water"].toObject();
    for (int k = 0; k < 4; ++k) {
        const QString key = kinds[k] + 's'; const auto ids = bindings[key].toArray();
        if (!bindings[key].isArray() || size_t(ids.size()) != sizes[k]) invalid("Transform binding count mismatch");
        const auto limit = id(high[key], 0);
        for (int i = 0; i < ids.size(); ++i) {
            const auto n = id(ids[i]);
            if (lookup[k].contains(n) || n > limit) invalid("Invalid transform bindings");
            lookup[k].insert(n, i);
        }
    }
    const auto selection = request["ids"].toArray();
    if (!request["ids"].isArray() || selection.isEmpty() || selection.size() > 100000) invalid("Invalid transform selection size");
    QSet<quint64> seen; std::vector<bool> wanted(net.points.size(), false); size_t count = 0;
    const auto include = [&](int index) {
        const auto &p = net.points.at(size_t(index)).value;
        if (p.locked || p.hardAnchor || p.constrained) invalid("Transform selection contains a locked or constrained point");
        if (!wanted[size_t(index)]) { wanted[size_t(index)] = true; ++count; }
    };
    for (const auto value : selection) {
        const auto key = id(value); const auto it = lookup[kind].constFind(key);
        if (it == lookup[kind].cend()) invalid("Unknown or deleted transform entity ID");
        if (seen.contains(key)) invalid("Duplicate transform entity ID");
        seen.insert(key);
        const auto index = size_t(*it);
        if (kind == 0) include(int(index));
        else if (kind == 1) { include(net.edges[index].a); include(net.edges[index].b); }
        else if (kind == 2) for (int p : net.faces[index].points) include(p);
        else for (int p : net.curves[index].points) include(p);
    }
    const auto operation = request["operation"].toString(); const auto v = vector(request["vector"]);
    const Transform transform(operation, v, pivot); const bool all = count == net.points.size();
    if (request.contains("adjust_markers") && !request["adjust_markers"].isBool()) invalid("Invalid adjust_markers flag");
    const bool markers = request["adjust_markers"].toBool(false);
    if (markers && !all) invalid("Markers can only follow a whole-model transform");
    std::vector<FbmPosition> positions; positions.reserve(net.points.size()); bool changed = false;
    for (size_t i = 0; i < net.points.size(); ++i) {
        const auto &p = net.points[i].value; const FbmPosition old{p.x, p.y, p.z};
        const auto next = wanted[i] ? transform(old) : old;
        changed = changed || differs(next, old); positions.push_back(next);
    }
    changed = changed || (all && operation == "scale" && (v.x != 1 || v.y != 1 || v.z != 1));
    if (!changed) return source;
    const auto finish = [&](Fbm candidate) {
        if (changedResult) *changedResult = true;
        return candidate;
    };
    if (!all) return finish(source.withPointPositions(positions));
    auto fields = source.transformFields();
    if (markers) for (auto &marker : fields.markers) for (auto &p : marker) p = transform(p);
    if (operation == "scale") scaleFields(fields, v);
    else if (operation == "translate") {
        if (!fields.defaultSplit) fields.split = fields.split + v.x;
        for (auto &plane : fields.sections) {
            plane[3] = plane[3] - (plane[0]*v.x + plane[1]*v.y + plane[2]*v.z); check(plane[3]);
        }
    }
    return finish(source.withTransformFields(positions, fields));
}
} // namespace fs::document
