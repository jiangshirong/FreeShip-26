// GPL-2.0-or-later. Field order/defaults from FreeSubdivisionLayer.LoadBinary,
// FreeProjectSettings.LoadBinary/Clear, FreeSubdivisionControlCurve.LoadBinary
// and FreeShip.Precision. This reads only previously validated FBM block spans.
#include "geometry_input.h"
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>

namespace fs::document {
namespace {
class Fields {
    const QByteArray &data_; qsizetype at_=0,end_=0;
public:
    Fields(const Fbm &source,const char *name):data_(source.bytes()) {
        bool found=false;
        for (const auto &value:source.inspect()["blocks"].toArray()) {
            const auto block=value.toObject();
            if (block["name"]==name) { at_=block["offset"].toInteger(); end_=at_+block["size"].toInteger(); found=true; break; }
        }
        if (!found || at_<0 || end_<at_ || end_>data_.size()) throw std::invalid_argument("Missing validated FBM geometry block");
    }
    void skip(qsizetype size) {
        if (size<0 || size>end_-at_) throw std::invalid_argument("Truncated FBM geometry fields");
        at_+=size;
    }
    int integer() { const auto p=at_; skip(4); return qFromLittleEndian<qint32>(data_.constData()+p); }
    float scalar() { quint32 raw=quint32(integer()); float result; std::memcpy(&result,&raw,4); return result; }
    bool flag() { const auto p=at_; skip(1); return data_[p]!=0; }
    QByteArray string() { const int n=integer(); const auto p=at_; skip(n); return data_.mid(p,n); }
};
using Key=std::pair<int,int>;
Key key(int a,int b) { return std::minmax(a,b); }
}

DocumentGeometrySettings documentGeometrySettings(const Fbm &source) {
    DocumentGeometrySettings settings; const int v=source.version(); Fields r(source,"project_settings");
    if (v>=2) {
        r.string();r.string();settings.length=r.scalar();settings.beam=r.scalar();settings.draft=r.scalar();r.flag();
        settings.density=r.scalar();settings.appendage=r.scalar();r.skip(1+4);settings.imperial=r.integer()!=0;
        if (v>=6) { settings.defaultSplit=r.flag();settings.split=r.scalar(); }
        if (v>=7) r.flag();
        r.string();r.string();
    }
    if (v>=16) { settings.actualCoefficients=r.integer()!=0; if (r.flag()) { r.skip(8);r.skip(r.integer()); } }
    if (v>=18) settings.simplifyIntersections=r.flag();
    if (settings.defaultSplit) settings.split=settings.length/2;
    return settings;
}

native_geometry::GeometryInput documentGeometry(const Fbm &source) {
    using namespace native_geometry;
    GeometryInput input; input.precision=source.precision();input.desiredLevel=unsigned(input.precision+1);
    const auto net=source.controlNet();input.points.reserve(net.points.size());input.edges.reserve(net.edges.size());
    for (const auto &point:net.points) { const auto &p=point.value;input.points.push_back({{p.x,p.y,p.z},VertexType(p.vertexType)}); }
    Fields layers(source,"layers");const int count=layers.integer();
    for (int i=0;i<count;++i) {
        LayerInput l;l.name=layers.string().toStdString();l.layerId=layers.integer();l.color=layers.integer();
        l.surfaceVisible=layers.flag();l.symmetric=layers.flag();l.developable=layers.flag();
        if (source.version()>=9) { l.useForIntersections=layers.flag();l.useInHydrostatics=layers.flag(); }
        if (source.version()>=11) { l.materialDensity=layers.scalar();l.thickness=layers.scalar(); }
        if (source.version()>=15) l.showInLinesplan=layers.flag();
        if (source.version()>=21) l.alphaBlend=unsigned(layers.integer());
        input.layers.push_back(std::move(l));
    }
    if (input.layers.empty()) { LayerInput l;l.color=0xffff00;l.layerId=0;input.layers.push_back(l); }
    std::map<Key,size_t> edges;
    for (const auto &e:net.edges) {
        if (!edges.emplace(key(e.a,e.b),input.edges.size()).second) throw std::invalid_argument("Duplicate FBM control edge");
        input.edges.push_back({{size_t(e.a),size_t(e.b)},e.crease,-1});
    }
    for (const auto &f:net.faces) {
        ControlFaceInput face;face.layer=f.layer<0 ? 0 : size_t(f.layer);
        for (int p:f.points) face.points.push_back(size_t(p));
        input.faces.push_back(std::move(face));
    }
    bool visible=false;
    if (source.version()>=12) { Fields r(source,"visibility");r.skip(4+10+4);visible=r.flag(); }
    for (size_t c=0;c<net.curves.size();++c) {
        ControlCurveInput curve;curve.visible=visible;
        const auto &points=net.curves[c].points;
        for (size_t i=0;i<points.size();++i) {
            curve.points.push_back(size_t(points[i]));
            if (i) { const auto edge=edges.find(key(points[i-1],points[i]));if (edge!=edges.end()) input.edges[edge->second].curve=int(c); }
        }
        input.curves.push_back(std::move(curve));
    }
    return input;
}
}
