// GPL-2.0-or-later. Adapted from FREE!ship / Plus FreeDevelopedPatch.Unroll,
// IntersectPlane and FreeSubdivisionLayer.Unroll (Martijn van Engeland, 2005;
// Timoshenko V.F., Mark Malakanov). See docs/migration/NATIVE-PLATES.md.
#include "plates.h"
#include "geometry_input.h"
#include "fbm_text.h"
#include "native/sections.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

namespace fs::document {
namespace {
using namespace native_geometry;
using Index=std::size_t;
using Key=std::pair<Index,Index>;
constexpr double Pi=3.1415926535897932384626433832795;
constexpr Index Missing=std::numeric_limits<Index>::max();
[[noreturn]] void bad(const char *message) { throw std::runtime_error(message); }
Key key(Index a,Index b) { return std::minmax(a,b); }
struct Job {
    QString cancel; QElapsedTimer timer;
    explicit Job(QString path):cancel(std::move(path)) { timer.start();check(); }
    void check() const {
        if(QFileInfo::exists(cancel))bad("板材展开已取消。");
        if(timer.elapsed()>90000)bad("板材展开超过 90 秒，已停止；请分图层展开。");
    }
};
double finite(double x) { if(!std::isfinite(x))bad("展开结果不是有限数，请检查曲面。");return x; }
QJsonArray xy(Vec3 p) { return {finite(p.x),finite(p.y)}; }
QJsonArray xyz(Vec3 p) { return {finite(p.x),finite(p.y),finite(p.z)}; }
void bounds(Vec3 p,Vec3 &lo,Vec3 &hi) {
    lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
    hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
}
double distance(Vec3 a,Vec3 b) {
    const double x=b.x-a.x,y=b.y-a.y,z=b.z-a.z;return std::sqrt(x*x+y*y+z*z);
}
float squared(Vec3 a,Vec3 b) {
    const float x=b.x-a.x,y=b.y-a.y,z=b.z-a.z;return x*x+y*y+z*z;
}
Vec3 normal(Vec3 a,Vec3 b,Vec3 c) {
    Vec3 n{(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),
           (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),
           (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)};
    const float length=std::max(1e-6f,std::sqrt(squared({},n)));return {n.x/length,n.y/length,n.z/length};
}
float area3(Vec3 a,Vec3 b,Vec3 c) {
    const float x=.5f*((a.y-b.y)*(a.z+b.z)+(b.y-c.y)*(b.z+c.z)+(c.y-a.y)*(c.z+a.z));
    const float y=.5f*((a.z-b.z)*(a.x+b.x)+(b.z-c.z)*(b.x+c.x)+(c.z-a.z)*(c.x+a.x));
    const float z=.5f*((a.x-b.x)*(a.y+b.y)+(b.x-c.x)*(b.y+c.y)+(c.x-a.x)*(c.y+a.y));
    return std::sqrt(x*x+y*y+z*z);
}
double area2(Vec3 a,Vec3 b,Vec3 c) {
    const double x=b.x-a.x,y=b.y-a.y,u=c.x-a.x,v=c.y-a.y;return .5*std::abs(x*v-y*u);
}
Vec3 thirdPoint(double a,double b,double c,Vec3 p,Vec3 q) {
    double angle=q.x==p.x ? (q.y>p.y ? Pi/2:-Pi/2):std::atan((q.y-p.y)/(q.x-p.x));
    if(angle<0 && q.y>p.y)angle+=Pi;
    if(angle>0 && q.y<p.y)angle+=Pi;
    if(2*a*c==0) { if(a==0)a=1e-7;if(c==0)c=1e-7; }
    angle+=std::acos(std::clamp((a*a+c*c-b*b)/(2*a*c),-1.0,1.0));
    return {float(c*std::cos(angle)+p.x),float(c*std::sin(angle)+p.y),0};
}
struct Read {
    QByteArray data;qsizetype at=0;
    QByteArray take(qsizetype n) { if(n<0 || n>data.size()-at)bad("Invalid plate settings");auto s=data.mid(at,n);at+=n;return s; }
    int integer() { auto b=take(4);return qFromLittleEndian<qint32>(b.constData()); }
    float real() { quint32 b=quint32(integer());float f;std::memcpy(&f,&b,4);return f; }
    bool flag() { return take(1)[0]!=0; }
    QByteArray string() { return take(integer()); }
};
struct Cut { Plane plane;int kind,color,dxf; };
struct Style {
    int units=0,underwater=0;bool shade=true;float draft=1;
    QString part,encoding;
    QJsonObject resolved;std::vector<int> palette;std::vector<Cut> cuts;
    int rgb(int raw) const {
        if(raw>=0 && raw<=0xffffff)return raw;
        auto v=resolved[QString::number(raw)];
        if(!v.isDouble() || v.toInt(-1)<0 || v.toInt()>0xffffff)bad("Missing resolved plate color");
        return v.toInt();
    }
    int dxf(int color) const {
        int best=1,smallest=std::numeric_limits<int>::max();
        for(Index i=0;i<palette.size();++i) {
            int sum=0;for(int shift:{0,8,16}) {int d=((color>>shift)&255)-((palette[i]>>shift)&255);sum+=d*d;}
            if(sum<smallest) { smallest=sum;best=int(i+1); }
        }return best;
    }
    Style(const Fbm &source,const QJsonObject &bindings,const QJsonObject &context) {
        encoding=bindings["fbm_encoding"].toString();resolved=bindings["resolved_colors"].toObject();
        part=context["part"].toString();if(part.isEmpty())bad("Missing plate language context");
        if(context["sections"].toArray().size()!=4)bad("Missing plate section colors");
        for(auto v:context["palette"].toArray())palette.push_back(v.toInt());
        if(palette.size()!=255)bad("Missing plate DXF palette");
        underwater=bindings["default_underwater_color"].toInt();
        if(source.version()>=2) {
            Read r{source.blockBytes("project_settings")};r.string();r.string();r.real();r.real();draft=r.real();
            r.flag();r.real();r.real();shade=r.flag();underwater=r.integer();units=r.integer();
        }
        underwater=rgb(underwater);
        int group=0;
        for(const char *name:{"stations","buttocks","waterlines","diagonals"}) {
            const auto style=context["sections"].toArray().at(group++).toObject();
            if(group==4 && source.version()<9)continue;
            Read r{source.blockBytes(name)};const int n=r.integer();
            for(int i=0;i<n;++i) {
                const int storedKind=r.integer();if(source.version()>=11)r.flag();
                const float a=r.real(),b=r.real(),c=r.real(),d=r.real();Plane plane(a,b,c,d);
                const int kind=std::abs(a)>.9999?1:std::abs(b)>.9999?2:std::abs(c)>.9999?3:(std::abs(b)>.5 && std::abs(c)>.5)?4:0;
                if(kind && plane.valid())cuts.push_back({plane,kind,style["color"].toInt(),style["dxf"].toInt()});
                r.flag();const int count=r.integer();const int dimensions=source.version()>=6 && storedKind>=1 && storedKind<=3?2:3;
                for(int j=0;j<count;++j)r.take(qsizetype(r.integer())*(dimensions*4+1));
            }
        }
    }
};
struct Edge { Index a,b;bool crease,boundary,seam;std::vector<Index> faces; };
struct Face { std::vector<Index> points,edges;float area=0;Vec3 normal{}; };
struct Trial {
    std::vector<Vec3> uv;std::vector<Index> order;std::vector<double> errors;
    double reference=0,signedArea=0,absolute=0,maxArea=0,normalized=0;
    Index errorFace=Missing;
};
struct Patch {
    std::vector<Vec3> original;std::vector<VertexType> types;
    std::vector<std::vector<Index>> pointFaces;
    std::vector<Face> faces;std::vector<Edge> edges;
    Trial best;int iterations=0;Vec3 lo{},hi{};float angle=0,cs=1,sn=0;
    bool joined=false;Plane mirror;

