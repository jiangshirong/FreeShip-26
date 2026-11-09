// GPL-2.0-or-later. Adapted from FREE!ship (Martijn van Engeland, 2005),
// Plus (Timoshenko V.F., Mark Malakanov), FreeHydrostaticCalc, FreeIntersection
// and FreeSubdivisionLayer. The bounded equilibrium solver follows the 2026
// HydrostaticsEngine adapter, not the original unchecked Balance routine.
#include "hydrostatics.h"
#include "geometry_input.h"
#include "fbm_text.h"
#include "native/sections.h"
#include <clipper2/clipper.engine.h>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QStringList>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <limits>
#include <stdexcept>

namespace fs::document {
namespace {
using namespace native_geometry;
constexpr double Pi=3.1415926535897932384626433832795;
double radians(double degrees) { return degrees*(Pi/180); }
[[noreturn]] void bad(const char *message) { throw std::runtime_error(message); }
struct Cancelled:std::runtime_error { using std::runtime_error::runtime_error; };
struct Job {
    QString cancelFile;
    QElapsedTimer clock;
    int evaluations=0,sections=0;
    explicit Job(QString path):cancelFile(std::move(path)) { clock.start(); }
    void check() const {
        if (!cancelFile.isEmpty() && QFileInfo::exists(cancelFile)) throw Cancelled("计算已取消。");
        if (clock.elapsed()>120000) throw Cancelled("计算超过两分钟，已停止；请减少工况数量。");
    }
};
double number(const QJsonObject &o,const char *key,double low,double high) {
    auto v=o[key];double n=v.toDouble();
    if (!v.isDouble() || !std::isfinite(n) || n<low || n>high) bad("缺少数值或数值超出允许范围。");
    return n;
}
std::vector<double> numbers(const QJsonObject &o,const char *key,double low,double high,int limit) {
    if (!o[key].isArray()) bad("缺少计算列表。");
    auto a=o[key].toArray();if (a.isEmpty() || a.size()>limit) bad("计算列表长度超出范围。");
    std::vector<double> out;
    for (auto v:a) {
        double n=v.toDouble();
        if (!v.isDouble() || !std::isfinite(n) || n<low || n>high || (!out.empty() && n<=out.back())) bad("计算列表必须在允许范围内严格递增。");
        out.push_back(n);
    }
    return out;
}
double finite(double n) { if (!std::isfinite(n)) bad("计算结果不是有限数，请检查模型。");return n; }
QJsonArray json(Vec3 p) { return {finite(p.x),finite(p.y),finite(p.z)}; }
QJsonArray json(const Plane &p) { QJsonArray a;for (float n:p.coefficients()) a.append(finite(n));return a; }
void bounds(Vec3 p,Vec3 &low,Vec3 &high) {
    low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
    high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
}
float distance(Vec3 a,Vec3 b) { auto x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z); }
struct Read {
    QByteArray bytes;qsizetype at=0;
    QByteArray take(qsizetype n) { if (n<0 || n>bytes.size()-at) bad("Truncated hydrostatic settings");auto b=bytes.mid(at,n);at+=n;return b; }
    int integer() { auto b=take(4);return qFromLittleEndian<qint32>(b.constData()); }
    float real() { quint32 bits=quint32(integer());float n;std::memcpy(&n,&bits,4);return n; }
    bool flag() { return take(1)[0]!=0; }
    QByteArray string() { return take(integer()); }
    std::vector<float> list() { int n=integer();if (n<0 || n>Fbm::MaximumCollectionItems) bad("Invalid hydrostatic list");std::vector<float> a;for(int i=0;i<n;++i)a.push_back(real());return a; }
};
struct Settings {
    DocumentGeometrySettings geometry;
    QByteArray name;
    float start=0,end=1,step=.1f,trim=0,vcg=1,minMass=0,maxMass=1,massStep=.1f;
    bool freeTrim=true,increments=true,auxiliary=false;
    std::vector<float> angles{0,2,5,10,15,20,30,40,50,60,70,80,90,100,110,120,130,140,150},masses,trims{0};
    std::vector<Plane> stations,waterlines;
    explicit Settings(const Fbm &source):geometry(documentGeometrySettings(source)) {
        Read r{source.blockBytes("project_settings")};const int v=source.version();
        if(v>=2) { name=r.string();r.string();r.take(30);if(v>=6)r.take(5);if(v>=7)r.take(1);r.string();r.string(); }
        if(v>=16) { r.take(4);if(r.flag()) {r.take(8);r.take(r.integer());} }
        if(v>=18)r.take(1);
        if(v>=20) { start=r.real();end=r.real();step=r.real();trim=r.real();masses=r.list();minMass=r.real();maxMass=r.real();massStep=r.real();increments=r.flag();angles=r.list();trims=r.list();freeTrim=r.flag();vcg=r.real(); }
        if(v>=37)r.flag();if(v>=39)auxiliary=r.flag();
        const auto readSections=[&](const char* block,std::vector<Plane>& planes){
        r={source.blockBytes(block)};const int count=r.integer();
        for(int i=0;i<count;++i) {
            const int kind=r.integer();if(v>=11)r.flag();
            const float a=r.real(),b=r.real(),c=r.real(),d=r.real();planes.emplace_back(a,b,c,d);
            r.flag();const int splines=r.integer();const int dimensions=v>=6 && kind>=1 && kind<=3 ? 2:3;
            for(int j=0;j<splines;++j)r.take(qsizetype(r.integer())*(dimensions*4+1));
        }};
        readSections("stations",stations);readSections("waterlines",waterlines);
    }
};
struct Rotation {
    float ch,sh,ct,st,keel;
    Rotation(float heel,float trim,float length,float z):keel(z) {
        // Keep the stored Single trim angle and direction convention.
        const float trimAngle=float(std::atan((-trim*std::cos(radians(heel)))/length)*180/Pi);
        ch=float(std::cos(radians(-heel)));sh=float(std::sin(radians(-heel)));
        ct=float(std::cos(radians(-trimAngle)));st=float(std::sin(radians(-trimAngle)));
    }
    Vec3 operator()(Vec3 p) const {
        p.z-=keel;
        return {p.x*ct+(p.y*sh+p.z*ch)*st,p.y*ch-p.z*sh,-p.x*st+(p.y*sh+p.z*ch)*ct};
    }
};
enum class Section { Station,Buttock,Waterline };
struct Area { float value=0;Vec3 cog{};float ix=0,iy=0; };
// Double intermediates for oriented polygon integrals. Input vertices remain
// the exact stored subdivision mesh; no spline fitting or resampling is used.
struct CutPoint { double x=0,y=0,z=0; };
struct CutSegment { CutPoint a,b; };
CutPoint mix(CutPoint a,CutPoint b,double t) {return {a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)};}
double evaluate(const Plane& p,CutPoint v) {const auto& n=p.coefficients();return n[0]*v.x+n[1]*v.y+n[2]*v.z+n[3];}
struct Volume {
    float value=0,mass=0,wetted=0,draft=0;
    Vec3 buoyancy{},subMin{},subMax{},wlMin{},wlMax{};
};
}

