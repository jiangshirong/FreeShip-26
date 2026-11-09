// GPL-2.0-or-later. Normal rules adapted from FREE!ship Plus
// FreeSubdivisionPoint.FGetNormal and FreeGometry_Functions.UnifiedNormal.
// Original copyright Martijn van Engeland; Plus Timoshenko and Malakanov.
#include "surface_display.h"
#include "flowlines.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace fs::native_geometry {
namespace {
QJsonArray coordinates(Vec3 p) {
    for (float n : {p.x, p.y, p.z})
        if (!std::isfinite(n) || std::abs(n) > 1e9f)
            throw std::runtime_error("Invalid surface display coordinate");
    return {double(p.x), double(p.y), double(p.z)};
}
Vec3 add(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 legacyNormalAdd(Vec3 a, Vec3 b) {
    // Plus AddPoint clamps negative Y even for normal vectors. Preserve this
    // inherited behavior here only; ordinary vector/coordinate sums must not.
    auto sum=add(a,b);
    if (sum.y<0) sum.y=0;
    return sum;
}
Vec3 normalize(Vec3 p) {
    float length = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
    if (std::isnan(length)) return {};
    length = std::max(length, 1e-6f);
    return {p.x/length, p.y/length, p.z/length};
}
Vec3 normal(Vec3 a, Vec3 b, Vec3 c) {
    return normalize({(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),
                      (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),
                      (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)});
}
}
std::vector<Vec3> surfacePointNormals(const SubdivisionLevel &mesh) {
    std::vector<Vec3> normals(mesh.points.size());
    for (std::size_t i=0; i<mesh.faces.size(); ++i) {
        const auto &f=mesh.faces[i];
        const auto n=f.points.size();
        // Preserve the kernel's incident-face summation order, including
        // invisible adjacent faces. Do not area-weight or average by layer.
        Vec3 polygon{};
        if (n>4) {
            Vec3 center{};
            for (auto p:f.points) center=add(center,mesh.points[p].position);
            center={center.x/float(n),center.y/float(n),center.z/float(n)};
            for (std::size_t k=1;k<n;++k)
                polygon=legacyNormalAdd(polygon,normal(center,mesh.points[f.points[k-1]].position,mesh.points[f.points[k]].position));
            polygon=normalize(polygon);
        }
        for (std::size_t k=0;k<n;++k) {
            auto p=f.points[k];
            auto v=n>4 ? polygon : normal(mesh.points[f.points[(k+n-1)%n]].position,
                                          mesh.points[p].position,mesh.points[f.points[(k+1)%n]].position);
            normals[p]=legacyNormalAdd(normals[p],v);
        }
    }
    for (auto &n:normals) n=normalize(n);
    return normals;
}
QByteArray buildSurfaceDisplay(const DocumentInput &d, const SubdivisionLevel &mesh) {
    if (!d.surfaceDisplay) return {};
    if (mesh.level != d.geometry.desiredLevel || mesh.curves.size() != d.curveIds.size())
        throw std::runtime_error("Surface display mesh does not match input");
    std::vector<std::vector<std::size_t>> children(d.faceIds.size());
    for (std::size_t i=0;i<mesh.faces.size();++i) children.at(mesh.faces[i].rootControlFace).push_back(i);
    auto normals=(d.showNormals || !d.flows.empty()) ? surfacePointNormals(mesh) : std::vector<Vec3>{};
    std::vector<std::size_t> order(d.faceIds.size());
    std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){return d.faceIds[a]<d.faceIds[b];});
    QJsonArray curves;
    QByteArray triangles="[",vectors="[",analysisPoints="[",analysisFaces="[";
    std::size_t triangleCount=0,normalCount=0,faceCount=0;
    // Qt remains the numeric encoder. Reuse its exact float-to-double JSON
    // representation for shared vertices instead of encoding every triangle.
    std::vector<QByteArray> positions;positions.reserve(mesh.points.size());
    std::vector<QByteArray> normalText(d.showNormals?mesh.points.size():0);
    // Keep vertex identity and polygon corners for on-demand display analysis.
    // Do not compute curvature or zebra normals on ordinary document loads.
    for(const auto& p:mesh.points){
        auto encoded=QJsonDocument(coordinates(p.position)).toJson(QJsonDocument::Compact);
        if(!positions.empty())analysisPoints+=',';
        analysisPoints.append(encoded.constData(),encoded.size()-1);
        analysisPoints+=',';analysisPoints+=QByteArray::number(int(p.type));analysisPoints+=']';
        positions.push_back(std::move(encoded));
    }
    std::vector<std::size_t> seen(mesh.points.size(),d.faceIds.size());
    for (auto root:order) {
        for (auto index:children[root]) {
            const auto &f=mesh.faces[index];
            if(faceCount++)analysisFaces+=',';analysisFaces+='[';
            for(std::size_t i=0;i<f.points.size();++i){if(i)analysisFaces+=',';analysisFaces+=QByteArray::number(qulonglong(f.points[i]));}
            analysisFaces+=']';
            for (std::size_t k=2;k<f.points.size();++k) {
                if (triangleCount>=5000000) throw std::runtime_error("Too many derived triangles");
                if(triangleCount++)triangles+=',';
                triangles+="{\"layer\":";triangles+=QByteArray::number(d.layerIds.at(f.layer));triangles+=",\"p\":[";
                triangles+=positions[f.points[0]];triangles+=',';triangles+=positions[f.points[k-1]];
                triangles+=',';triangles+=positions[f.points[k]];triangles+="]}";
            }
            if (d.showNormals && mesh.layers.at(f.layer).surfaceVisible)
                for (auto p:f.points) if (seen[p]!=root) {
                    seen[p]=root;
                    if (normalCount>=4000000) throw std::runtime_error("Too many surface normals");
                    if(normalText[p].isEmpty())normalText[p]=QJsonDocument(coordinates(normals[p])).toJson(QJsonDocument::Compact);
                    if(normalCount++)vectors+=',';
                    vectors+="{\"face\":";vectors+=QByteArray::number(d.faceIds[root]);vectors+=",\"n\":";
                    vectors+=normalText[p];vectors+=",\"p\":";vectors+=positions[p];vectors+='}';
                }
        }
    }
    std::size_t count=0;
    for (std::size_t i=0;i<mesh.curves.size();++i) {
        const auto &c=mesh.curves[i];count+=c.points.size();
        if (count>1000000) throw std::runtime_error("Too many auxiliary spline points");
        QJsonArray points,flags;
        for (auto p:c.points) points.append(coordinates(mesh.points[p].position));
        for (bool k:c.knuckles) flags.append(k);
        curves.append(QJsonObject{{"id",d.curveIds[i]},
            {"spline",QJsonObject{{"points",points},{"knuckles",flags}}}});
    }
    triangles+=']';vectors+=']';analysisPoints+=']';analysisFaces+=']';
    // Preserve QJsonObject's key order and the unchanged public wire format.
    QByteArray result="{\"analysis_topology\":{\"faces\":";result+=analysisFaces;
    result+=",\"points\":";result+=analysisPoints;result+="},\"child_faces\":";
    result+=QByteArray::number(qulonglong(mesh.faces.size()));result+=",\"control_curves\":";
    result+=QJsonDocument(curves).toJson(QJsonDocument::Compact);result+=",\"flowlines\":";
    result+=QJsonDocument(buildFlowlines(d,mesh,normals)).toJson(QJsonDocument::Compact);
    result+=",\"format\":\"FreeShipSurfaceDisplay\",\"surface_normals\":";result+=vectors;
    result+=",\"triangles\":";result+=triangles;result+=",\"version\":1}";return result;
}
}