    Patch(const SubdivisionLevel &mesh,const std::vector<Index> &selected,
          const std::map<Key,Index> &meshEdges,Job &job) {
        std::map<Index,Index> points,selectedFaces;
        for(Index i=0;i<selected.size();++i) {
            selectedFaces.emplace(selected[i],i);
            for(auto p:mesh.faces[selected[i]].points)points.emplace(p,0);
        }
        for(auto &[source,local]:points) {local=original.size();original.push_back(mesh.points[source].position);types.push_back(mesh.points[source].type);}
        pointFaces.resize(original.size());std::map<Index,Index> localEdges;
        for(auto f:selected) {
            const auto &p=mesh.faces[f].points;
            for(Index i=0;i<p.size();++i)localEdges.emplace(meshEdges.at(key(p[(i+p.size()-1)%p.size()],p[i])),0);
        }
        for(auto &[source,local]:localEdges) {
            const auto &e=mesh.edges[source];local=edges.size();Edge out{points.at(e.points[0]),points.at(e.points[1]),e.crease,false,false,{}};
            if(e.reversed)std::swap(out.a,out.b);
            for(auto f:e.faces)if(auto it=selectedFaces.find(f);it!=selectedFaces.end())out.faces.push_back(it->second);
            out.boundary=out.faces.size()==1;
            out.seam=e.faces.size()==1 && std::abs(original[out.a].y)<=1e-4 && std::abs(original[out.b].y)<=1e-4;
            edges.push_back(std::move(out));
        }
        for(auto f:selected) {
            if((faces.size()&255)==0)job.check();
            Face face;const auto &source=mesh.faces[f].points;Vec3 centre{},n{};
            for(auto p:source) {auto i=points.at(p);face.points.push_back(i);pointFaces[i].push_back(faces.size());auto v=original[i];centre.x+=v.x;centre.y+=v.y;centre.z+=v.z;}
            const float count=float(source.size());centre={centre.x/count,centre.y/count,centre.z/count};
            for(Index i=0;i<source.size();++i) {
                auto prev=(i+source.size()-1)%source.size();face.edges.push_back(localEdges.at(meshEdges.at(key(source[prev],source[i]))));
                auto v=normal(centre,original[face.points[prev]],original[face.points[i]]);n.x+=v.x;n.y+=v.y;n.z+=v.z;
                if(i>=2)face.area+=area3(original[face.points[0]],original[face.points[i-1]],original[face.points[i]]);
            }
            float length=std::sqrt(squared({},n));if(length==0)length=1;
            face.normal={n.x/length,n.y/length,n.z/length};faces.push_back(std::move(face));
        }
    }
    Trial process(Index seed,Job &job) const {
        Trial t;t.uv.resize(original.size());std::vector<bool> done(original.size()),visited(faces.size());
        bool first=true,orientation=false;
        auto triangle=[&](Index a,Index b,Index c,Index face) {
            double length=distance(original[a],original[b]);
            if(!done[a] && !done[b] && !done[c]) {t.uv[a]={original[a].x,original[a].y,0};t.uv[b]={t.uv[a].x,float(t.uv[a].y+length),0};done[a]=done[b]=true;}
            if(done[a] && done[b] && !done[c]) {
                const auto p=t.uv[a],q=t.uv[b];
                auto v=thirdPoint(distance(p,q),distance(original[b],original[c]),distance(original[c],original[a]),p,q);
                const bool winding=(q.x-p.x)*(v.y-q.y)-(q.y-p.y)*(v.x-q.x)>=0;
                if(first) {first=false;orientation=winding;} else if(winding!=orientation && t.errorFace==Missing)t.errorFace=face;
                t.uv[c]=v;done[c]=true;
            }
        };
        t.order.push_back(seed);visited[seed]=true;Index cursor=0;
        while(cursor<t.order.size()) {
            if((cursor&255)==0)job.check();
            const auto f=t.order[cursor++];const auto &face=faces[f];const auto &p=face.points;Index start=0;
            for(Index i=0;i<p.size();++i)if(done[p[i]] && done[p[(i+1)%p.size()]]) {start=i;break;}
            for(Index i=2;i<p.size();++i)triangle(p[start],p[(start+i-1)%p.size()],p[(start+i)%p.size()],f);
            for(auto e:face.edges)for(auto next:edges[e].faces)if(!visited[next]) {visited[next]=true;t.order.push_back(next);}
            if(cursor==t.order.size() && cursor<faces.size()) {
                const auto next=Index(std::find(visited.begin(),visited.end(),false)-visited.begin());visited[next]=true;t.order.push_back(next);
            }
        }
        if(std::find(done.begin(),done.end(),false)!=done.end())bad("展开面缺少连通顶点。");
        for(auto f:t.order) {
            const auto &p=faces[f].points;double area=0;
            for(Index i=2;i<p.size();++i)area+=area2(t.uv[p[0]],t.uv[p[i-1]],t.uv[p[i]]);
            const auto error=area-faces[f].area;t.reference+=std::abs(faces[f].area);t.signedArea+=error;t.absolute+=std::abs(error);t.maxArea=std::max(t.maxArea,std::abs(error));
        }
        double maximum=0,length=0;
        for(const auto &e:edges) {
            const double a=std::sqrt(double(squared(original[e.a],original[e.b]))),b=std::sqrt(double(squared(t.uv[e.a],t.uv[e.b])));
            t.errors.push_back(b-a);maximum=std::max(maximum,std::abs(b-a));length=std::max(length,a);
        }
        t.normalized=t.reference>0?maximum/std::sqrt(t.reference)+t.absolute/t.reference:length>0?maximum/length:0;
        finite(t.normalized);return t;
    }
    void rotation(float value) {angle=value;cs=float(std::cos(double(value)*Pi/180));sn=float(std::sin(double(value)*Pi/180));}
    Vec3 display(Vec3 p) const {
        const float x=.5f*(lo.x+hi.x),y=.5f*(lo.y+hi.y),u=p.x-x,v=p.y-y;
        return {u*cs-v*sn+x,u*sn+v*cs+y,0};
    }
    std::pair<Vec3,Vec3> extents() const {
        Vec3 a=display(best.uv.front()),b=a;for(auto p:best.uv)bounds(display(p),a,b);return {a,b};
    }
    void rawBounds() {lo=hi=best.uv.front();for(auto p:best.uv)bounds(p,lo,hi);}
    void unroll(bool symmetric,Job &job) {
        std::vector<Index> seeds;
        for(const auto &p:pointFaces)if(p.size()==1)seeds.push_back(p[0]);
        if(seeds.empty()) {
            if(faces.empty())bad("展开图层没有可用的曲面。");
            Index seed=0;for(Index f=1;f<faces.size();++f)if(faces[f].area>faces[seed].area)seed=f;
            seeds.push_back(seed);
        }
        // Retain the original candidate ordering rule, including duplicate corner
        // seeds. Mesh IDs replace allocator-address order; no pointer dependence.
        for(Index i=0;i+1<seeds.size();++i) {float a=faces[seeds[i]].area;for(Index j=1;j<seeds.size();++j)if(faces[seeds[j]].area<a) {std::swap(seeds[i],seeds[j]);a=faces[seeds[i]].area;}}
        for(Index i=0;i<seeds.size();++i) {
            job.check();auto trial=process(seeds[i],job);++iterations;
            if(trial.errorFace!=Missing && seeds.size()<25 && std::find(seeds.begin(),seeds.end(),trial.errorFace)==seeds.end())seeds.push_back(trial.errorFace);
            if(i==0 || trial.normalized<best.normalized)best=std::move(trial);
        }
        rawBounds();float opt=0,bestArea=0;
        for(int i=0;i<=180;++i) {
            if((i&15)==0)job.check();
            rotation(float(i)/2);auto [a,b]=extents();const float area=(b.x-a.x)*(b.y-a.y);
            if(i==0 || area<bestArea) {bestArea=area;opt=angle;}
        }
        rotation(opt);auto [a,b]=extents();if(b.x-a.x<b.y-a.y)rotation(angle-90);
        if(symmetric)joinMirror();
    }
    void joinMirror() {
        std::vector<Index> seam;for(Index i=0;i<edges.size();++i)if(edges[i].seam)seam.push_back(i);
        if(seam.empty() || std::any_of(faces.begin(),faces.end(),[](const Face &f){return std::abs(f.normal.y)>1e-2;}))return;
        // Endpoints of the first centre-edge chain. Check every centre point,
        // including other chains, before joining the two physical halves.
        std::vector<Index> chain{edges[seam[0]].a,edges[seam[0]].b};seam.erase(seam.begin());
        bool grew=true;while(grew) {grew=false;for(auto it=seam.begin();it!=seam.end();++it) {
            auto e=edges[*it];
            if(e.a==chain.back())chain.push_back(e.b);else if(e.b==chain.back())chain.push_back(e.a);
            else if(e.a==chain.front())chain.insert(chain.begin(),e.b);else if(e.b==chain.front())chain.insert(chain.begin(),e.a);else continue;
            seam.erase(it);grew=true;break;
        }}
        auto p=best.uv[chain.front()],q=best.uv[chain.back()];if(chain.front()==chain.back() && chain.size()>2)q=best.uv[chain[chain.size()-2]];
        const double length=distance(p,q);if(length<=1e-7)return;
        for(Index i=0;i<original.size();++i)if(std::abs(original[i].y)<=1e-4) {
            const auto v=best.uv[i];double d=std::abs(double(q.x-p.x)*(v.y-p.y)-double(q.y-p.y)*(v.x-p.x))/length;
            if(d>1e-3)return;
        }
        mirror=Plane::fromPoints(p,q,{q.x,q.y,1});if(!mirror.valid())return;
        joined=true;for(auto p:best.uv)bounds(*mirror.mirror(p),lo,hi);
    }
    QJsonArray curves(const Style &style,Job &job) const {
        std::array<QJsonArray,4> output;
        for(const auto &cut:style.cuts) {
            job.check();std::vector<SectionCurve> curves;std::set<Index> coplanarEdges;
            for(Index index=0;index<best.order.size();++index) {
                if((index&255)==0)job.check();
                const auto &f=faces[best.order[index]];const auto &pts=f.points;
                if(std::all_of(pts.begin(),pts.end(),[&](Index p){return std::abs(cut.plane.evaluate(original[p]))<=1e-5;}))continue;
                SectionCurve curve;
                Index a=pts.back();float sa=cut.plane.evaluate(original[a]);
                for(Index j=0;j<pts.size();++j) {
                    auto b=pts[j],e=f.edges[j];float sb=cut.plane.evaluate(original[b]);
                    if((sa < -1e-5 && sb>1e-5) || (sa>1e-5 && sb < -1e-5)) {
                        const float t=-sa/(sb-sa);auto p=best.uv[a],q=best.uv[b];
                        curve.points.push_back({p.x+t*(q.x-p.x),p.y+t*(q.y-p.y),0});curve.knuckles.push_back(edges[e].crease);
                    } else if(std::abs(sa)<=1e-5 && std::abs(sb)<=1e-5) {
                        if(coplanarEdges.insert(e).second)curves.push_back({{best.uv[a],best.uv[b]},{true,true},{},1,false});
                    } else if(std::abs(sb)<1e-5) {curve.points.push_back(best.uv[b]);curve.knuckles.push_back(types[b]!=VertexType::Regular);}
                    a=b;sa=sb;
                }
                if(curve.points.size()>1 && distance(curve.points.front(),curve.points.back())<1e-4) {curve.points.pop_back();curve.knuckles.pop_back();}
                if(curve.points.size()>1) {curve.fragments=curve.points.size()-1;curves.push_back(std::move(curve));}
            }
            if(curves.size()>1) {
                joinSectionCurves(curves,.01f,false);job.check();
                curves.erase(std::remove_if(curves.begin(),curves.end(),[](const SectionCurve &c) {Vec3 a=c.points.front(),b=a;for(auto p:c.points)bounds(p,a,b);return squared(a,b)<1e-3;}),curves.end());
            }
            for(const auto &c:curves)for(int side=0;side<=int(joined);++side) {
                job.check();auto points=c.points;if(side)for(auto &p:points)p=*mirror.mirror(p);
                Spline spline(points,c.knuckles,std::max(c.fragments,points.size()-1));QJsonArray row;
                for(Index i=0;i<=spline.fragments();++i)row.append(xy(display(spline.value(double(i)/std::max(Index(1),spline.fragments())))));
                output[cut.kind-1].append(QJsonObject{{"kind",cut.kind},{"color",cut.color},{"dxf_color",cut.dxf},{"points",row}});
            }
        }
        QJsonArray out;for(auto &rows:output)for(auto row:rows)out.append(row);return out;
    }
    QJsonObject snapshot(const QString &name,int color,int dxf,const Style &style,Job &job) const {
        QJsonArray pts,orig,es,fs,corners;const auto n=original.size();
        for(int side=0;side<=int(joined);++side) {
            const auto offset=side*n;
            for(Index i=0;i<n;++i) {
                if((i&255)==0)job.check();
                auto p=best.uv[i],v=original[i];if(side) {p=*mirror.mirror(p);v.y=-v.y;}
                pts.append(xy(display(p)));orig.append(xyz(v));
                if(pointFaces[i].size()==1 || types[i]==VertexType::Corner)corners.append(qint64(i+offset));
            }
            for(Index i=0;i<edges.size();++i) {const auto &e=edges[i];es.append(QJsonArray{qint64(e.a+offset),qint64(e.b+offset),finite(best.errors[i]),e.boundary});}
            for(auto f:best.order) {QJsonArray row;for(auto p:faces[f].points)row.append(qint64(p+offset));fs.append(row);}
        }
        const auto range=std::minmax_element(best.errors.begin(),best.errors.end());
        return {{"name",name},{"color",color},{"dxf_color",dxf},{"joined_mirror",joined},{"reference_area",finite(best.reference)},
            {"signed_area_error",finite(best.signedArea)},{"absolute_area_error",finite(best.absolute)},{"max_face_area_error",finite(best.maxArea)},
            {"min_edge_error",finite(*range.first)},{"max_edge_error",finite(*range.second)},{"normalized_error",finite(best.normalized)},{"iterations",iterations},
            {"points",pts},{"original",orig},{"edges",es},{"faces",fs},{"corners",corners},{"curves",curves(style,job)}};
    }
};
}

QJsonObject developPlates(const Fbm &source,const QJsonObject &bindings,const QJsonObject &context,const QString &cancelFile) {
    Job job(cancelFile);const auto input=documentGeometry(source);Style style(source,bindings,context);
    SubdivisionSurface surface(input);const auto &mesh=surface.level(input.desiredLevel);job.check();
    std::map<Key,Index> meshEdges;for(Index e=0;e<mesh.edges.size();++e)meshEdges.emplace(key(mesh.edges[e].points[0],mesh.edges[e].points[1]),e);
    std::map<Key,std::vector<Index>> controlEdges;
    for(Index f=0;f<input.faces.size();++f) {const auto &p=input.faces[f].points;for(Index i=0;i<p.size();++i)controlEdges[key(p[(i+p.size()-1)%p.size()],p[i])].push_back(f);}
    std::vector<std::vector<Index>> children(input.faces.size());for(Index f=0;f<mesh.faces.size();++f)children[mesh.faces[f].rootControlFace].push_back(f);
    QJsonArray plates;bool first=true;float keel=mesh.minimum.z;
    for(const auto &face:mesh.faces)if(input.layers[face.layer].useInHydrostatics)for(auto p:face.points) {const float z=mesh.points[p].position.z;if(first || z<keel)keel=z;first=false;}
    const auto orders=bindings["layer_faces"].toArray(),ids=bindings["faces"].toArray();std::map<qint64,Index> byId;
    for(qsizetype i=0;i<ids.size();++i)byId.emplace(ids[i].toInteger(),Index(i));
    for(Index l=0;l<input.layers.size();++l) {
        const auto &layer=input.layers[l];if(!layer.developable)continue;job.check();
        std::vector<Index> order;
        if(l<Index(orders.size()))for(auto id:orders[qsizetype(l)].toArray())order.push_back(byId.at(id.toInteger()));
        else for(Index f=0;f<input.faces.size();++f)if(input.faces[f].layer==l)order.push_back(f);
        std::vector<bool> todo(input.faces.size());for(auto f:order)todo[f]=true;
        std::vector<std::vector<Index>> groups;
        for(auto start=order.rbegin();start!=order.rend();++start)if(todo[*start]) {
            std::vector<Index> queue{*start};todo[*start]=false;
            for(Index i=0;i<queue.size();++i) {
                if((i&255)==0)job.check();
                const auto &p=input.faces[queue[i]].points;
                for(Index j=0;j<p.size();++j)for(auto f:controlEdges.at(key(p[(j+p.size()-1)%p.size()],p[j])))if(todo[f]) {todo[f]=false;queue.push_back(f);}
            }
            std::vector<Index> selected;for(auto f:queue)selected.insert(selected.end(),children[f].begin(),children[f].end());
            std::sort(selected.begin(),selected.end());if(!selected.empty())groups.push_back(std::move(selected));
        }
        // FDescription may be empty in saved/new layers. Match FGetName rather
        // than emitting an empty label that the Qt plate parser must reject.
        const auto base=layer.name.empty() ? bindings["layer_name_prefix"].toString()+" "+QString::number(layer.layerId)
            :decodeFbmText(QByteArray::fromStdString(layer.name),style.encoding);
        for(Index i=0;i<groups.size();++i) {
            Patch patch(mesh,groups[i],meshEdges,job);patch.unroll(layer.symmetric,job);
            const auto name=base+(groups.size()==1?QString():" "+style.part.toLower()+" "+QString::number(i+1));
            const auto color=style.rgb(layer.color),dxf=style.dxf(layer.color);
            if(layer.symmetric && !patch.joined) {
                plates.append(patch.snapshot(name+" (SB)",color,dxf,style,job));
                for(auto &p:patch.best.uv)p.y=-p.y;
                patch.rotation(-patch.angle);patch.rawBounds();
                plates.append(patch.snapshot(name+" (P)",color,dxf,style,job));
            } else plates.append(patch.snapshot(name,color,dxf,style,job));
        }
    }
    if(plates.isEmpty())bad("没有可展开的板片。请在图层属性中勾选“可展开”，并确认图层含有曲面。");
    job.check();return {{"schema",1},{"units",style.units},{"waterline",finite(float(style.draft+keel))},
        {"underwater_color",style.underwater},{"shade_submerged",style.shade},{"plates",plates},{"analysis_engine","cpp-plates"},{"calculation_ms",job.timer.nsecsElapsed()/1e6}};
}
}