struct Hydrostatics::Impl {
    Settings settings;
    GeometryInput input;
    SubdivisionSurface surface;
    const SubdivisionLevel &mesh;
    SectionIndex index;
    std::vector<size_t> faces;
    struct Vertex { Vec3 point;bool leak; };
    std::vector<Vertex> vertices;
    Vec3 minimum{},maximum{};
    float keel=0;
    bool asymmetric=false,prepared=false;
    QString encoding;
    double buildMs=0;
    std::map<std::array<float,4>,std::vector<CutSegment>> fullCuts;
    size_t fullCutSegments=0;
    std::map<std::pair<std::array<float,4>,bool>,Area> projections;
    explicit Impl(const Fbm &source,const QJsonObject &bindings):settings(source),input(documentGeometry(source)),surface(input),mesh(surface.level(input.desiredLevel)),index(input,mesh) {
        encoding=bindings.value("fbm_encoding").toString("cp1252");
        // Layer / control-face / child order is part of Single accumulation.
        std::map<qint64,size_t> ids;auto faceIds=bindings["faces"].toArray(),orders=bindings["layer_faces"].toArray();
        for(qsizetype i=0;i<faceIds.size();++i)ids.emplace(faceIds[i].toInteger(),size_t(i));
        std::vector<std::vector<size_t>> children(input.faces.size());
        for(size_t f=0;f<mesh.faces.size();++f)children[mesh.faces[f].rootControlFace].push_back(f);
        for(size_t l=0;l<input.layers.size();++l) if(input.layers[l].useInHydrostatics) {
            const auto firstFace=faces.size();
            if(l<size_t(orders.size())) {
                for(auto id:orders[qsizetype(l)].toArray()) {
                    auto found=ids.find(id.toInteger());if(found==ids.end())bad("Invalid hydrostatic face order");
                    const auto &child=children[found->second];faces.insert(faces.end(),child.begin(),child.end());
                }
            } else for(size_t f=0;f<mesh.faces.size();++f)if(mesh.faces[f].layer==l)faces.push_back(f);
            if(faces.size()>firstFace && !input.layers[l].symmetric)asymmetric=true;
        }
        bool first=true;
        for(auto f:faces)for(auto p:mesh.faces[f].points) {
            auto v=mesh.points[p].position;
            if(first) {minimum=maximum=v;first=false;} else bounds(v,minimum,maximum);
        }
        keel=first?mesh.minimum.z:minimum.z;
    }
    void prepare(Job &job) {
        if(prepared)return;
        std::vector<unsigned> masks(mesh.points.size(),0),faceMasks(mesh.faces.size(),0);
        for(auto f:faces) {
            unsigned mask=input.layers[mesh.faces[f].layer].symmetric?3:1;faceMasks[f]=mask;
            for(auto p:mesh.faces[f].points)masks[p]|=mask;
        }
        std::vector<std::array<bool,2>> leaks(mesh.points.size(),{false,false});
        for(const auto &e:mesh.edges) {
            int count[2]{},orientation[2]{};
            for(auto f:e.faces){
                int direction=0;
                if(asymmetric && faceMasks[f]){const auto& loop=mesh.faces[f].points;
                    for(size_t i=0;i<loop.size();++i){auto a=loop[i],b=loop[(i+1)%loop.size()];
                        if(a==e.points[0]&&b==e.points[1])direction=1;
                        if(a==e.points[1]&&b==e.points[0])direction=-1;
                    }
                }
                for(int side=0;side<2;++side)if(faceMasks[f]&(1u<<side)){++count[side];orientation[side]+=direction;}
            }
            bool seam=std::abs(mesh.points[e.points[0]].position.y)<=1e-4 && std::abs(mesh.points[e.points[1]].position.y)<=1e-4 && count[0]==1 && count[1]==1;
            for(int side=0;side<2;++side) {
                if(count[side]>2)bad("参与计算的船体有非流形边，无法确定封闭体积。");
                if(asymmetric && count[side]==2 && orientation[side]!=0)bad("参与静水力计算的相邻面朝向不一致。");
                if(count[side]==1 && !seam)for(auto p:e.points)leaks[p][side]=true;
            }
        }
        std::vector<Vertex> result;
        for(size_t p=0;p<masks.size();++p) {
            if(p%1024==0)job.check();
            for(int side=0;side<2;++side)if(masks[p]&(1u<<side)) {
                auto point=mesh.points[p].position;if(side)point.y=-point.y;result.push_back({point,leaks[p][side]});
            }
        }
        vertices=std::move(result);prepared=true;
    }
    void projectedBounds(Vec3 normal,double &lo,double &hi,double &leak,Job &job) {
        prepare(job);lo=1e300;hi=-1e300;leak=1e300;
        for(const auto &v:vertices) {
            double n=double(v.point.x)*normal.x+double(v.point.y)*normal.y+double(v.point.z)*normal.z;
            lo=std::min(lo,n);hi=std::max(hi,n);if(v.leak)leak=std::min(leak,n);
        }
        if(hi<=lo)bad("参与静水力计算的船体没有有效体积范围。");
    }
    Plane waterline(float draft,float heel,float trim) const {
        const auto length=settings.geometry.length;
        Vec3 a{0,0,float(keel+(draft-.5f*trim))},b{length,0,float(keel+(draft+.5f*trim))};
        Vec3 c{length,float(std::cos(radians(-heel))),float(keel+(draft+.5f*trim)-std::sin(radians(-heel)))};
        return Plane::fromPoints(a,b,c);
    }
    Volume volume(const Plane &plane,float heel,float trim,bool detailed,Job &job) {
        job.check();++job.evaluations;
        prepare(job);
        for(const auto &v:vertices)if(v.leak && plane.evaluate(v.point)<-1e-7)bad("水线以下有漏点。");
        auto projected=plane.project({});if(!projected)bad("水线平面无效。");const Vec3 origin=*projected;
        Rotation rotate(heel,trim,settings.geometry.length,keel);
        Volume out;out.draft=1000;bool submerged=false,firstSub=true,firstWl=true;
        std::vector<Vec3> polygon;polygon.reserve(8);
        auto check=[&](Vec3 p,float side) {
            if(!detailed)return;
            p=rotate(p);
            if(firstSub) {out.subMin=out.subMax=p;firstSub=false;} else bounds(p,out.subMin,out.subMax);
            if(side>-1e-7 && side<1e-7) {
                if(firstWl) {out.wlMin=out.wlMax=p;firstWl=false;} else bounds(p,out.wlMin,out.wlMax);
            }
        };
        auto triangle=[&](Vec3 a,Vec3 b,Vec3 c) {
            a={a.x-origin.x,a.y-origin.y,a.z-origin.z};b={b.x-origin.x,b.y-origin.y,b.z-origin.z};c={c.x-origin.x,c.y-origin.y,c.z-origin.z};
            Vec3 center{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
            const float v=(a.z*(b.x*c.y-b.y*c.x)+a.y*(b.z*c.x-b.x*c.z)+a.x*(b.y*c.z-b.z*c.y))/6;
            if(v!=0) {
                Vec3 moment{.75f*center.x*v,.75f*center.y*v,.75f*center.z*v};
                out.value+=v;out.buoyancy.x+=moment.x;out.buoyancy.y+=moment.y;out.buoyancy.z+=moment.z;
            }
            if(detailed) {
                float x=.5f*((a.y-b.y)*(a.z+b.z)+(b.y-c.y)*(b.z+c.z)+(c.y-a.y)*(c.z+a.z));
                float y=.5f*((a.z-b.z)*(a.x+b.x)+(b.z-c.z)*(b.x+c.x)+(c.z-a.z)*(c.x+a.x));
                float z=.5f*((a.x-b.x)*(a.y+b.y)+(b.x-c.x)*(b.y+c.y)+(c.x-a.x)*(c.y+a.y));
                out.wetted+=std::sqrt(x*x+y*y+z*z);
            }
        };
        size_t count=0;
        for(auto f:faces) {
            if(++count%1024==0)job.check();
            const auto &face=mesh.faces[f];
            for(int mirror=0;mirror<(input.layers[face.layer].symmetric?2:1);++mirror) {
              // A warped quad is not a plane. Clip its fixed mesh triangles
              // before integration so volume and section area refer to the
              // same surface. Preserve the established symmetric path exactly.
              for(size_t piece=0;piece<(asymmetric?face.points.size()-2:1);++piece){
                const size_t size=asymmetric?3:face.points.size();
                auto point=[&](size_t i) {auto p=mesh.points[face.points[asymmetric&&i>0?piece+i:i]].position;if(mirror)p.y=-p.y;return p;};
                polygon.clear();auto a=point(size-1);float sa=plane.evaluate(a);
                for(size_t i=0;i<size;++i) {
                    auto b=point(i);float sb=plane.evaluate(b);
                    if((sa<-1e-7 && sb>1e-7)||(sa>1e-7 && sb<-1e-7)) {
                        float t=-sa/(sb-sa);Vec3 p{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)};
                        polygon.push_back(p);check(p,0);
                    }
                    if(sb<=1e-7) {out.draft=std::min(out.draft,sb);polygon.push_back(b);check(b,sb);}
                    a=b;sa=sb;
                }
                if(polygon.size()>2)submerged=true;
                for(size_t i=2;i<polygon.size();++i)if(mirror)triangle(polygon[0],polygon[i],polygon[i-1]);else triangle(polygon[0],polygon[i-1],polygon[i]);
              }
            }
        }
        out.draft=submerged?-out.draft:0;
        if(out.value!=0)out.buoyancy=rotate({origin.x+out.buoyancy.x/out.value,origin.y+out.buoyancy.y/out.value,origin.z+out.buoyancy.z/out.value});
        const auto &s=settings.geometry;
        out.mass=out.value*s.appendage*s.density;if(s.imperial)out.mass/=2240;
        out.value*=s.appendage;
        if(!std::isfinite(out.mass) || out.mass<0)bad("排水量无效，请检查面的朝向和船体闭合情况。");
        return out;
    }

