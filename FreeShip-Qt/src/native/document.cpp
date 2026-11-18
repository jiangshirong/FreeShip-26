// FreeShip_26 native input decoder. GPL-2.0-or-later; see COPYING.
#include "document.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <limits>
#include <set>
#include <map>
namespace fs::native_geometry {
namespace {
[[noreturn]] void bad() {
    throw std::invalid_argument("Invalid native lines-plan input");
}
QJsonObject obj(QJsonValue v) {
    if (!v.isObject())
        bad();
    return v.toObject();
}
QJsonArray arr(QJsonValue v) {
    if (!v.isArray() || v.toArray().size() > 2000000)
        bad();
    return v.toArray();
}
double num(QJsonValue v) {
    if (!v.isDouble() || !std::isfinite(v.toDouble()))
        bad();
    return v.toDouble();
}
int integer(QJsonValue v, int lo = 0, int hi = 1000000) {
    double n = num(v);
    if (n < lo || n > hi || std::floor(n) != n)
        bad();
    return int(n);
}
float scalar(QJsonValue v) {
    float n = float(num(v));
    if (!std::isfinite(n) || std::abs(n) > 1e9f)
        bad();
    return n;
}
bool flag(QJsonValue v) {
    if (!v.isBool())
        bad();
    return v.toBool();
}
Vec3 point(QJsonValue v) {
    auto a = arr(v);
    if (a.size() != 3)
        bad();
    return {scalar(a[0]), scalar(a[1]), scalar(a[2])};
}
std::vector<std::size_t> ids(QJsonValue v) {
    std::vector<std::size_t> out;
    for (auto n : arr(v))
        out.push_back(integer(n));
    return out;
}
} // namespace
DocumentInput DocumentInput::read(const QJsonObject &root) {
    if (root["format"] != "FreeShipLinesplanInput" || integer(root["version"]) != 1)
        bad();
    DocumentInput d;
    d.documentId = root["document_id"].toString();
    double rev = num(root["revision"]);
    if (d.documentId.isEmpty() || d.documentId.contains(QChar(0)) || rev < 1 || rev > 9007199254740991.0 ||
        std::floor(rev) != rev)
        bad();
    d.revision = quint64(rev);
    auto g = obj(root["geometry"]);
    if (g["format"] != "FreeShipGeometryInput" || integer(g["version"]) != 1)
        bad();
    auto &in = d.geometry;
    in.subdivisionMode = SubdivisionMode(integer(g["subdivision_mode"], 0, 1));
    in.precision = integer(g["precision"], 0, 3);
    in.desiredLevel = integer(g["desired_level"], 1, 4);
    for (auto value : arr(g["points"])) {
        auto p = obj(value);
        in.points.push_back({point(p["position"]), VertexType(integer(p["type"], 0, 3))});
    }
    for (auto value : arr(g["edges"])) {
        auto e = obj(value);
        auto ends = ids(e["points"]);
        if (ends.size() != 2)
            bad();
        in.edges.push_back({{ends[0], ends[1]}, flag(e["crease"]), integer(e["curve"], -1)});
    }
    for (auto value : arr(g["faces"])) {
        auto f = obj(value);
        in.faces.push_back({ids(f["points"]), std::size_t(integer(f["layer"]))});
    }
    for (auto value : arr(g["layers"])) {
        auto l = obj(value);
        LayerInput layer;
        layer.symmetric = flag(l["symmetric"]);
        layer.surfaceVisible = flag(l["surface_visible"]);
        layer.controlNetVisible = flag(l["control_net_visible"]);
        layer.useForIntersections = flag(l["use_for_intersections"]);
        layer.useInHydrostatics = flag(l["use_in_hydrostatics"]);
        layer.showInLinesplan = flag(l["show_in_linesplan"]);
        layer.developable = flag(l["developable"]);
        layer.color = integer(l["color"], std::numeric_limits<int>::min(), 2147483647);
        layer.alphaBlend = integer(l["alpha_blend"], 0, 255);
        layer.materialDensity = scalar(l["material_density"]);
        layer.thickness = scalar(l["thickness"]);
        layer.name = l["name"].toString().toStdString();
        layer.layerId = integer(l["layer_id"], -2147483647, 2147483647);
        in.layers.push_back(layer);
    }
    for (auto value : arr(g["curves"])) {
        auto c = obj(value);
        in.curves.push_back({ids(c["points"]), flag(c["visible"])});
    }
    auto settings = obj(root["settings"]);
    d.draft = scalar(settings["draft"]);
    d.split = scalar(settings["split"]);
    d.creaseSplit = scalar(settings["crease_split"]);
    d.imperial = flag(settings["imperial"]);
    d.simplify = flag(settings["simplify"]);
    d.shadeUnderwater = flag(settings["shade_underwater"]);
    d.colors = obj(root["colors"]);
    d.texts = obj(root["texts"]);
    for (auto name : {"underwater", "station", "buttock", "waterline", "diagonal", "crease", "grid", "red"}) {
        auto c = obj(d.colors[name]);
        integer(c["value"], 0, 0xffffff);
        integer(c["raw"], std::numeric_limits<int>::min(), 2147483647);
        integer(c["dxf"], 1, 255);
    }
    for (auto name : {"base", "dwl", "center"})
        if (!d.texts[name].isString() || d.texts[name].toString().size() > 1024)
            bad();
    for (auto value : arr(root["sections"])) {
        auto s = obj(value);
        auto p = arr(s["plane"]);
        if (p.size() != 4)
            bad();
        Plane plane(scalar(p[0]), scalar(p[1]), scalar(p[2]), scalar(p[3]));
        if (!plane.valid())
            bad();
        d.sections.push_back({integer(s["kind"], 1, 4), integer(s["order"]), plane});
    }
    if (d.sections.size() > 10000)
        bad();
    std::sort(d.sections.begin(), d.sections.end(),
              [](auto &a, auto &b) { return std::tie(a.kind, a.order) < std::tie(b.kind, b.order); });
    for (std::size_t i = 1; i < d.sections.size(); ++i)
        if (d.sections[i].kind == d.sections[i - 1].kind && d.sections[i].order == d.sections[i - 1].order)
            bad();
    if (root.contains("surface_display")) {
        auto display = obj(root["surface_display"]);
        d.surfaceDisplay = true;
        d.showNormals = flag(display["show_normals"]);
        auto bindings = [&](const char *name, std::size_t count) {
            auto values = arr(display[name]);
            if (std::size_t(values.size()) != count) bad();
            std::vector<int> result;
            std::set<int> seen;
            for (auto value : values) {
                auto id = integer(value, 1, 2000000);
                if (!seen.insert(id).second) bad();
                result.push_back(id);
            }
            return result;
        };
        d.faceIds = bindings("faces", in.faces.size());
        d.layerIds = bindings("layers", in.layers.size());
        d.curveIds = bindings("curves", in.curves.size());
        if (display.contains("flows")) {
            std::set<int> seen;
            for (auto value:arr(display["flows"])) {
                auto flow=obj(value);auto source=arr(flow["source"]);
                if (source.size()!=2 || d.flows.size()>=4096) bad();
                const auto id=integer(flow["id"],1,2000000);
                if (!seen.insert(id).second) bad();
                d.flows.push_back({id,integer(flow["view"],0,2),scalar(source[0]),scalar(source[1])});
            }
        }
        if (display.contains("layer_faces")) {
            auto layers=arr(display["layer_faces"]);
            if (std::size_t(layers.size())!=in.layers.size()) bad();
            std::map<int,std::size_t> indices;
            for(std::size_t i=0;i<d.faceIds.size();++i)indices.emplace(d.faceIds[i],i);
            std::set<std::size_t> seen;
            for (qsizetype i=0;i<layers.size();++i) {
                std::vector<std::size_t> faces;
                for (auto value:arr(layers[i])) {
                    auto found=indices.find(integer(value,1,2000000));if(found==indices.end())bad();
                    const auto face=found->second;
                    if(in.faces[face].layer!=std::size_t(i)||!seen.insert(face).second)bad();
                    faces.push_back(face);
                }
                d.layerFaces.push_back(std::move(faces));
            }
            if (seen.size()!=in.faces.size()) bad();
        } else {
            d.layerFaces.resize(in.layers.size());
            for (std::size_t i=0;i<in.faces.size();++i) d.layerFaces.at(in.faces[i].layer).push_back(i);
        }
    }
    return d;
}
} // namespace fs::native_geometry
