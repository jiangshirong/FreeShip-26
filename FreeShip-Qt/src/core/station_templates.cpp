// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_templates.h"
#include "station_shell.h"
#include "hull_sections.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace fs::frames {
namespace {
namespace ng=fs::native_geometry;
using Id=size_t;
using Key=std::pair<Id,Id>; // vertex (i,i) or canonical edge (i,j)
Key edgeKey(Id a,Id b) { return std::minmax(a,b); }
Id physicalVertex(const ng::SubdivisionLevel& mesh,Id i,bool mirror) {
    return i*2+(mirror && mesh.points.at(i).position.y!=0);
}
ng::Vec3 physicalPoint(const ng::SubdivisionLevel& mesh,Id id) {
    auto p=mesh.points.at(id/2).position;
    if(id%2) p.y=-p.y;
    return p;
}
struct Node {
    QPointF position; // Y,Z in mm
    bool knuckle=false;
    std::vector<Id> neighbours;
};
class Contours {
public:
    Id node(Key key,QPointF p,bool knuckle) {
        const auto [it,added]=lookup.emplace(key,nodes.size());
        if(added) nodes.push_back({p,knuckle,{}});
        else nodes[it->second].knuckle|=knuckle;
        return it->second;
    }
    void link(Id a,Id b) {
        if(a==b || !edges.insert(edgeKey(a,b)).second) return;
        nodes[a].neighbours.push_back(b); nodes[b].neighbours.push_back(a);
    }
    QList<QPolygonF> paths(bool smooth,double units) const {
        std::set<Key> visited;
        QList<QPolygonF> result;
        for(const auto& node:nodes) if(node.neighbours.size()>2)
            throw std::runtime_error("剖面轮廓存在分叉，请单独选择船壳图层或检查连接。");
        // Walk open paths from an endpoint, then closed paths from any node.
        for(int pass=0;pass<2;++pass) for(Id start=0;start<nodes.size();++start) {
            const auto& links=nodes[start].neighbours;
            if(links.empty() || (pass==0 && links.size()!=1) ||
               visited.count(edgeKey(start,links.front()))) continue;
            QPolygonF line;
            ng::SectionCurve curve;
            Id current=start;
            while(true) {
                const auto& n=nodes[current];
                if(line.isEmpty() || n.position!=line.back()) {
                    line.append(n.position);
                    if(smooth) {
                        curve.points.push_back({0,float(n.position.x()/units),float(n.position.y()/units)});
                        curve.knuckles.push_back(n.knuckle);
                    }
                }
                auto next=std::find_if(n.neighbours.begin(),n.neighbours.end(),[&](Id v){
                    return !visited.count(edgeKey(current,v));
                });
                if(next==n.neighbours.end()) break;
                visited.insert(edgeKey(current,*next)); current=*next;
            }
            if(line.size()<2) continue;
            if(smooth && line.size()>2) {
                line.clear();
                for(auto p:hull::sampleSection(curve,.02/units)) line.append(QPointF(p[0]*units,p[1]*units));
            }
            result.append(line);
        }
        if(result.isEmpty()) throw std::runtime_error("该站与所选船壳没有有效交线。");
        return result;
    }
private:
    std::map<Key,Id> lookup;
    std::vector<Node> nodes;
    std::set<Key> edges;
};

double cross(QPointF a,QPointF b) { return a.x()*b.y()-a.y()*b.x(); }
void validate(const QList<QPolygonF>& paths) {
    struct Segment { QPointF a,b; int path,offset; double left,right; };
    std::vector<Segment> segments;
    for(int k=0;k<paths.size();++k) for(int i=1;i<paths[k].size();++i) {
        auto a=paths[k][i-1],b=paths[k][i];
        if(a==b) throw std::runtime_error("剖面轮廓包含零长度边。");
        segments.push_back({a,b,k,i,std::min(a.x(),b.x()),std::max(a.x(),b.x())});
    }
    std::sort(segments.begin(),segments.end(),[](auto& a,auto& b){return a.left<b.left;});
    for(size_t i=0;i<segments.size();++i) {
        const auto& a=segments[i];
        for(size_t j=i+1;j<segments.size() && segments[j].left<=a.right;++j) {
            const auto& b=segments[j];
            if(std::max(a.a.y(),a.b.y())<std::min(b.a.y(),b.b.y()) ||
               std::max(b.a.y(),b.b.y())<std::min(a.a.y(),a.b.y())) continue;
            const auto u=a.b-a.a,v=b.b-b.a;
            if(cross(u,b.a-a.a)*cross(u,b.b-a.a)>0 || cross(v,a.a-b.a)*cross(v,a.b-b.a)>0) continue;
            const auto& path=paths[a.path];
            const int gap=std::abs(a.offset-b.offset);
            if(a.path==b.path && (gap==1 || (path.isClosed() && gap==path.size()-2))) {
                // Adjacent edges may meet, but must not double back over the same line.
                if(cross(u,v)!=0 || QPointF::dotProduct(u,v)>0) continue;
            }
            throw std::runtime_error("剖面轮廓相交、接触或重叠，请检查船壳图层和连接。");
        }
    }
}
void closeTops(QList<QPolygonF>& paths,bool allow) {
    for(auto& path:paths) {
        if(path.isClosed()) continue;
        if(!allow) throw std::runtime_error("该站舷顶开放。要使用直线封顶，请勾选「舷顶直线封合」。");
        QPointF a=path.front(),b=path.back();
        if(a.x()>b.x()) std::swap(a,b);
        auto chord=b-a;
        if(chord.x()<.001 || std::any_of(path.begin(),path.end(),[&](QPointF p){
            return cross(chord,p-a)>.001*std::hypot(chord.x(),chord.y());
        })) throw std::runtime_error("断口不在舷顶，不能自动封合，请检查船壳连接。");
        path.append(path.front());
    }
    validate(paths);
    for(int i=0;i<paths.size();++i) for(int j=0;j<paths.size();++j)
        if(i!=j && paths[i].containsPoint(paths[j].front(),Qt::OddEvenFill))
            throw std::runtime_error("所选轮廓相互嵌套，请单独选择外板图层。");
}

std::vector<bool> reflectedLayers(const ng::SubdivisionLevel& mesh) {
    std::vector<int> sides(mesh.layers.size());
    for(const auto& face:mesh.faces) for(Id id:face.points) {
        const float y=mesh.points[id].position.y;
        sides[face.layer]|= y>0 ? 1 : (y<0 ? 2 : 0);
    }
    std::vector<bool> reflect(sides.size());
    for(Id i=0;i<sides.size();++i) reflect[i]=mesh.layers[i].symmetric && (sides[i]==1 || sides[i]==2);
    return reflect;
}
bool onCentre(const ng::SubdivisionLevel& mesh,const ng::SubdivisionFace& face) {
    return std::all_of(face.points.begin(),face.points.end(),[&](Id i){return mesh.points[i].position.y==0;});
}
// A boundary of a half transom becomes interior when its face is mirrored.
// A transverse cut through a keel or a non-coplanar face must retain its edges.
bool mirroredSeam(const ng::SubdivisionLevel& mesh,const ng::SectionSegment& segment,
                  const ng::SubdivisionFace& face) {
    if(segment.start.edge!=segment.end.edge || !segment.start.vertex || !segment.end.vertex ||
       segment.start.position.y!=0 || segment.end.position.y!=0) return false;
    const auto& edge=mesh.edges[segment.start.edge];
    if(edge.faces.size()!=1) return false;
    // SectionIndex has already classified this edge as lying on the station.
    const float x=segment.start.position.x;
    return std::all_of(face.points.begin(),face.points.end(),[&](Id i){return mesh.points[i].position.x==x;});
}
QList<QPolygonF> outerLines(const ng::SectionIndex& index,const ng::SubdivisionLevel& mesh,
                            const std::vector<bool>& mirrors,const ng::Plane& plane,double units) {
    const auto cut=index.intersect(plane);
    Contours graph;
    for(const auto& segment:cut.segments) {
        const auto& face=mesh.faces[segment.face];
        if(mirrors[face.layer] && (onCentre(mesh,face) || mirroredSeam(mesh,segment,face))) continue;
        for(int side=0;side<=(mirrors[face.layer]?1:0);++side) {
            auto endpoint=[&](const ng::SectionEndpoint& e){
                auto ids=mesh.edges[e.edge].points;
                if(e.vertex) ids={*e.vertex,*e.vertex};
                auto key=edgeKey(physicalVertex(mesh,ids[0],side),physicalVertex(mesh,ids[1],side));
                return graph.node(key,{(side?-1:1)*e.position.y*units,e.position.z*units},e.knuckle);
            };
            const auto a=endpoint(segment.start),b=endpoint(segment.end);
            graph.link(a,b);
        }
    }
    auto result=graph.paths(true,units);
    validate(result);
    return result;
}

struct IndexedTriangle { std::array<Id,3> ids; double minX,maxX; };
std::vector<IndexedTriangle> shellMesh(const ng::SubdivisionLevel& mesh,const std::vector<bool>& mirrors,
                                      const ng::GeometryInput& input,double units) {
    std::vector<IndexedTriangle> triangles;
    for(const auto& face:mesh.faces) {
        if(!input.layers[face.layer].useForIntersections || (mirrors[face.layer] && onCentre(mesh,face))) continue;
        for(int side=0;side<=(mirrors[face.layer]?1:0);++side) {
            for(Id i=2;i<face.points.size();++i) {
                std::array<Id,3> ids{face.points.front(),face.points[i-1],face.points[i]};
                for(auto& id:ids) id=physicalVertex(mesh,id,side);
                const auto x=[&](int j){return physicalPoint(mesh,ids[j]).x*units;};
                triangles.push_back({ids,std::min({x(0),x(1),x(2)}),std::max({x(0),x(1),x(2)})});
            }
        }
    }
    return triangles;
}
QList<QPolygonF> innerLines(const ng::SubdivisionLevel& mesh,const std::vector<IndexedTriangle>& triangles,
                            double units,double x,const Options& options,const std::atomic_bool& cancelled) {
    Contours graph;
    std::map<Key,int> coplanar;
    std::vector<ShellTriangle> nearby;
    auto point=[&](Id id){
        auto p=physicalPoint(mesh,id);return ShellPoint{p.x*units,{p.y*units,p.z*units}};
    };
    auto vertex=[&](Id id){return graph.node({id,id},point(id).yz,true);};
    for(const auto& face:triangles) {
        if(face.minX>x+options.thickness || face.maxX<x-options.thickness) continue;
        if(cancelled.load()) return {};
        const auto& v=face.ids;
        ShellTriangle tri{point(v[0]),point(v[1]),point(v[2])};
        nearby.push_back(tri);
        if(x<face.minX || x>face.maxX) continue;
        if(face.minX==x && face.maxX==x) {
            for(int i=0;i<3;++i) ++coplanar[edgeKey(v[i],v[(i+1)%3])];
            continue;
        }
        std::vector<Id> hits;
        for(int i=0;i<3;++i) {
            const int j=(i+1)%3;
            if(tri[i].x==x) hits.push_back(vertex(v[i]));
            if((tri[i].x<x && tri[j].x>x) || (tri[j].x<x && tri[i].x>x)) {
                double u=(x-tri[i].x)/(tri[j].x-tri[i].x);
                hits.push_back(graph.node(edgeKey(v[i],v[j]),tri[i].yz+(tri[j].yz-tri[i].yz)*u,false));
            }
        }
        if(hits.size()==2) graph.link(hits[0],hits[1]);
    }
    for(const auto& [key,count]:coplanar) if(count==1) graph.link(vertex(key.first),vertex(key.second));
    auto contours=graph.paths(false,units);
    closeTops(contours,options.closeSheer);
    auto result=innerContours(contours,nearby,x,options.thickness,cancelled);
    validate(result);
    return result;
}
}
QList<Station> sectionTemplates(const ng::DocumentInput& document,const Options& options,const std::atomic_bool& cancelled) {
    if(options.indices.isEmpty() || !std::isfinite(options.thickness) || options.thickness<0)
        throw std::invalid_argument("请选择横剖站并输入有效板厚。");
    auto input=document.geometry;
    if(options.layer < -1 || options.layer>=int(input.layers.size())) throw std::invalid_argument("图层已更改。");
    if(options.layer>=0) for(int i=0;i<int(input.layers.size());++i) input.layers[i].useForIntersections=(i==options.layer);
    const double units=document.imperial?304.8:1000.;
    // Exterior lines follow the project's own precision and native section splines.
    // The thickness envelope uses the finest supported subdivision of the same FBM.
    const unsigned level=options.thickness>0?4:input.desiredLevel;
    ng::SubdivisionSurface surface(std::move(input));
    if(cancelled.load()) return {};
    const auto& mesh=surface.level(level);
    const auto mirrors=reflectedLayers(mesh);
    std::unique_ptr<ng::SectionIndex> index;
    std::vector<IndexedTriangle> triangles;
    if(options.thickness==0) index=std::make_unique<ng::SectionIndex>(surface.input(),mesh);
    else triangles=shellMesh(mesh,mirrors,surface.input(),units);
    QList<Station> results;
    for(const auto& section:document.sections) {
        if(cancelled.load()) return {};
        if(section.kind!=1 || !options.indices.contains(section.order)) continue;
        const auto p=section.plane.coefficients();
        if(p[0]!=1 || p[1]!=0 || p[2]!=0) throw std::runtime_error("横剖站的平面定义不正确。");
        Station station;station.number=section.order+1;station.x=-double(p[3])*units;
        try {
            station.outlines=index?outerLines(*index,mesh,mirrors,section.plane,units):
                innerLines(mesh,triangles,units,station.x,options,cancelled);
        } catch(const std::runtime_error& e) { station.error=QString::fromUtf8(e.what()); }
        results.append(std::move(station));
    }
    if(results.size()!=options.indices.size()) throw std::runtime_error("所选横剖站已更改，请重新选择。");
    return results;
}
}