    // Orthographic XZ silhouette of the actual clipped hydrostatic surface.
    // Neither a centreplane section nor a sum of face areas handles overlapping
    // hulls and through-holes. Union preserves both without screen rasterization.
    Area projectedArea(const Plane &water,bool above,Job &job) {
        using namespace Clipper2Lib;
        const auto key=std::make_pair(water.coefficients(),above);
        job.check();auto found=projections.find(key);if(found!=projections.end())return found->second;
        const double span=std::max(double(maximum.x)-minimum.x,double(maximum.z)-minimum.z);
        if(!(span>0))return {};
        // Recenter before quantization. A 1e12 grid is much finer than stored
        // Single geometry, while staying below Clipper's 1e15 accuracy limit.
        const double ox=(double(minimum.x)+maximum.x)*.5,oz=(double(minimum.z)+maximum.z)*.5,scale=1e12/span;
        auto signedDoubleArea=[](const Path64& path) {
            long double sum=0;auto a=path.back();
            for(const auto& b:path){sum+=static_cast<long double>(a.x)*b.y-static_cast<long double>(b.x)*a.y;a=b;}
            return sum;
        };
        auto unite=[&](Paths64 paths) {
            job.check();if(paths.empty())return paths;
            Clipper64 clipper;clipper.AddSubject(paths);Paths64 result;
            if(!clipper.Execute(ClipType::Union,FillRule::NonZero,result))bad("侧投影轮廓合并失败。");
            job.check();return result;
        };
        // Reduce bounded batches, then merge them as a balanced tree. This
        // avoids one enormous overlapping input and checks cancellation between
        // operations. Intermediate holes retain their opposite orientation.
        std::vector<Paths64> bins;Paths64 batch;
        auto flush=[&]() {
            auto paths=unite(std::move(batch));batch.clear();size_t level=0;
            while(!paths.empty()) {
                if(level==bins.size())bins.emplace_back();
                if(bins[level].empty()){bins[level]=std::move(paths);break;}
                for(auto& p:bins[level])paths.push_back(std::move(p));bins[level].clear();
                paths=unite(std::move(paths));++level;
            }
        };
        size_t count=0;
        for(auto f:faces) {
            if(++count%256==0)job.check();const auto& face=mesh.faces[f];
            for(int mirror=0;mirror<(input.layers[face.layer].symmetric?2:1);++mirror) {
                auto point=[&](size_t i){const auto v=mesh.points[face.points[i]].position;return CutPoint{v.x,mirror?-double(v.y):v.y,v.z};};
                for(size_t i=2;i<face.points.size();++i) {
                    const std::array<CutPoint,3> triangle{point(0),point(i-1),point(i)};
                    Path64 path;auto a=triangle.back();double da=evaluate(water,a)*(above?-1:1);
                    auto append=[&](CutPoint p){path.emplace_back(std::llround((p.x-ox)*scale),std::llround((p.z-oz)*scale));};
                    for(const auto b:triangle) {
                        const double db=evaluate(water,b)*(above?-1:1);
                        if((da<0 && db>0)||(da>0 && db<0))append(mix(a,b,da/(da-db)));
                        if(db<=0)append(b);a=b;da=db;
                    }
                    if(path.size()<3)continue;const auto area=signedDoubleArea(path);if(area==0)continue;
                    // Every projected face covers material, regardless of its
                    // normal. Opposite hull sides must not cancel each other.
                    if(area<0)std::reverse(path.begin(),path.end());batch.push_back(std::move(path));
                    if(batch.size()==512)flush();
                }
            }
        }
        if(!batch.empty())flush();Paths64 paths;
        for(auto& bin:bins)for(auto& path:bin)paths.push_back(std::move(path));
        paths=unite(std::move(paths));
        long double area2=0,mx=0,mz=0;
        for(const auto& path:paths) {
            auto a=path.back();
            for(const auto b:path) {
                const long double cross=static_cast<long double>(a.x)*b.y-static_cast<long double>(b.x)*a.y;
                area2+=cross;mx+=(static_cast<long double>(a.x)+b.x)*cross;mz+=(static_cast<long double>(a.y)+b.y)*cross;a=b;
            }
        }
        Area result;
        if(area2>0){result.value=float(area2/(2*scale*scale));result.cog={float(ox+mx/(3*area2*scale)),0,float(oz+mz/(3*area2*scale))};}
        if(projections.size()>=512)projections.clear();projections.emplace(key,result);return result;
    }

