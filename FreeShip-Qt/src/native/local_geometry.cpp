// Exact derived geometry cache. GPL-2.0-or-later; see COPYING.
#include "local_geometry.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fs::native_geometry {
namespace {
constexpr qsizetype budget = 64 * 1024 * 1024;
constexpr char magic[] = "FSGEO002";
QByteArray digest(const QByteArray& b) { return QCryptographicHash::hash(b, QCryptographicHash::Blake2b_256); }
// Local cache format is tied to the executable hash, including ABI/endian.
// Counts are checked against remaining bytes before any allocation.
struct Writer {
    QByteArray data;
    void bytes(const void* p, qsizetype size) {
        if (size < 0 || size > budget - data.size()) throw std::length_error("Local geometry cache budget");
        data.append(static_cast<const char*>(p), size);
    }
    template<class T> void pod(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>); bytes(&v, sizeof(v));
    }
    void count(std::size_t n) {
        if (n > std::numeric_limits<quint32>::max()) throw std::length_error("Cache count overflow");
        pod(quint32(n));
    }
    template<class T> void list(const std::vector<T>& a) {
        count(a.size()); bytes(a.data(), qsizetype(a.size() * sizeof(T)));
    }
    void ids(const std::vector<std::size_t>& a) { count(a.size()); for (auto v:a) count(v); }
};
struct Reader {
    const QByteArray& data;
    qsizetype at = 0;
    void bytes(void* p, qsizetype size) {
        if (size < 0 || size > data.size() - at) throw std::runtime_error("Truncated geometry cache");
        if (size) std::memcpy(p, data.constData() + at, size);
        at += size;
    }
    template<class T> T pod() { T v{}; bytes(&v, sizeof(v)); return v; }
    std::size_t count(qsizetype minimum = 1) {
        const auto n = pod<quint32>();
        if (qsizetype(n) > (data.size() - at) / minimum) throw std::runtime_error("Invalid geometry cache count");
        return n;
    }
    template<class T> std::vector<T> list() {
        std::vector<T> v(count(sizeof(T))); bytes(v.data(), qsizetype(v.size() * sizeof(T))); return v;
    }
    std::vector<std::size_t> ids() {
        std::vector<std::size_t> v(count(4)); for (auto& i:v) i=pod<quint32>(); return v;
    }
    bool flag() { const auto n=pod<quint8>(); if(n>1)throw std::runtime_error("Invalid cache flag");return n!=0; }
};
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
void require(bool b) { if (!b) throw std::runtime_error("Invalid geometry cache reference"); }
QByteArray topologyKey(const GeometryInput& g, const QByteArray& engine) {
    Writer w;w.bytes(engine.data(),engine.size());w.pod(g.subdivisionMode);w.pod(g.precision);w.pod(g.desiredLevel);
    w.count(g.points.size());for(const auto& p:g.points)w.pod(p.type);
    w.count(g.edges.size());for(const auto& e:g.edges){w.count(e.points[0]);w.count(e.points[1]);w.pod(quint8(e.crease));w.pod(e.curve);}
    w.count(g.faces.size());for(const auto& f:g.faces){w.ids(f.points);w.count(f.layer);}
    w.count(g.curves.size());for(const auto& c:g.curves){w.ids(c.points);w.pod(quint8(c.visible));}
    // Include all layer metadata, even flags only used by downstream displays.
    w.count(g.layers.size());for(const auto& l:g.layers){
        for(bool flag:{l.symmetric,l.surfaceVisible,l.controlNetVisible,l.useForIntersections,l.useInHydrostatics,l.showInLinesplan,l.developable})w.pod(quint8(flag));
        w.pod(l.color);w.pod(l.alphaBlend);w.pod(l.materialDensity);w.pod(l.thickness);w.pod(l.layerId);
        w.count(l.name.size());w.bytes(l.name.data(),qsizetype(l.name.size()));
    }
    return digest(w.data);
}
void writeTriangle(Writer& w, const FillTriangle& t) {
    w.pod(t.a);w.pod(t.b);w.pod(t.c);w.pod(t.center);w.pod(t.color);w.pod(quint8(t.symmetric));w.pod(t.shades);
}
FillTriangle readTriangle(Reader& r) {
    FillTriangle t;t.a=r.pod<Vec3>();t.b=r.pod<Vec3>();t.c=r.pod<Vec3>();t.center=r.pod<Vec3>();
    t.color=r.pod<quint32>();t.symmetric=r.flag();t.shades=r.pod<decltype(t.shades)>();
    require(finite(t.a)&&finite(t.b)&&finite(t.c)&&finite(t.center));return t;
}
QByteArray encode(const SubdivisionProgram& p, const QByteArray& key, const QByteArray& fillKey, const std::vector<RootFill>& fills) {
    Writer w;w.bytes(key.data(),key.size());w.list(p.operations);w.list(p.controls);w.list(p.outputs);w.list(p.boundOrder);
    const auto& m=p.mesh;w.pod(m.level);w.count(m.points.size());for(const auto& v:m.points){w.pod(v.position);w.pod(v.type);}
    w.count(m.faces.size());for(const auto& f:m.faces){w.ids(f.points);w.count(f.rootControlFace);w.pod(f.childIndex);w.count(f.layer);}
    w.count(m.edges.size());for(const auto& e:m.edges){w.count(e.points[0]);w.count(e.points[1]);w.pod(quint8(e.crease));w.pod(e.curve);w.pod(quint8(e.controlDescendant));w.ids(e.faces);w.pod(quint8(e.reversed));}
    w.count(m.curves.size());for(const auto& c:m.curves){w.ids(c.points);w.pod(quint8(c.visible));w.pod(quint8(c.freeStanding));w.count(c.knuckles.size());for(bool b:c.knuckles)w.pod(quint8(b));}
    w.count(m.looseControlPoints.size());for(const auto& c:m.looseControlPoints){w.count(c.controlPoint);w.pod(c.position);w.pod(c.type);}
    w.count(m.looseControlEdges.size());for(const auto& e:m.looseControlEdges){w.count(e.controlEdge);w.count(e.points[0]);w.count(e.points[1]);w.pod(quint8(e.crease));w.pod(e.curve);}
    w.count(fillKey.size());w.bytes(fillKey.data(),fillKey.size());w.count(fills.size());
    for(const auto& f:fills)for(const auto* a:{&f.below,&f.above}){w.count(a->size());for(const auto& t:*a)writeTriangle(w,t);}
    return w.data;
}
void decode(const QByteArray& data, const GeometryInput& g, const QByteArray& key, SubdivisionProgram& p, QByteArray& fillKey, std::vector<RootFill>& fills) {
    Reader r{data};QByteArray stored(32,0);r.bytes(stored.data(),32);require(stored==key);
    p.operations=r.list<SubdivisionOperation>();p.controls=r.list<quint32>();p.outputs=r.list<quint32>();p.boundOrder=r.list<quint32>();
    require(p.controls.size()==g.points.size()&&p.operations.size()>p.controls.size());
    require(p.operations[0].code==SubdivisionOperation::Zero&&p.operations[0].value.x==0&&p.operations[0].value.y==0&&p.operations[0].value.z==0);
    for(std::size_t i=0;i<p.controls.size();++i)require(p.controls[i]==i+1&&p.operations[i+1].code==SubdivisionOperation::Input);
    for(std::size_t i=0;i<p.operations.size();++i){const auto& o=p.operations[i];require(finite(o.value)&&std::isfinite(o.factor));
        if(i>p.controls.size())require(o.code>=SubdivisionOperation::Scale&&o.code<=SubdivisionOperation::Add&&o.a<i&&o.b<i&&(o.code!=SubdivisionOperation::Divide||o.factor!=0));}
    auto& m=p.mesh;m.level=r.pod<unsigned>();require(m.level==g.desiredLevel);m.layers=g.layers;
    m.points.resize(r.count(16));for(auto& v:m.points){v.position=r.pod<Vec3>();v.type=r.pod<VertexType>();require(finite(v.position)&&int(v.type)>=0&&int(v.type)<=3);}
    require(p.outputs.size()==m.points.size());for(auto i:p.outputs)require(i<p.operations.size());for(auto i:p.boundOrder)require(i<m.points.size());
    m.faces.resize(r.count(16));std::size_t prior=0;
    for(auto& f:m.faces){f.points=r.ids();f.rootControlFace=r.pod<quint32>();f.childIndex=r.pod<int>();f.layer=r.pod<quint32>();
        require(f.points.size()>=3&&f.rootControlFace<g.faces.size()&&f.rootControlFace>=prior&&f.layer==g.faces[f.rootControlFace].layer);prior=f.rootControlFace;for(auto i:f.points)require(i<m.points.size());}
    m.edges.resize(r.count(19));for(auto& e:m.edges){e.points={r.pod<quint32>(),r.pod<quint32>()};e.crease=r.flag();e.curve=r.pod<int>();e.controlDescendant=r.flag();e.faces=r.ids();e.reversed=r.flag();
        require(e.points[0]<m.points.size()&&e.points[1]<m.points.size()&&e.curve>=-1&&(e.curve<0||std::size_t(e.curve)<g.curves.size()));for(auto f:e.faces)require(f<m.faces.size());}
    const auto curveCount=r.count(10);require(curveCount==g.curves.size());m.curves.resize(curveCount);
    for(auto& c:m.curves){c.points=r.ids();c.visible=r.flag();c.freeStanding=r.flag();const auto knuckles=r.count();require(knuckles==c.points.size());c.knuckles.resize(knuckles);for(std::size_t i=0;i<c.knuckles.size();++i)c.knuckles[i]=r.flag();for(auto i:c.points)require(i<m.points.size());}
    m.looseControlPoints.resize(r.count(20));for(auto& c:m.looseControlPoints){c.controlPoint=r.pod<quint32>();c.position=r.pod<Vec3>();c.type=r.pod<VertexType>();require(c.controlPoint<g.points.size()&&c.type==g.points[c.controlPoint].type&&finite(c.position));}
    m.looseControlEdges.resize(r.count(17));for(auto& e:m.looseControlEdges){e.controlEdge=r.pod<quint32>();e.points={r.pod<quint32>(),r.pod<quint32>()};e.crease=r.flag();e.curve=r.pod<int>();require(e.controlEdge<g.edges.size()&&e.points==g.edges[e.controlEdge].points);}
    fillKey.resize(qsizetype(r.count()));r.bytes(fillKey.data(),fillKey.size());require(fillKey.size()==0||fillKey.size()==32);
    const auto fillCount=r.count(8);require(fillCount==0||fillCount==g.faces.size());fills.resize(fillCount);
    for(auto& f:fills)for(auto* a:{&f.below,&f.above}){a->resize(r.count(65));for(auto& t:*a)t=readTriangle(r);}
    require(r.at==data.size());
}
} // namespace
LocalGeometry::LocalGeometry(const GeometryInput& g, const QString& directory, const QByteArray& engine):level_(g.desiredLevel) {
    QElapsedTimer t;t.start();
    if(!directory.isEmpty())try{
        QDir dir(directory);const auto name=dir.filePath("local-geometry.fscache");
        if(!QFileInfo(directory).isSymLink()&&!QFileInfo(name).isSymLink()&&dir.mkpath(".")){
            key_=topologyKey(g,engine);path_=name;QFile f(path_);
            if(f.size()<=budget+40&&f.open(QIODevice::ReadOnly)){
                const auto data=f.readAll();require(data.size()>=40&&data.left(8)==QByteArray(magic,8));
                const auto payload=data.mid(40);require(data.mid(8,32)==digest(payload));
                decode(payload,g,key_,program_,fillKey,fills);loadedFillKey_=fillKey;hit=true;
            }
        }
    }catch(const std::exception&){program_={};fillKey.clear();fills.clear();hit=false;}
    loadMs_=t.nsecsElapsed()/1e6;t.restart();
    if(!path_.isEmpty()){
        if(hit){try{program_.update(g);}catch(const std::exception&){program_={};fillKey.clear();fills.clear();hit=false;}}
        if(!hit)try{program_=compileSubdivision(g,g.desiredLevel);}catch(const std::length_error&){program_={};path_.clear();}
    }
    if(!active()){full_=std::make_unique<SubdivisionSurface>(g);full_->level(g.desiredLevel);}
    computeMs_=t.nsecsElapsed()/1e6;
}
const SubdivisionLevel& LocalGeometry::mesh() const { return active()?program_.mesh:full_->level(level_); }
void LocalGeometry::save() {
    if(!active()||path_.isEmpty())return;
    // A validated snapshot is a sufficient base for exact incremental work;
    // it need not be rewritten for every small drag. Accumulate edits relative
    // to that snapshot, refreshing when more than a quarter of vertices/roots
    // change or the global fill datum changes. Never mix its old arithmetic
    // values with new fill records on disk.
    if(hit&&fillKey==loadedFillKey_&&program_.changedPoints<=program_.mesh.points.size()/4&&
       std::size_t(std::count(program_.changedRoots.begin(),program_.changedRoots.end(),true))<=program_.changedRoots.size()/4)return;
    QElapsedTimer t;t.start();
    try{
        if(QFileInfo(path_).isSymLink()||QFileInfo(QFileInfo(path_).absolutePath()).isSymLink())return;
        const auto payload=encode(program_,key_,fillKey,fills);QSaveFile file(path_);file.setDirectWriteFallback(false);
        if(file.open(QIODevice::WriteOnly)&&file.write(magic,8)==8&&file.write(digest(payload))==32&&file.write(payload)==payload.size())file.commit();
    }catch(const std::exception&){/* Cache failure never prevents publication of complete output. */}
    saveMs_=t.nsecsElapsed()/1e6;
}
QJsonObject LocalGeometry::diagnostics() const {
    return {{"local_geometry_cache_hit",hit},{"local_geometry_cache_active",active()},
        {"local_geometry_load_ms",loadMs_},{"local_geometry_compute_ms",computeMs_},{"local_geometry_save_ms",saveMs_},
        {"subdivision_operations",double(program_.operations.size())},{"subdivision_evaluated",double(program_.evaluated)},
        {"subdivision_changed_points",double(program_.changedPoints)},
        {"subdivision_changed_roots",int(std::count(program_.changedRoots.begin(),program_.changedRoots.end(),true))}};
}
} // namespace fs::native_geometry
