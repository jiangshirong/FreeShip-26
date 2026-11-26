// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_templates.h"
#include "station_shell.h"
#include "hull_sections.h"
#include "native/subdivision_faces.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <thread>
#include <map>
#include <set>
#include <stdexcept>

namespace fs::frames {
namespace {
namespace ng=fs::native_geometry;
using Id=size_t;
using Key=std::pair<Id,Id>; // vertex (i,i) or canonical edge (i,j)
Key edgeKey(Id a,Id b) { return std::minmax(a,b); }
// Ignore sub-nanometre noise at the symmetry plane, below the cutting grid.
constexpr double centrelineToleranceMm=1e-6;
float centreY(float y,double tolerance) { return std::abs(y)<=tolerance ? 0.f : y; }
Id physicalVertex(const std::vector<ng::SubdivisionPoint>& points,Id i,bool mirror,double tolerance) {
    return i*2+(mirror && centreY(points.at(i).position.y,tolerance)!=0);
}
ng::Vec3 physicalPoint(const std::vector<ng::SubdivisionPoint>& points,Id id,double tolerance) {
    auto p=points.at(id/2).position;
    p.y=centreY(p.y,tolerance);
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
    QList<QPolygonF> paths(bool smooth,double units,QList<QPolygonF>& diagnostic) const {
        std::set<Key> visited;
        QList<QPolygonF> result;
        for(const auto& node:nodes) if(node.neighbours.size()>2) {
            for(const auto& [a,b]:edges) if(nodes[a].position!=nodes[b].position)
                diagnostic.append(QPolygonF(QList<QPointF>{nodes[a].position,nodes[b].position}));
            throw std::runtime_error("剖面轮廓存在分叉，请单独选择船壳图层或检查连接。");
        }
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
void closeTops(QList<QPolygonF>& paths) {
    for(auto& path:paths) {
        if(path.isClosed()) continue;
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

// Thickness subtraction and slot intersection need an area. Discard only
// segments on its temporary top chord before exposing paths to the exporters.
QList<QPolygonF> withoutTop(const QPolygonF& loop,QPointF a,QPointF b) {
    const auto chord=b-a;
    const double length=std::hypot(chord.x(),chord.y());
    constexpr double tolerance=.0003; // Intersection rounding on the 0.0001 mm clipping grid.
    auto onTop=[&](QPointF p) {
        const double along=QPointF::dotProduct(chord,p-a)/length;
        return along>=-tolerance && along<=length+tolerance &&
               std::abs(cross(chord,p-a))<=length*tolerance;
    };
    const int count=loop.size()-1;
    int first=0;
    while(first<count && !(onTop(loop[first]) && onTop(loop[first+1])))++first;
    if(first==count)return {loop}; // The shell itself may close below the sheer.
    QList<QPolygonF> paths;
    QPolygonF run;
    for(int k=1;k<=count;++k){
        const int i=(first+k)%count;
        if(onTop(loop[i]) && onTop(loop[i+1])){
            if(!run.isEmpty()){paths.append(std::move(run));run.clear();}
        }else{
            if(run.isEmpty())run.append(loop[i]);
            run.append(loop[i+1]);
        }
    }
    return paths;
}

std::vector<bool> reflectedLayers(const std::vector<ng::SubdivisionPoint>& points,
                                  const std::vector<ng::SubdivisionFace>& faces,
                                  const std::vector<ng::LayerInput>& layers,double tolerance) {
    std::vector<int> sides(layers.size());
    for(const auto& face:faces) for(Id id:face.points) {
        const float y=centreY(points[id].position.y,tolerance);
        sides[face.layer]|= y>0 ? 1 : (y<0 ? 2 : 0);
    }
    std::vector<bool> reflect(sides.size());
    for(Id i=0;i<sides.size();++i) reflect[i]=layers[i].symmetric && (sides[i]==1 || sides[i]==2);
    return reflect;
}
bool onCentre(const std::vector<ng::SubdivisionPoint>& points,const ng::SubdivisionFace& face,double tolerance) {
    return std::all_of(face.points.begin(),face.points.end(),[&](Id i){return centreY(points[i].position.y,tolerance)==0;});
}
// A boundary of a half transom becomes interior when its face is mirrored.
// A transverse cut through a keel or a non-coplanar face must retain its edges.
bool mirroredSeam(const ng::SubdivisionLevel& mesh,const ng::SectionSegment& segment,
                  const ng::SubdivisionFace& face,double tolerance) {
    if(segment.start.edge!=segment.end.edge || !segment.start.vertex || !segment.end.vertex ||
       centreY(segment.start.position.y,tolerance)!=0 || centreY(segment.end.position.y,tolerance)!=0) return false;
    const auto& edge=mesh.edges[segment.start.edge];
    if(edge.faces.size()!=1) return false;
    // SectionIndex has already classified this edge as lying on the station.
    const float x=segment.start.position.x;
    return std::all_of(face.points.begin(),face.points.end(),[&](Id i){return mesh.points[i].position.x==x;});
}
void outerLines(const ng::SectionIndex& index,const ng::SubdivisionLevel& mesh,
                const std::vector<bool>& mirrors,const ng::Plane& plane,double units,double tolerance,Station& station) {
    const auto cut=index.intersect(plane);
    Contours graph;
    for(const auto& segment:cut.segments) {
        const auto& face=mesh.faces[segment.face];
        if(mirrors[face.layer] && (onCentre(mesh.points,face,tolerance) || mirroredSeam(mesh,segment,face,tolerance))) continue;
        for(int side=0;side<=(mirrors[face.layer]?1:0);++side) {
            auto endpoint=[&](const ng::SectionEndpoint& e){
                auto ids=mesh.edges[e.edge].points;
                if(e.vertex) ids={*e.vertex,*e.vertex};
                auto key=edgeKey(physicalVertex(mesh.points,ids[0],side,tolerance),physicalVertex(mesh.points,ids[1],side,tolerance));
                return graph.node(key,{(side?-1:1)*centreY(e.position.y,tolerance)*units,e.position.z*units},e.knuckle);
            };
            const auto a=endpoint(segment.start),b=endpoint(segment.end);
            graph.link(a,b);
        }
    }
    auto& preview=station.diagnosticOutlines;
    preview=graph.paths(true,units,preview);
    validate(preview);
    station.outlines.swap(preview);
}

struct IndexedTriangle { std::array<Id,3> ids; double minX,maxX; };
std::vector<IndexedTriangle> shellMesh(const ng::SubdivisionFaces& mesh,const std::vector<bool>& mirrors,
                                      const ng::GeometryInput& input,double units,double tolerance) {
    std::vector<IndexedTriangle> triangles;
    for(const auto& face:mesh.faces) {
        if(!input.layers[face.layer].useForIntersections || (mirrors[face.layer] && onCentre(mesh.points,face,tolerance))) continue;
        for(int side=0;side<=(mirrors[face.layer]?1:0);++side) {
            for(Id i=2;i<face.points.size();++i) {
                std::array<Id,3> ids{face.points.front(),face.points[i-1],face.points[i]};
                for(auto& id:ids) id=physicalVertex(mesh.points,id,side,tolerance);
                const auto x=[&](int j){return mesh.points[ids[j]/2].position.x*units;};
                triangles.push_back({ids,std::min({x(0),x(1),x(2)}),std::max({x(0),x(1),x(2)})});
            }
        }
    }
    return triangles;
}
void innerLines(const std::vector<ng::SubdivisionPoint>& points,const std::vector<IndexedTriangle>& triangles,
                double units,double tolerance,Station& station,const Options& options,const std::atomic_bool& cancelled) {
    Contours graph;
    std::map<Key,int> coplanar;
    std::vector<ShellTriangle> nearby;
    const double x=station.x;
    auto point=[&](Id id){
        auto p=physicalPoint(points,id,tolerance);return ShellPoint{p.x*units,{p.y*units,p.z*units}};
    };
    auto vertex=[&](Id id){return graph.node({id,id},point(id).yz,true);};
    for(const auto& face:triangles) {
        if(face.minX>x+options.thickness || face.maxX<x-options.thickness) continue;
        if(cancelled.load()) return;
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
    auto& preview=station.diagnosticOutlines;
    preview=graph.paths(false,units,preview);
    auto contours=preview;
    closeTops(contours);
    auto* profile=options.measureShell?&station.shellProfile.emplace():nullptr;
    auto result=innerContours(contours,nearby,x,options.thickness,cancelled,profile);
    validate(result);
    QList<QPolygonF> cutLines;
    for(int i=0;i<result.size();++i){
        const auto& raw=preview[i];
        if(!options.closeSheer && !raw.isClosed()){
            const auto paths=withoutTop(result[i],raw.front(),raw.back());
            if(paths.isEmpty())throw std::runtime_error("扣厚后轮廓消失、分裂或出现内孔，请检查板厚和船壳图层。");
            cutLines.append(paths);
        }else cutLines.append(std::move(result[i]));
    }
    station.outlines=std::move(cutLines);
    station.shellReference.swap(preview);
}
}
StationAssemblyError::StationAssemblyError(int number,const QString& reason,int adjacent)
    :std::runtime_error(QStringLiteral("站 %1：%2").arg(number).arg(reason).toStdString()),
     number(number),adjacent(adjacent),reason(reason){}
QList<Station> interlockFrames(QList<Station> frames,double stock) {
    if(frames.size()<2 || !std::isfinite(stock) || stock<=0)
        throw std::runtime_error("成套模架需要至少两站和有效的模架板厚。");
    const double half=stock/2;
    double low=-INFINITY, high=INFINITY;
    for(int i=0;i<frames.size();++i){
        const auto& f=frames[i];
        auto reject=[&](const QString& why){
            throw StationAssemblyError(f.number,why);
        };
        if(!f.error.isEmpty())reject(f.error);
        if(i && f.x-frames[i-1].x<=stock)
            throw StationAssemblyError(f.number,QStringLiteral("槽位重叠，请增大站距。"),frames[i-1].number);
        if(f.outlines.size()!=1 || f.outlines.first().size()<3)
            reject(QStringLiteral("需要一条穿过中心线的闭合轮廓。"));
        auto poly=f.outlines.first();
        if(!poly.isClosed()){
            if(std::min(poly.first().x(),poly.last().x())>-half ||
               std::max(poly.first().x(),poly.last().x())<half)
                reject(QStringLiteral("中心槽范围超出板片或穿过多个轮廓。"));
            poly.append(poly.first());
        }
        QList<double> cuts{-half,half};
        for(auto p:poly)if(std::abs(p.x())<half)cuts.append(p.x());
        std::sort(cuts.begin(),cuts.end());
        cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
        // Each boundary is linear between vertex abscissae. Check both ends
        // of every interval across the stock, including chines inside the slot.
        for(int k=1;k<cuts.size();++k){
            const double l=cuts[k-1],r=cuts[k],mid=(l+r)/2;
            QList<QPair<double,double>> hits;
            for(int j=1;j<poly.size();++j){
                const auto a=poly[j-1],b=poly[j];
                if(mid<=std::min(a.x(),b.x()) || mid>=std::max(a.x(),b.x()))continue;
                auto z=[&](double y){return a.y()+(b.y()-a.y())*(y-a.x())/(b.x()-a.x());};
                hits.append({z(l),z(r)});
            }
            if(hits.size()!=2)reject(QStringLiteral("中心槽范围超出板片或穿过多个轮廓。"));
            if(hits[0].first+hits[0].second>hits[1].first+hits[1].second)hits.swapItemsAt(0,1);
            low=std::max({low,hits[0].first,hits[0].second});
            high=std::min({high,hits[1].first,hits[1].second});
        }
    }
    if(high-low<=stock)throw std::runtime_error("各站没有足够的共同装配高度。");
    const double seat=(low+high)/2;
    const double left=frames.first().x-stock,right=frames.last().x+stock;
    Station spine;
    QPolygonF rail;
    rail<<QPointF(left,low);
    for(auto& f:frames){
        auto outline=f.outlines.first();
        const auto a=outline.front(),b=outline.back();
        if(a!=b)outline.append(a);
        const double top=outline.boundingRect().bottom()+stock;
        outline=outline.subtracted(QPolygonF(QRectF(QPointF(-half,seat),QPointF(half,top))));
        if(outline.size()<4)throw StationAssemblyError(f.number,QStringLiteral("横板开槽失败。"));
        if(!outline.isClosed())outline.append(outline.first());
        f.outlines=a==b?QList<QPolygonF>{outline}:withoutTop(outline,a,b);
        if(f.outlines.isEmpty())throw StationAssemblyError(f.number,QStringLiteral("横板开槽失败。"));
        const double x=f.x;
        rail<<QPointF(x-half,low)<<QPointF(x-half,seat)<<QPointF(x+half,seat)<<QPointF(x+half,low);
        spine.slotLabels.append({f.number,{x,(seat+high)/2}});
    }
    rail<<QPointF(right,low)<<QPointF(right,high)<<QPointF(left,high);
    rail.append(rail.first());
    spine.outlines.append(rail);
    frames.append(spine);
    return frames;
}
QList<Station> sectionTemplates(const ng::DocumentInput& document,const Options& options,const std::atomic_bool& cancelled) {
    return sectionTemplatesAt(document,document.sections,options,cancelled);
}
namespace {
ng::GeometryInput sectionGeometry(const ng::DocumentInput& document,int layer) {
    auto input=document.geometry;
    if(layer < -1 || layer>=int(input.layers.size()))throw std::invalid_argument("图层已更改。");
    if(layer>=0)for(int i=0;i<int(input.layers.size());++i)input.layers[i].useForIntersections=(i==layer);
    return input;
}
void validateOptions(const Options& options) {
    if(options.indices.isEmpty() || !std::isfinite(options.thickness) || options.thickness<0)
        throw std::invalid_argument("请选择横剖站并输入有效板厚。");
}
}

struct StationGeometry::Prepared {
    Prepared(const ng::DocumentInput& document,int layer,bool shell)
        : units(document.imperial?304.8:1000.), tolerance(centrelineToleranceMm/units) {
        auto input=sectionGeometry(document,layer);
        if(shell){
            // The thickness envelope still uses the finest supported level.
            auto adjacent=ng::buildSubdivisionFaces(input,4);
            const auto reflected=reflectedLayers(adjacent.points,adjacent.faces,input.layers,tolerance);
            triangles=shellMesh(adjacent,reflected,input,units,tolerance);
            shellPoints=std::move(adjacent.points);
        }else{
            // Exterior splines use the project's precision and edge topology.
            surface=std::make_unique<ng::SubdivisionSurface>(std::move(input));
            mesh=&surface->level(surface->input().desiredLevel);
            mirrors=reflectedLayers(mesh->points,mesh->faces,mesh->layers,tolerance);
            index=std::make_unique<ng::SectionIndex>(surface->input(),*mesh);
        }
    }
    const double units,tolerance;
    std::unique_ptr<ng::SubdivisionSurface> surface;
    const ng::SubdivisionLevel* mesh=nullptr;
    std::vector<bool> mirrors;
    std::unique_ptr<ng::SectionIndex> index;
    std::vector<ng::SubdivisionPoint> shellPoints;
    std::vector<IndexedTriangle> triangles;
};

StationGeometry::StationGeometry(const ng::DocumentInput& document,int layer,bool shell)
    : prepared_(std::make_unique<Prepared>(document,layer,shell)) {}
StationGeometry::~StationGeometry()=default;

QList<Station> sectionTemplatesAt(const ng::DocumentInput& document,const std::vector<ng::SectionInput>& sections,
                                const Options& options,const std::atomic_bool& cancelled) {
    validateOptions(options);
    if(cancelled.load())return {};
    return StationGeometry(document,options.layer,options.thickness>0).cut(sections,options,cancelled);
}

QList<Station> StationGeometry::cut(const std::vector<ng::SectionInput>& sections,
                                   const Options& options,const std::atomic_bool& cancelled) const {
    validateOptions(options);
    const auto& prepared=*prepared_;
    const double units=prepared.units,tolerance=prepared.tolerance;
    std::vector<const ng::SectionInput*> selected;
    for(const auto& section:sections) {
        if(cancelled.load())return {};
        if(section.kind!=1 || !options.indices.contains(section.order))continue;
        const auto p=section.plane.coefficients();
        if(p[0]!=1 || p[1]!=0 || p[2]!=0)throw std::runtime_error("横剖站的平面定义不正确。");
        selected.push_back(&section);
    }
    if(selected.size()!=size_t(options.indices.size()))
        throw std::runtime_error("所选横剖站已更改，请重新选择。");

    std::vector<Station> stations(selected.size());
    const auto cutRange=[&](size_t first,size_t end){
        for(size_t i=first;i<end&&!cancelled.load();++i){
            const auto& section=*selected[i];
            auto& station=stations[i];
            station.number=section.order+1;
            station.x=-double(section.plane.coefficients()[3])*units;
            try {
                if(prepared.index)outerLines(*prepared.index,*prepared.mesh,prepared.mirrors,section.plane,units,tolerance,station);
                else innerLines(prepared.shellPoints,prepared.triangles,units,tolerance,station,options,cancelled);
            }catch(const std::runtime_error& e){station.error=QString::fromUtf8(e.what());}
        }
    };
    // Stations read the same immutable mesh. Keep the output order and limit
    // each thickness calculation to at most one additional worker.
    if(!prepared.index&&selected.size()>1&&std::thread::hardware_concurrency()>2){
        const auto middle=selected.size()/2;
        auto second=std::async(cutRange,middle,selected.size());
        cutRange(0,middle);
        second.get();
    }else{
        cutRange(0,selected.size());
    }
    if(cancelled.load())return {};
    QList<Station> results;
    results.reserve(qsizetype(stations.size()));
    for(auto& station:stations)results.append(std::move(station));
    return results;
}
}