    std::vector<SectionCurve> curves(const Plane &plane,Job &job) {
        job.check();++job.sections;auto sections=index.intersect(plane,{true});auto result=std::move(sections.curves);
        // Match the established section policy also used by scene_builder:
        // only multi-chain cleanup warmed the old spline's chord-length cache.
        // Eager simplification of every new contour changes hydrostatic data.
        if(settings.geometry.simplifyIntersections && sections.connectedCurves.size()>1)for(auto &curve:result) {
            Spline spline(curve.points,curve.knuckles,curve.fragments);spline.simplify(2);
            curve.points=spline.points();curve.knuckles=spline.knuckles();
        }
        // Simplified/mirrored integration contours no longer map one-to-one to
        // the raw mesh segments used only for section tracing diagnostics.
        for(auto &curve:result)curve.sourceSegments.clear();
        return result;
    }
    const std::vector<CutSegment>& fullSection(const Plane& plane,Job& job) {
        const auto key=plane.coefficients();auto cached=fullCuts.find(key);
        if(cached!=fullCuts.end())return cached->second;
        job.check();prepare(job);++job.sections;
        double low=1e300,high=-1e300;
        for(const auto& v:vertices){double d=evaluate(plane,{v.point.x,v.point.y,v.point.z});low=std::min(low,d);high=std::max(high,d);}
        // At a coincident vertex/edge use the positive-side limit, except at
        // the extreme end where the interior limit preserves a flat end face.
        // Coplanar triangles contribute no cut. Adjacent triangles thus never
        // double-count a shared edge, even when several contours meet the plane.
        const bool zeroInside=!(high==0 && low<0);
        std::vector<CutSegment> cuts;
        size_t count=0;
        for(auto f:faces){
            if(++count%1024==0)job.check();const auto& face=mesh.faces[f];
            for(int mirror=0;mirror<(input.layers[face.layer].symmetric?2:1);++mirror){
                auto point=[&](size_t i){const auto v=mesh.points[face.points[i]].position;return CutPoint{v.x,mirror?-double(v.y):v.y,v.z};};
                for(size_t i=2;i<face.points.size();++i){
                    std::array<CutPoint,3> triangle{point(0),point(mirror?i:i-1),point(mirror?i-1:i)};
                    std::array<double,3> side;for(int j=0;j<3;++j)side[j]=evaluate(plane,triangle[j]);
                    if(side[0]==0 && side[1]==0 && side[2]==0)continue;
                    CutPoint enter,leave;bool hasEnter=false,hasLeave=false;
                    for(int j=0;j<3;++j){const int k=(j+1)%3;
                        const bool a=side[j]<0 || (side[j]==0 && zeroInside),b=side[k]<0 || (side[k]==0 && zeroInside);
                        if(a==b)continue;
                        const auto p=mix(triangle[j],triangle[k],side[j]/(side[j]-side[k]));
                        if(a){leave=p;hasLeave=true;}else{enter=p;hasEnter=true;}
                    }
                    if(hasEnter && hasLeave && (enter.x!=leave.x || enter.y!=leave.y || enter.z!=leave.z))cuts.push_back({enter,leave});
                }
            }
        }
        // A bounded per-revision cache also serves all Bonjean waterlines.
        if(fullCuts.size()>=1024 || fullCutSegments+cuts.size()>2000000){fullCuts.clear();fullCutSegments=0;}
        fullCutSegments+=cuts.size();return fullCuts.emplace(key,std::move(cuts)).first->second;
    }
    Area fullArea(Section kind,const Plane& section,const Plane& water,Job& job,float heel=0,float trim=0) {
        const auto& cuts=fullSection(section,job);job.check();Area out;
        const auto& n=section.coefficients();
        CutPoint origin{(double(minimum.x)+maximum.x)/2,(double(minimum.y)+maximum.y)/2,(double(minimum.z)+maximum.z)/2};
        if(kind==Section::Station)origin.x=-double(n[3])/n[0];
        else if(kind==Section::Buttock)origin.y=-double(n[3])/n[1];
        // For clipped vertical sections the missing closing edges lie on the
        // waterline. Place the fan origin on that line: their triangular area
        // and every moment are exactly zero. Disconnected loops and holes keep
        // their winding, so they add/subtract without artificial connectors.
        if(kind!=Section::Waterline){
            const auto& w=water.coefficients();
            const double u=kind==Section::Station?w[1]:w[0],v=w[2],den=u*u+v*v;
            if(den>0){double d=evaluate(water,origin)/den;if(kind==Section::Station)origin.y-=d*u;else origin.x-=d*u;origin.z-=d*v;}
        }
        Rotation rotate(heel,trim,settings.geometry.length,keel);
        auto xy=[&](CutPoint p)->std::pair<double,double>{
            if(kind==Section::Station)return {p.y-origin.y,p.z-origin.z};
            if(kind==Section::Buttock)return {p.x-origin.x,p.z-origin.z};
            p.x-=origin.x;p.y-=origin.y;p.z-=origin.z;
            return {p.x*rotate.ct+(p.y*rotate.sh+p.z*rotate.ch)*rotate.st,p.y*rotate.ch-p.z*rotate.sh};
        };
        double area=0,mx=0,my=0,xx=0,yy=0,absoluteArea=0;
        size_t count=0;
        for(auto cut:cuts){
            if(++count%1024==0)job.check();
            if(kind!=Section::Waterline){
                const double a=evaluate(water,cut.a),b=evaluate(water,cut.b);
                if(a>0 && b>0)continue;
                if((a>0)!=(b>0)){const auto p=mix(cut.a,cut.b,a/(a-b));if(a>0)cut.a=p;else cut.b=p;}
            }
            const auto a=xy(cut.a),b=xy(cut.b);
            const double cross=(a.first*b.second-b.first*a.second)*(kind==Section::Buttock?-1:1),da=cross/2;
            area+=da;absoluteArea+=std::abs(da);
            mx+=cross*(a.first+b.first)/6;my+=cross*(a.second+b.second)/6;
            xx+=cross*(a.second*a.second+a.second*b.second+b.second*b.second)/12;
            yy+=cross*(a.first*a.first+a.first*b.first+b.first*b.first)/12;
        }
        if(area< -absoluteArea*1e-12)bad("截面方向不一致，请检查参与静水力计算的面朝向。");
        if(area<=absoluteArea*1e-12)return out;
        const double x=mx/area,y=my/area;
        out.value=float(finite(area));
        if(kind==Section::Station)out.cog={float(origin.x),float(origin.y+x),float(origin.z+y)};
        else if(kind==Section::Buttock)out.cog={float(origin.x+x),float(origin.y),float(origin.z+y)};
        else{
            // The report uses the same horizontal, keel-relative frame as B.
            const double z=origin.z-keel;
            out.cog={float(origin.x*rotate.ct+(origin.y*rotate.sh+z*rotate.ch)*rotate.st+x),float(origin.y*rotate.ch-z*rotate.sh+y),
                float(-origin.x*rotate.st+(origin.y*rotate.sh+z*rotate.ch)*rotate.ct)};
            // origin may not lie on the waterplane; its Z is recovered from
            // an actual cut point, while translations in XY cancel above.
            if(!cuts.empty()){const auto p=cuts.front().a;out.cog.z=float(-p.x*rotate.st+(p.y*rotate.sh+(p.z-keel)*rotate.ch)*rotate.ct);}
            const double ix=xx-area*y*y,iy=yy-area*x*x;
            if(ix< -std::abs(xx)*1e-10 || iy< -std::abs(yy)*1e-10)bad("截面惯性矩无效，请检查面朝向和重叠船体。");
            out.ix=float(finite(std::max(0.,ix)));out.iy=float(finite(std::max(0.,iy)));
        }
        return out;
    }
    Area area(Section kind,const Plane &section,const Plane &water,Job &job,std::vector<SectionCurve> paths) {
        if(asymmetric)return fullArea(kind,section,water,job);
        job.check();
        if(kind!=Section::Buttock) {
            for(size_t i=paths.size();i>0;--i) {auto copy=paths[i-1];for(auto &p:copy.points)p.y=-p.y;paths.push_back(std::move(copy));}
            joinSectionCurves(paths,.05f,false);
        }
        auto xy=[&](Vec3 p)->std::pair<float,float> {
            if(kind==Section::Station)return {p.y,p.z};
            if(kind==Section::Buttock)return {p.x,p.z};
            return {p.x,p.y};
        };
        for(size_t i=paths.size();i>0;--i) {
            auto &path=paths[i-1];if(path.points.empty()) {paths.erase(paths.begin()+i-1);continue;}
            Spline s(path.points,path.knuckles,path.fragments);float signedArea=0;auto a=xy(path.points.back());
            for(int j=0;j<=500;++j) {auto b=xy(s.value(double(j)/500));signedArea+=.5f*(b.first+a.first)*(b.second-a.second);a=b;}
            if(std::abs(signedArea)<1e-4)paths.erase(paths.begin()+i-1);
            else if(signedArea<0) {std::reverse(path.points.begin(),path.points.end());std::reverse(path.knuckles.begin(),path.knuckles.end());}
        }
        Area result;Vec3 lastMoment{};
        for(auto path:paths) {
            if(kind==Section::Waterline && distance(path.points.front(),path.points.back())>=1e-4) {
                path.points.push_back(path.points.front());path.knuckles.push_back(false);path.knuckles[path.knuckles.size()-2]=true;
            }
            Spline spline(path.points,path.knuckles,500);std::vector<float> parameters{0,1};
            if(kind!=Section::Waterline) {IntersectionData cuts;if(spline.intersectPlane(water,cuts))parameters.insert(parameters.end(),cuts.parameters.begin(),cuts.parameters.end());}
            std::sort(parameters.begin(),parameters.end());float t1=0,sum=0,cx=0,cy=0,ix=0,iy=0;std::pair<float,float> a{};
            for(size_t i=1;i<parameters.size();++i) {
                float t2=parameters[i],t=.5f*(t1+t2);
                if(kind==Section::Waterline || water.evaluate(spline.value(t))<0)for(int j=0;j<=500;++j) {
                    t=float(t1+(double(j)/500)*(t2-t1));auto b=xy(spline.value(t));
                    if(j>0) {
                        float da=.5f*(b.first+a.first)*(b.second-a.second);sum+=da;
                        cx+=da*.25f*(b.first+a.first);cy+=da*.5f*(b.second+a.second);
                        // FPC evaluates this stored Single expression with a
                        // Single 1/12 constant and rounds each multiplication.
                        // Promoting just this coefficient to Double changes KM.
                        constexpr float twelfth=1.f/12.f;
                        ix+=twelfth*(a.second+b.second)*(a.second*a.second+b.second*b.second)*(b.first-a.first);
                        iy+=twelfth*(b.first*b.first*(3*b.second+a.second)+2*a.first*b.first*(a.second+b.second)+a.first*a.first*(3*a.second+b.second))*(b.first-a.first);
                    }
                    a=b;
                }
                t1=t2;
            }
            Vec3 cog{},moment{};
            if(sum!=0) {
                cx/=sum;cy/=sum;ix=std::abs(ix);iy=std::abs(iy);
                if(kind==Section::Station) {cog={-section.coefficients()[3],cx,cy};moment={0,ix,iy};}
                else if(kind==Section::Buttock) {cog={cx,-section.coefficients()[3],cy};moment={ix,0,iy};}
                else {cog={cx,cy,-section.coefficients()[3]};moment={ix,iy,0};}
            }
            result.value+=sum;result.cog.x+=sum*cog.x;result.cog.y+=sum*cog.y;result.cog.z+=sum*cog.z;lastMoment=moment;
        }
        if(result.value!=0) {
            result.cog.x/=result.value;result.cog.y/=result.value;result.cog.z/=result.value;
            if(kind==Section::Waterline) {
                // Preserve the validated symmetric-hull convention. This
                // historical last-contour rule is NOT valid for general
                // asymmetric hulls; callers explicitly omit those fields.
                result.ix=(lastMoment.x-result.cog.y*result.cog.y*result.value)*(paths.size()>=2?2:1);
                result.iy=std::abs(lastMoment.y-result.cog.x*result.cog.x*result.value);
            }
        } else {
            result.cog={};if(kind==Section::Station)result.cog.x=-section.coefficients()[3];
            else if(kind==Section::Buttock)result.cog.y=-section.coefficients()[3];else result.cog.z=-section.coefficients()[3];
        }
        return result;
    }
    Area area(Section kind,const Plane &section,const Plane &water,Job &job) {return asymmetric?fullArea(kind,section,water,job):area(kind,section,water,job,curves(section,job));}
    float midship(const Plane &water,Job &job) {
        double low=minimum.x,high=maximum.x,length=high-low,center=low+.5*length,maxArea=-1;
        float best=float(center);if(length<=0)return best;
        std::map<float,double> cached;
        auto at=[&](double x) {
            float key=float(x);auto found=cached.find(key);if(found!=cached.end())return found->second;
            double a=area(Section::Station,Plane(1,0,0,-key),water,job).value;cached.emplace(key,a);
            if(a>maxArea || (a==maxArea && std::abs(double(key)-center)<std::abs(double(best)-center))) {maxArea=a;best=key;}return a;
        };
        std::vector<double> positions;for(int i=0;i<=128;++i)positions.push_back(low+length*i/128);
        for(const auto &f:input.faces)if(input.layers[f.layer].useInHydrostatics)for(auto p:f.points) {
            double x=input.points[p].position.x;if(x>=low && x<=high)positions.push_back(x);
        }
        std::sort(positions.begin(),positions.end());positions.erase(std::unique(positions.begin(),positions.end()),positions.end());
        constexpr double golden=.6180339887498949;const double tolerance=std::max(1e-6,length*1e-6);
        auto refine=[&](double left,double right) {
            double a=right-golden*(right-left),b=left+golden*(right-left),aa=at(a),ab=at(b);
            for(int i=0;i<32 && right-left>tolerance;++i) {
                if(aa<ab) {left=a;a=b;aa=ab;b=left+golden*(right-left);ab=at(b);}
                else {right=b;b=a;ab=aa;a=right-golden*(right-left);aa=at(a);}
            }
            at((left+right)*.5);
        };
        at(center);std::vector<double> areas;for(auto x:positions)areas.push_back(at(x));
        if(areas[0]>areas[1])refine(positions[0],positions[1]);
        for(size_t i=1;i+1<areas.size();++i)if(areas[i]>=areas[i-1] && areas[i]>=areas[i+1] && (areas[i]>areas[i-1] || areas[i]>areas[i+1]))refine(positions[i-1],positions[i+1]);
        if(areas.back()>areas[areas.size()-2])refine(positions[positions.size()-2],positions.back());
        return best;
    }
    std::pair<float,Vec3> gravity() const {
        float total=0;Vec3 center{};
        for(size_t l=0;l<input.layers.size();++l) {
            float sum=0;Vec3 cog{};
            for(const auto &f:mesh.faces)if(f.layer==l)for(size_t i=2;i<f.points.size();++i) {
                auto a=mesh.points[f.points[0]].position,b=mesh.points[f.points[i-1]].position,c=mesh.points[f.points[i]].position;
                Vec3 p{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
                float x=(a.y-b.y)*(a.z+b.z)+(b.y-c.y)*(b.z+c.z)+(c.y-a.y)*(c.z+a.z);
                float y=(a.z-b.z)*(a.x+b.x)+(b.z-c.z)*(b.x+c.x)+(c.z-a.z)*(c.x+a.x);
                float z=(a.x-b.x)*(a.y+b.y)+(b.x-c.x)*(b.y+c.y)+(c.x-a.x)*(c.y+a.y);
                float area=std::sqrt(x*x+y*y+z*z)*.5f;sum+=area;cog.x+=area*p.x;cog.y+=area*p.y;cog.z+=area*p.z;
            }
            if(sum!=0) {
                cog.x/=sum;cog.y/=sum;cog.z/=sum;if(input.layers[l].symmetric) {sum*=2;cog.y=0;}
                float weight=sum*input.layers[l].thickness*input.layers[l].materialDensity;
                weight/=settings.geometry.imperial?(12*2240):1000;cog.z-=minimum.z;
                total+=weight;center.x+=weight*cog.x;center.y+=weight*cog.y;center.z+=weight*cog.z;
            }
        }
        if(total!=0) {center.x/=total;center.y/=total;center.z/=total;}return {total,center};
    }
    QJsonObject single(float draft,float heel,float trim,Job &job) {
        const auto plane=waterline(draft,heel,trim);const auto &p=plane.coefficients();double lo,hi,leak;
        projectedBounds({p[0],p[1],p[2]},lo,hi,leak,job);if(-double(p[3])>leak+1e-7)bad("水线以下有漏点。");
        const auto v=volume(plane,heel,trim,true,job);if(v.value<=0)bad("没有有效排水体积，请检查面的朝向。");
        const auto &s=settings.geometry;const float length=v.subMax.x-v.subMin.x,width=v.subMax.y-v.subMin.y,lwl=v.wlMax.x-v.wlMin.x,bwl=v.wlMax.y-v.wlMin.y;
        float mx=midship(plane,job);auto mid=area(Section::Station,Plane(1,0,0,-mx),plane,job);
        float cm=0,cwp=0,cb=0,cp=0,cvp=0;bool symmetryCorrection=false;
        if(s.actualCoefficients) {if(width*draft!=0)cm=mid.value/(width*draft);if(!asymmetric && cm>1.01) {cm/=2;mid.value/=2;symmetryCorrection=true;}}
        else if(s.beam*draft!=0)cm=mid.value/(s.beam*draft);
        auto paths=asymmetric?std::vector<SectionCurve>{}:curves(plane,job);Rotation rotate(heel,trim,s.length,keel);
        float entranceAngle=0,foremost=-1e6;
        for(auto &path:paths) {
            for(auto &point:path.points)point=rotate(point);
            Spline spline(path.points,path.knuckles,path.fragments);
            if(spline.value(0).x>spline.value(1).x) {std::reverse(path.points.begin(),path.points.end());std::reverse(path.knuckles.begin(),path.knuckles.end());}
            Spline forward(path.points,path.knuckles,path.fragments);const auto p1=forward.value(1),p2=forward.value(.96);
            if(p1.x>foremost){foremost=p1.x;entranceAngle=p1.x!=p2.x?float(std::atan(double((p2.y-p1.y)/(p1.x-p2.x)))*(180/Pi)):90;
                if(p2.y<p1.y)entranceAngle=-entranceAngle;if(entranceAngle<0)entranceAngle=-entranceAngle/2;}
        }
        auto wl=asymmetric?fullArea(Section::Waterline,plane,plane,job,heel,trim):area(Section::Waterline,plane,plane,job,std::move(paths));
        if(s.actualCoefficients) {
            if(width*length!=0)cwp=wl.value/(width*length);
            if(!asymmetric && cwp>1.1) {cwp/=2;wl.value/=2;wl.iy=-wl.iy+wl.value*(wl.cog.x*wl.cog.x);}
        } else if(bwl*lwl!=0)cwp=wl.value/(bwl*lwl);
        if(draft!=0) {
            const float l=s.actualCoefficients?length:s.length,b=s.actualCoefficients?width:s.beam;
            if(b*l*draft!=0)cb=v.value/(b*l*draft);
            if(mid.value*l!=0)cp=v.value/(mid.value*l);
            if(cwp!=0)cvp=cb/cwp;
        }
        std::vector<float> sectionAreas;for(const auto &station:settings.stations)sectionAreas.push_back(area(Section::Station,station,plane,job).value);
        if(!sectionAreas.empty() && symmetryCorrection && sectionAreas[sectionAreas.size()/2]>mid.value)for(auto &a:sectionAreas)a/=2;
        QJsonArray sac;for(size_t i=0;i<sectionAreas.size();++i)sac.append(QJsonArray{-settings.stations[i].coefficients()[3],finite(sectionAreas[i])});
        auto lateral=asymmetric?projectedArea(plane,false,job):area(Section::Buttock,Plane(0,1,0,-.001f),plane,job);lateral.cog.z-=minimum.z;
        auto weight=gravity();
        auto bulb=area(Section::Station,Plane(1,0,0,-s.length),plane,job);if(symmetryCorrection)bulb.value/=2;bulb.cog.z-=minimum.z;
        QJsonObject out{{"status","ok"},{"buoyancy",json(v.buoyancy)},{"plane",json(plane)},{"keel_z",keel},{"draft",draft},{"heel",heel},{"trim",trim},
            {"absolute_draft",v.draft},{"volume",v.value},{"displacement",v.mass},{"lwl",lwl},{"bwl",bwl},{"wetted_area",v.wetted},
            {"cb",cb},{"cp",cp},{"cvp",cvp},{"midship_x",mx},{"midship_area",mid.value},{"cm",cm},{"waterplane_area",wl.value},{"cwp",cwp},
            {"waterplane_cog",json(wl.cog)},{"waterplane_ix",wl.ix},{"waterplane_iy",wl.iy},
            {"wl_min",json(v.wlMin)},{"wl_max",json(v.wlMax)},{"sub_min",json(v.subMin)},{"sub_max",json(v.subMax)},{"model_min",json(minimum)},
            {"entrance_angle",entranceAngle},{"bulb_area",bulb.value},{"bulb_cog",json(bulb.cog)},
            {"kmt",v.buoyancy.z+wl.ix/v.value},{"kml",v.buoyancy.z+std::abs(wl.iy)/v.value},{"lateral_area",lateral.value},{"lateral_cog",json(lateral.cog)},
            {"weight",weight.first},{"weight_cog",json(weight.second)},{"sectional_areas",sac}};
        for(auto it=out.begin();it!=out.end();++it)if(it.value().isDouble())finite(it.value().toDouble());
        if(asymmetric) {
            out["area_method"]="oriented-mesh-sections";
            out["entrance_angle"]=QJsonValue::Null;
            out["lateral_method"]="clipped-mesh-projection-union";
            out["partial"]=true;
            out["message"]="截面积和侧投影面积已按实际船面计算；非对称船体的单侧进流角暂不提供。";
        }
        return out;
    }
    // Read-only counterpart of BuildAuxiliaryReports. Each station is cut once;
    // its curves are clipped at each waterline without re-subdividing the hull.
    QJsonObject auxiliary(QJsonObject base,Job &job) {
        const auto& stations=settings.stations;const auto& waterlines=settings.waterlines;
        if(waterlines.empty()||waterlines.size()>99||stations.size()>99)bad("附加表需要 1–99 条水线，肋位不超过 99 个。请在剖面线中设置。");
        if(base["status"]!="ok"||(base["partial"].toBool()&&!asymmetric))bad("本次静水力计算不完整，未生成附加表。");
        // Bonjean tables refer to upright, horizontal waterlines even when the
        // main report is heeled/trimmed. Elevations are absolute model Z, not
        // drafts to which keel Z can be added a second time.
        if(base["heel"].toDouble()!=0||base["trim"].toDouble()!=0)
            base=single(float(base["draft"].toDouble()),0,0,job);
        const auto basePlane=waterline(float(base["draft"].toDouble()),0,0);
        const float mid=float(base["midship_area"].toDouble());
        const bool correction=!asymmetric && area(Section::Station,Plane(1,0,0,-float(base["midship_x"].toDouble())),basePlane,job).value>mid;
        const auto number=[](double n,int width,int decimals){return QString::number(finite(n),'f',decimals).rightJustified(width,' ');};
        const auto header=QString::number(waterlines.size()).rightJustified(3,' ')+QString::number(stations.size()).rightJustified(3,' ');
        QStringList sac{header," "},bonjean{header," "},sterns{header," "};QString headings="        ";
        QJsonArray stationX,rows;std::vector<std::vector<SectionCurve>> paths;
        for(const auto& station:stations){job.check();const float x=-station.coefficients()[3];stationX.append(x);headings+=number(x,8,3);paths.push_back(asymmetric?std::vector<SectionCurve>{}:curves(station,job));}
        bonjean<<headings;double low,high,leak;projectedBounds({0,0,1},low,high,leak,job);
        for(const auto& water:waterlines){
            job.check();const float z=-water.coefficients()[3];const Plane plane(0,0,1,-z);
            if(double(z)>leak+1e-7)bad("附加表中的水线高于船体漏点，未生成不完整报表。");
            const auto v=volume(plane,0,0,true,job);const bool dry=v.value<=0;
            QJsonArray raw,areas;for(size_t i=0;i<stations.size();++i)raw.append(dry?0:finite(area(Section::Station,stations[i],plane,job,paths[i]).value));
            const bool half=correction&&!raw.isEmpty()&&raw[raw.size()/2].toDouble()>mid;
            sac<<number(z,10,4);QString line=number(z,8,3);
            for(qsizetype i=0;i<raw.size();++i){const double a=raw[i].toDouble();areas.append(half?a/2:a);
                line+=number(a,8,(i+1<raw.size()&&a>999)?2:3);sac<<number(stationX[i].toDouble(),10,4)+number(areas[i].toDouble(),10,4);}
            sac<<" ";bonjean<<line;
            sterns<<number(z,9,3)+number(dry?0:v.wlMin.x,9,3)+number(dry?0:v.wlMax.x,9,3);
            rows.append(QJsonObject{{"waterline_z",z},{"draft",double(z)-keel},{"dry",dry},{"bonjean_areas",raw},{"sac_areas",areas},{"sac_correction",half},
                {"aft_x",dry?QJsonValue(QJsonValue::Null):QJsonValue(v.wlMin.x)},{"fore_x",dry?QJsonValue(QJsonValue::Null):QJsonValue(v.wlMax.x)}});
        }
        sac<<"EOF ";bonjean<<"EOF ";sterns<<" "<<"EOF ";
        // Weights.txt v2 fixes the old last-triangle-only child-face records.
        // Every child contributes its whole area and area-weighted centroid;
        // weights have the same physical units as the main hydrostatic report.
        const float divisor=settings.geometry.imperial?12*2240:1000;
        QStringList weights{"FreeShip_26 Weights v2",settings.geometry.imperial?"length=ft area=ft2 weight=long_ton thickness=in density=lb/ft3":"length=m area=m2 weight=t thickness=mm density=t/m3",
            "# layer child area weight x y z xmin xmax (coordinates: model origin)"};
        QJsonArray layers;size_t detailCount=0;qsizetype textSize=0;
        for(size_t l=0;l<input.layers.size();++l){
            const auto& layer=input.layers[l];QJsonArray entries;double totalArea=0,totalWeight=0,cx=0,cy=0,cz=0;
            for(size_t f=0;f<mesh.faces.size();++f){const auto& face=mesh.faces[f];if(face.layer!=l)continue;job.check();
                if(++detailCount>250000)bad("重量明细超过 250000 个面的大小上限，未截断输出；主静水力报告仍可查看。");
                float sum=0;Vec3 cog{};float xmin=std::numeric_limits<float>::max(),xmax=-xmin;
                for(auto i:face.points){xmin=std::min(xmin,mesh.points[i].position.x);xmax=std::max(xmax,mesh.points[i].position.x);}
                for(size_t i=2;i<face.points.size();++i){
                    const auto a=mesh.points[face.points[0]].position,b=mesh.points[face.points[i-1]].position,c=mesh.points[face.points[i]].position;
                    const Vec3 p{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
                    const float x=(a.y-b.y)*(a.z+b.z)+(b.y-c.y)*(b.z+c.z)+(c.y-a.y)*(c.z+a.z),
                        y=(a.z-b.z)*(a.x+b.x)+(b.z-c.z)*(b.x+c.x)+(c.z-a.z)*(c.x+a.x),
                        z=(a.x-b.x)*(a.y+b.y)+(b.x-c.x)*(b.y+c.y)+(c.x-a.x)*(c.y+a.y);
                    const float area=std::sqrt(x*x+y*y+z*z)*.5f;sum+=area;cog.x+=area*p.x;cog.y+=area*p.y;cog.z+=area*p.z;
                }
                if(sum<=0)continue;cog.x/=sum;cog.y/=sum;cog.z/=sum;if(layer.symmetric){sum*=2;cog.y=0;}
                const float mass=sum*layer.thickness*layer.materialDensity/divisor;
                entries.append(QJsonObject{{"child",double(f)},{"control_face",double(face.rootControlFace)},{"area",sum},{"weight",mass},{"cog",json(cog)},{"xmin",xmin},{"xmax",xmax}});
                totalArea+=sum;totalWeight+=mass;cx+=sum*cog.x;cy+=sum*cog.y;cz+=sum*cog.z;
                const QString line=QString::number(l)+" "+QString::number(f)+" "+QString::number(sum,'g',9)+" "+QString::number(mass,'g',9)+" "+QString::number(cog.x,'g',9)+" "+QString::number(cog.y,'g',9)+" "+QString::number(cog.z,'g',9)+" "+QString::number(xmin,'g',9)+" "+QString::number(xmax,'g',9);
                weights<<line;textSize+=line.size();if(textSize>16*1024*1024)bad("重量报表超过大小上限，未生成截断文件。");
            }
            if(entries.isEmpty())continue;
            const auto name=decodeFbmText(QByteArray::fromStdString(layer.name),encoding);
            const QJsonArray center{cx/totalArea,cy/totalArea,cz/totalArea};
            layers.append(QJsonObject{{"index",double(l)},{"name",name},{"surface_area",totalArea},{"weight",totalWeight},{"cog",center},{"thickness",layer.thickness},{"density",layer.materialDensity},{"faces",entries}});
            auto label=name;label.replace('\n',' ');label.replace('\r',' ');
            weights<<QString("# total %1 %2 area=%3 weight=%4 thickness=%5 density=%6").arg(l).arg(label).arg(totalArea,0,'g',12).arg(totalWeight,0,'g',12).arg(layer.thickness,0,'g',9).arg(layer.materialDensity,0,'g',9);
        }
        QJsonArray files;const auto file=[&](QString name,const QStringList& lines){files.append(QJsonObject{{"name",name},{"text",lines.join('\n')+'\n'}});};
        if(!stations.empty()){file("SACs.txt",sac);file("Bonjean.txt",bonjean);}file("Weights.txt",weights);file("Sterns.txt",sterns);
        return {{"status",stations.empty()?"partial":"ok"},{"message",stations.empty()?"未定义肋位，仅生成重量和首尾水线位置表。":"附加表按已定义的水平水线计算；重量明细采用完整面的面积。"},
            {"reference","upright_model_z"},{"stations",stationX},{"waterlines",rows},{"layers",layers},{"station_cuts",double(paths.size())},{"weight_format",2},{"files",files}};
    }
    QJsonObject balance(double mass,double heel,double trim,bool freeTrim,Vec3 cg,Job &job) {
        Vec3 normal{},g{};Volume v;double residual=0,offset=0;
        float finalTrim=0;const auto length=settings.geometry.length;
        double massTolerance=std::max(mass*2e-6,1e-6),lengthTolerance=std::max(length*2e-6,1e-6);
        auto solve=[&](double value) {
            job.check();finalTrim=float(value);const float hValue=float(heel);
            const float trimAngle=float(std::atan((-finalTrim*std::cos(radians(hValue)))/length)*180/Pi);
            double h=radians(-double(hValue)),t=radians(-double(trimAngle));
            normal={float(-std::sin(t)),float(std::sin(h)*std::cos(t)),float(std::cos(h)*std::cos(t))};
            double lo,hi,leak;projectedBounds(normal,lo,hi,leak,job);
            if(leak<hi)hi=leak-std::max(1e-6,(hi-lo)*1e-7);
            if(hi<=lo)bad("该横倾角下，船体在形成有效排水量之前已进水。");
            auto evaluate=[&](double d) {return volume(Plane(normal.x,normal.y,normal.z,float(-d)),hValue,finalTrim,false,job);};
            v=evaluate(hi);if(v.mass<mass-massTolerance)bad("目标排水量超过当前横倾下的完整浮力范围。");
            double bestError=1e300,bestOffset=hi;
            for(int i=0;i<64;++i) {
                double mid=.5*(lo+hi);v=evaluate(mid);double error=v.mass-mass;
                if(std::abs(error)<bestError) {bestError=std::abs(error);bestOffset=mid;}
                if(std::abs(error)<=massTolerance)break;
                if(error<0)lo=mid;else hi=mid;
            }
            offset=bestOffset;v=evaluate(offset);
            if(std::abs(v.mass-mass)>massTolerance)bad("排水量求解未收敛，未返回近似成功值。");
            g={float(cg.x*std::cos(t)+(cg.y*std::sin(h)+(cg.z-keel)*std::cos(h))*std::sin(t)),
               float(cg.y*std::cos(h)-(cg.z-keel)*std::sin(h)),
               float(-cg.x*std::sin(t)+(cg.y*std::sin(h)+(cg.z-keel)*std::cos(h))*std::cos(t))};
            residual=v.buoyancy.x-g.x;
        };
        bool valid=false;
        try {solve(trim);valid=true;} catch(const Cancelled &) {throw;} catch(const std::exception &) {if(!freeTrim)throw;}
        if(freeTrim && (!valid || std::abs(residual)>lengthTolerance)) {
            double left=0,right=0,rl=0;int found=0;
            for(int i=0;i<=40;++i) {
                right=length*(-.5+double(i)/40);
                try {solve(right);} catch(const Cancelled &) {throw;} catch(const std::exception &) {found=0;continue;}
                if(std::abs(residual)<=lengthTolerance || (found==1 && rl*residual<0)) {found=2;break;}
                left=right;rl=residual;found=1;
            }
            if(found!=2)bad("未找到纵倾平衡，请检查重心和排水量。");
            if(std::abs(residual)>lengthTolerance)for(int i=0;i<48;++i) {
                double mid=.5*(left+right);solve(mid);if(std::abs(residual)<=lengthTolerance)break;
                if(residual*rl>0) {left=mid;rl=residual;} else right=mid;
            }
            if(std::abs(residual)>lengthTolerance)bad("纵倾平衡未收敛。");
        }
        return {{"status","ok"},{"target_displacement",mass},{"heel",heel},{"trim",finalTrim},{"displacement",v.mass},{"volume",v.value},
            {"mass_residual",double(v.mass)-mass},{"longitudinal_residual",residual},{"mass_tolerance",massTolerance},{"length_tolerance",lengthTolerance},
            {"absolute_draft",v.draft},{"keel_z",keel},{"kn",v.buoyancy.y},{"gz",double(v.buoyancy.y)-g.y},{"buoyancy",json(v.buoyancy)},
            {"gravity_projected",json(g)},{"plane",QJsonArray{normal.x,normal.y,normal.z,-offset}},{"evaluations",job.evaluations}};
    }
};

Hydrostatics::Hydrostatics(const Fbm &source,const QJsonObject &bindings) {QElapsedTimer timer;timer.start();impl_=std::make_unique<Impl>(source,bindings);impl_->buildMs=timer.nsecsElapsed()/1e6;}
Hydrostatics::~Hydrostatics()=default;
QJsonObject Hydrostatics::presentation(const QJsonObject &visibility,const QJsonObject &context) {
    auto &h=*impl_;Job job({});float split=0;
    // The old headless design calculation has draft=0, independently of the
    // project's design draft. Keep this historical crease convention exactly.
    try {
        const auto plane=h.waterline(0,0,0);
        if(h.volume(plane,0,0,true,job).value>0)split=h.midship(plane,job);
    } catch(const std::exception &) { /* An open/empty hull still has a drawing. */ }
    QJsonObject hydro{{"available",false},{"markers",QJsonArray{}},{"areas",QJsonArray{}},{"path",QJsonArray{}}};
    try {
        const auto row=h.single(h.settings.geometry.draft,0,0,job);
        if(row["partial"].toBool() && !h.asymmetric)bad("Incomplete hydrostatic overlay");
        QJsonArray markers,areas,path;
        const auto point=[](QJsonValue value) {auto a=value.toArray();return Vec3{float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};};
        const auto caption=[&](QString name,float value) {
            auto number=QString::number(finite(value),'f',2);
            const auto separator=context["decimal_separator"].toString(".");
            if(separator!=".")number.replace('.',separator);
            return name+'='+number;
        };
        const auto marker=[&](const char *key,QString name,float value,Vec3 p,bool compensate) {
            if(compensate)p.z+=h.minimum.z;
            markers.append(QJsonObject{{"key",key},{"text",caption(name,value)},{"p",json(p)}});
        };
        const auto buoyancy=point(row["buoyancy"]),waterplane=point(row["waterplane_cog"]);
        if(visibility["displacement"].toBool())marker("displacement","Displ",float(row["displacement"].toDouble()),buoyancy,true);
        if(h.asymmetric || std::abs(waterplane.y)<.01) {
            const float km=float(row["kmt"].toDouble());
            if(visibility["metacentric_height"].toBool())marker("km","KM",h.asymmetric?km:km+h.minimum.z,{buoyancy.x,h.asymmetric?buoyancy.y:0,km},true);
            if(visibility["lcf"].toBool())marker("lcf","LCF",waterplane.x,waterplane,h.asymmetric);
        }
        const float weight=float(row["weight"].toDouble());
        if(visibility["displacement"].toBool() && weight>0)marker("weight","Weight",weight,point(row["weight_cog"]),true);
        if(visibility["lateral_area"].toBool()) {
            const float submerged=float(row["lateral_area"].toDouble());const auto cog=point(row["lateral_cog"]);
            marker("lateral",context["lateral"].toString(),submerged,cog,true);
            auto plane=h.waterline(h.settings.geometry.draft,0,0);auto p=plane.coefficients();
            if(h.asymmetric) {
                const auto wind=h.projectedArea(plane,true,job);
                if(wind.value>0)marker("wind",context["wind"].toString(),wind.value,wind.cog,false);
            } else {
            auto all=h.area(Section::Buttock,Plane(0,1,0,-.001f),Plane(p[0],p[1],p[2],-500),job);
            all.cog.z-=h.minimum.z;
            const float momentZ=all.value*all.cog.z-submerged*cog.z,momentX=all.value*all.cog.x-submerged*cog.x;
            float wind=all.value-submerged;if(wind<=0)wind=.0001f;
            const float z=momentZ/wind+p[3]; // Keep the two legacy Single rounding steps.
            marker("wind",context["wind"].toString(),wind,{momentX/wind,0,z-p[3]},true);
            }
        }
        if(visibility["sectional_areas"].toBool()) {
            std::vector<Vec3> points;const float bwl=float(row["bwl"].toDouble()),draft=h.settings.geometry.draft;
            for(const auto value:row["sectional_areas"].toArray()) {
                auto a=value.toArray();const float area=float(a[1].toDouble());Vec3 p{float(a[0].toDouble()),0,area};
                if(bwl*draft!=0)p.z=2*(h.maximum.z-h.minimum.z)*area/(bwl*draft);
                p.z+=h.minimum.z;points.push_back(p);areas.append(QJsonObject{{"p",json(p)},{"value",area}});
            }
            if(points.size()>1) {Spline spline(std::move(points));for(int i=0;i<=600;++i)path.append(json(spline.value(double(i)/600)));}
        }
        hydro={{"available",true},{"markers",markers},{"areas",areas},{"path",path}};
    } catch(const std::exception &) {
        // Same presentation boundary as OverlaySnapshot: retain the editable
        // hull, but never show a fabricated numerical result.
    }
    return {{"crease_split",split},{"hydro",hydro}};
}
QJsonObject Hydrostatics::calculate(const QJsonObject &request,const QString &cancelFile) {
    auto &h=*impl_;const auto &s=h.settings.geometry;const auto mode=request["mode"].toString();
    if(mode!="single" && mode!="table" && mode!="cross")bad("未知静水力计算类型。");
    if(s.length<=0)bad("请先设置有效船长。");
    double trim=number(request,"trim",-s.length,s.length),heel=0;
    std::vector<double> drafts,angles,masses;Vec3 cg{};bool freeTrim=false;
    if(mode=="single") {drafts={number(request,"draft",0,1e6)};heel=number(request,"heel",-85,85);}
    else if(mode=="table") {
        double start=number(request,"start",0,1e6),end=number(request,"end",start,1e6),step=number(request,"step",1e-6,1e6);
        if((end-start)/step>199+1e-8)bad("吃水表最多包含 200 行。");
        int count=int(std::floor((end-start)/step+1e-8))+1;for(int i=0;i<count;++i)drafts.push_back(start+i*step);
    } else {
        angles=numbers(request,"angles",-180,180,181);masses=numbers(request,"displacements",1e-6,1e9,100);
        if(angles.size()*masses.size()>500)bad("稳性表最多包含 500 个工况。");
        cg={float(number(request,"lcg",-1e9,1e9)),float(number(request,"tcg",-1e9,1e9)),float(number(request,"vcg",-1e9,1e9))};
        if(!request["free_trim"].isBool())bad("缺少纵倾模式。");
        freeTrim=request["free_trim"].toBool();
    }
    Job job(cancelFile);job.check();QJsonObject inputs;
    for(const char *key:{"mode","model_name","draft","heel","trim","start","end","step","angles","displacements","lcg","tcg","vcg","free_trim"})if(request.contains(key))inputs[key]=request[key];
    QJsonObject out{{"schema",1},{"engine","cpp-hydrostatics"},{"mode",mode},{"keel_z",h.keel},{"units",s.imperial?"imperial":"metric"},
        {"water_density",s.density},{"appendage_coefficient",s.appendage},{"inputs",inputs},{"cancelled",false}};
    QJsonArray rows;
    try {
        if(mode=="cross")for(double mass:masses) {
            bool previousValid=false;double integral=0,previousAngle=0,previousGz=0;
            for(size_t i=0;i<angles.size();++i) {
                job.check();QJsonObject row;
                try {
                    row=h.balance(mass,angles[i],trim,freeTrim,cg,job);
                    if(previousValid)integral+=.5*(previousGz+row["gz"].toDouble())*radians(angles[i]-previousAngle);
                    if(i==0)previousValid=angles[i]==0;
                    row["gz_integral"]=previousValid?QJsonValue(integral):QJsonValue(QJsonValue::Null);previousAngle=angles[i];previousGz=row["gz"].toDouble();
                } catch(const Cancelled &) {throw;} catch(const std::exception &e) {
                    previousValid=false;row={{"status","error"},{"message",QString::fromUtf8(e.what())},{"heel",angles[i]},{"target_displacement",mass}};
                }
                rows.append(row);
            }
        } else for(double draft:drafts) {
            job.check();
            try {rows.append(h.single(float(draft),float(heel),float(trim),job));}
            catch(const Cancelled &) {throw;} catch(const std::exception &e) {rows.append(QJsonObject{{"status","error"},{"message",QString::fromUtf8(e.what())},{"draft",draft},{"heel",heel},{"trim",trim}});}
        }
        if(mode=="single"&&h.settings.auxiliary){
            try{out["auxiliary"]=h.auxiliary(rows[0].toObject(),job);}
            catch(const Cancelled&){throw;}
            catch(const std::exception& e){out["auxiliary"]=QJsonObject{{"status","error"},{"message",QString::fromUtf8(e.what())},{"files",QJsonArray{}}};}
        }
    } catch(const Cancelled &e) {out["cancelled"]=true;out["message"]=QString::fromUtf8(e.what());}
    out["rows"]=rows;out["elapsed_ms"]=double(job.clock.elapsed());out["mesh_build_ms"]=h.buildMs;
    out["volume_evaluations"]=job.evaluations;out["section_evaluations"]=job.sections;return out;
}
QJsonObject Hydrostatics::defaults(const QString &encoding) {
    auto &h=*impl_;const auto &s=h.settings;QJsonArray angles,masses;
    for(float a:s.angles)angles.append(a);
    if(s.increments && s.massStep>0 && (s.maxMass-s.minMass)/s.massStep<=99) {
        for(int i=0;i<100 && s.minMass+i*s.massStep<=s.maxMass+1e-6;++i)if(s.minMass+i*s.massStep>0)masses.append(double(s.minMass+i*s.massStep));
    } else for(float m:s.masses)masses.append(m);
    QJsonObject out{{"start",s.start},{"end",s.end},{"step",s.step},{"trim",s.trim},{"draft",s.geometry.draft},{"keel_z",h.keel},
        {"vcg",s.vcg},{"free_trim",s.freeTrim},{"cross_trim",s.trims.empty()?0:s.trims[0]},{"length",s.geometry.length},
        {"name",decodeFbmText(s.name,encoding)},{"coefficient_basis",s.geometry.actualCoefficients?1:0},{"angles",angles},{"displacements",masses}};
    try {
        Job job({});auto plane=h.waterline(s.geometry.draft,0,0);auto p=plane.coefficients();double lo,hi,leak;
        h.projectedBounds({p[0],p[1],p[2]},lo,hi,leak,job);
        if(-double(p[3])<=leak+1e-7) {auto v=h.volume(plane,0,0,false,job);if(v.mass>0)out["design_displacement"]=v.mass;}
    } catch(const std::exception &) { /* Ranges remain editable for an incomplete hull. */ }
    return out;
}
}
