// GPL-2.0-or-later. Adapted from FREE!ship Plus FreeFlowline.inc and
// FreeGometry_Functions.inc (van Engeland, Timoshenko, Malakanov).
// Keep the geometric tracing/Single precision contract. Unlike the original
// Lines3DIntersect, edge intersection always initializes its interpolation
// parameter. Shared normals, triangles and adjacency are built once per mesh.
#include "flowlines.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>

namespace fs::native_geometry {
namespace {
using Index=std::size_t;
using Edge=std::pair<Index,Index>;
Vec3 sub(Vec3 a,Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 normalize(Vec3 p) {
    float n=std::sqrt(dot(p,p));if (std::isnan(n)) return {};
    n=std::max(n,1e-6f);return {p.x/n,p.y/n,p.z/n};
}
Vec3 legacyAdd(Vec3 a,Vec3 b) {return {a.x+b.x,std::max(a.y+b.y,0.f),a.z+b.z};}
Vec3 interpolate(Vec3 a,Vec3 b,double t) {
    return {float(a.x+t*(b.x-a.x)),float(a.y+t*(b.y-a.y)),float(a.z+t*(b.z-a.z))};
}
bool sameSide(Vec3 p,Vec3 other,Vec3 a,Vec3 b) {
    const auto edge=sub(b,a);const auto d=dot(cross(sub(p,a),edge),cross(sub(other,a),edge));
    return std::abs(double(d))<1e-5 || d>=0;
}
bool inside(Vec3 p,Vec3 a,Vec3 b,Vec3 c) {
    return sameSide(p,a,b,c)&&sameSide(p,b,a,c)&&sameSide(p,c,a,b);
}
bool intersect(Vec3 a,Vec3 b,Vec3 c,Vec3 d,double &edgeParameter,Vec3 &point) {
    const auto e=sub(d,c),v=sub(b,a),w=sub(a,c);
    const double ee=dot(e,e),vv=dot(v,v),we=dot(w,e),ev=dot(e,v),wv=dot(w,v);
    if (ee<1e-12 || vv<1e-12) return false;
    const double denominator=vv*ee-ev*ev;
    if (std::abs(denominator)<1e-6) return false;
    const double along=(we*ev-wv*ee)/denominator;
    const double across=(we+ev*along)/ee;
    if (!(along>-1e-6 && along<1 && across>-1e-6 && across<1)) return false;
    // The old helper never assigned Param. The caller uses it to interpolate
    // the flow directions at c/d; therefore it is mub, not mua.
    edgeParameter=across;point=interpolate(a,b,along);return true;
}
Edge edgeKey(Index a,Index b) {return std::minmax(a,b);}
struct Triangle {
    std::array<Index,3> points;
    Plane plane;
    std::array<int,3> adjacent{{-1,-1,-1}};
};
struct Tracer {
    const SubdivisionLevel &mesh;
    const std::vector<Vec3> &normals;
    std::vector<Vec3> directions;
    std::vector<Triangle> triangles;
    std::vector<unsigned> visited;
    float waterline=0;
    Tracer(const DocumentInput &d,const SubdivisionLevel &m,const std::vector<Vec3> &n):mesh(m),normals(n) {
        if (n.size()!=m.points.size()) throw std::invalid_argument("Missing flow normals");
        std::vector<std::vector<Index>> children(d.geometry.faces.size());
        bool first=true;float lowest=m.minimum.z;
        for (Index i=0;i<m.faces.size();++i) {
            const auto &f=m.faces[i];children.at(f.rootControlFace).push_back(i);
            if (m.layers.at(f.layer).useInHydrostatics)
                for (auto p:f.points) {
                    auto z=m.points[p].position.z;if (first || z<lowest) lowest=z;first=false;
                }
        }
        waterline=lowest+d.draft;
        std::map<Edge,std::vector<std::pair<int,int>>> edges;
        for (Index l=0;l<d.layerFaces.size();++l) if (m.layers.at(l).useInHydrostatics)
            for (auto root:d.layerFaces[l]) for (auto child:children.at(root)) {
                const auto &f=m.faces[child];
                if (std::none_of(f.points.begin(),f.points.end(),[&](auto p){return m.points[p].position.z<=waterline;})) continue;
                for (Index k=2;k<f.points.size();++k) {
                    Triangle t{{f.points[0],f.points[k-1],f.points[k]}, {}};
                    t.plane=Plane::fromPoints(position(t,0),position(t,1),position(t,2));
                    // A zero-area face supplies no ray hit or usable tangent.
                    if (!t.plane.valid()) continue;
                    if (triangles.size()>=5000000) throw std::runtime_error("Too many flow triangles");
                    const int index=int(triangles.size());triangles.push_back(t);
                    for (int j=0;j<3;++j) edges[edgeKey(t.points[j],t.points[(j+1)%3])].push_back({index,j});
                }
            }
        // Preserve the first neighbouring face on a non-manifold edge, but
        // avoid re-scanning two point-to-triangle lists at every trace step.
        for (const auto &[edge,uses]:edges) {
            Q_UNUSED(edge);
            for (auto [index,slot]:uses) for (auto [other,unused]:uses) {
                Q_UNUSED(unused);
                if (other!=index) {triangles[index].adjacent[slot]=other;break;}
            }
        }
        directions.resize(m.points.size());std::vector<bool> built(m.points.size(),false);
        for (const auto &t:triangles) for (auto p:t.points) if (!built[p]) {
            built[p]=true;const auto position=m.points[p].position;
            const auto plane=Plane::fromPointNormal(position,n[p]);
            const auto incoming=legacyAdd(position,normalize(legacyAdd(n[p],{-1,0,0})));
            directions[p]=normalize(sub(plane.project(incoming).value_or(Vec3{}),position));
        }
        visited.resize(triangles.size());
    }
    Vec3 position(const Triangle &t,int i) const {return mesh.points[t.points[i]].position;}
    int initial(Vec3 start,Vec3 end,Vec3 &hit,Vec3 &direction) const {
        int best=-1;double distance=1e8;
        for (Index i=0;i<triangles.size();++i) {
            const auto &t=triangles[i];const float a=t.plane.evaluate(start),b=t.plane.evaluate(end);
            if (!((a<0 && b>0)||(a>0 && b<0))) continue;
            const float fraction=-a/(b-a);
            // Ray interpolation is Single, unlike the Double edge parameter.
            const Vec3 p{start.x+fraction*(end.x-start.x),start.y+fraction*(end.y-start.y),start.z+fraction*(end.z-start.z)};
            if (!inside(p,position(t,0),position(t,1),position(t,2))) continue;
            const auto delta=sub(start,p);const float length=std::sqrt(dot(delta,delta));
            if (length<distance) {distance=length;best=int(i);hit=p;}
        }
        if (best<0) return best;
        const auto &t=triangles[best];const auto u=sub(position(t,1),position(t,0)),
            v=sub(position(t,2),position(t,0)),w=sub(hit,position(t,0));
        const double uu=dot(u,u),uv=dot(u,v),vv=dot(v,v),wu=dot(w,u),wv=dot(w,v),den=uv*uv-uu*vv;
        if (den==0) return -1;
        const float s=float((uv*wv-vv*wu)/den),r=float((uv*wu-uu*wv)/den);
        const double b0=1.f-s-r,b1=s,b2=r;
        if (float(b0+b1+b2)==1) {
            const auto p=directions[t.points[0]],q=directions[t.points[1]],z=directions[t.points[2]];
            direction={float(b0*p.x+b1*q.x+b2*z.x),float(b0*p.y+b1*q.y+b2*z.y),float(b0*p.z+b1*q.z+b2*z.z)};
        }
        return best;
    }
    bool step(int current,Edge &skip,Vec3 &hit,Vec3 &direction,int &next,unsigned generation) {
        const auto &t=triangles[current];visited[current]=generation;next=current;
        const auto projected=t.plane.project(hit).value_or(Vec3{});
        Vec3 a{float(projected.x+.0005*direction.x),float(projected.y+.0005*direction.y),float(projected.z+.0005*direction.z)};
        if (!inside(a,position(t,0),position(t,1),position(t,2))) a=projected;
        a=t.plane.project(a).value_or(Vec3{});
        auto b=t.plane.project({a.x+50.f*direction.x,a.y+50.f*direction.y,a.z+50.f*direction.z}).value_or(Vec3{});
        for (int i=0;i<3;++i) {
            const auto p=t.points[i],q=t.points[(i+1)%3];const auto edge=edgeKey(p,q);
            if (edge==skip) continue;
            Vec3 intersection;double parameter=0;
            if (!intersect(a,b,mesh.points[p].position,mesh.points[q].position,parameter,intersection) ||
                !(t.plane.evaluate(intersection)<.1)) continue;
            hit=intersection;direction=interpolate(directions[p],directions[q],parameter);
            skip=edge;next=t.adjacent[i];return true;
        }
        return false;
    }
    std::vector<Vec3> trace(const FlowInput &flow,unsigned generation) {
        Vec3 start,end;const auto lo=mesh.minimum,hi=mesh.maximum;
        if (flow.view==1) {start={flow.x,hi.y+10,flow.y};end={start.x,0,start.z};}
        else if (flow.view==2) {start={flow.x,flow.y,lo.z-10};end={start.x,start.y,hi.z+100};}
        else if (flow.x<0) {start={lo.x-10,-flow.x,flow.y};end={hi.x+10,start.y,start.z};}
        else {start={hi.x+10,flow.x,flow.y};end={lo.x-10,start.y,start.z};}
        Vec3 hit{},direction{};int index=initial(start,end,hit,direction);
        std::vector<Vec3> result;if (index<0) return result;
        result.push_back(hit);Edge skip{Index(-1),Index(-1)};
        for (int iteration=0;iteration<=5000;++iteration) {
            bool valid=visited[index]!=generation && step(index,skip,hit,direction,index,generation);
            if (valid) result.push_back(hit);
            else valid=step(index,skip,hit,direction,index,generation);
            if (!valid || index<0) break;
        }
        while (result.size()>1) {
            auto &last=result.back();const auto previous=result[result.size()-2];
            if (last.z>waterline && previous.z>waterline) result.pop_back();
            else if (last.z>waterline && previous.z<waterline) {
                last={previous.x+(last.x-previous.x)*(waterline-previous.z)/(last.z-previous.z),
                      previous.y+(last.y-previous.y)*(waterline-previous.z)/(last.z-previous.z),waterline};
            } else break;
        }
        return result;
    }
};
}
QJsonArray buildFlowlines(const DocumentInput &document,const SubdivisionLevel &mesh,const std::vector<Vec3> &normals) {
    QJsonArray result;if (document.flows.empty()) return result;
    Tracer tracer(document,mesh,normals);std::size_t count=0;unsigned generation=0;
    for (const auto &flow:document.flows) {
        auto trace=tracer.trace(flow,++generation);count+=trace.size();
        if (count>1000000) throw std::runtime_error("Too many flowline points");
        QJsonArray points,knuckles;
        for (auto p:trace) {
            for (float n:{p.x,p.y,p.z}) if (!std::isfinite(n)||std::abs(n)>1e9f) throw std::runtime_error("Invalid flowline coordinate");
            points.append(QJsonArray{double(p.x),double(p.y),double(p.z)});knuckles.append(false);
        }
        result.append(QJsonObject{{"id",flow.id},{"spline",QJsonObject{{"points",points},{"knuckles",knuckles}}}});
    }
    return result;
}
}
