// SPDX-License-Identifier: GPL-2.0-or-later
#include "obj_fit.h"
#include "native/subdivision.h"

#include <QHash>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace fs::exchange {
namespace {
namespace subdivision = native_geometry;
using Index = size_t;
using Vector = std::vector<Vec3>;
constexpr Index missing = std::numeric_limits<Index>::max();
constexpr double infinity = std::numeric_limits<double>::infinity();

[[noreturn]] void fail(const QString& message) {
    throw std::runtime_error(("模型交接："+message).toStdString());
}

void checkCancelled(const std::atomic_bool* cancel) {
    if(cancel && cancel->load(std::memory_order_relaxed))fail("已取消船型重建。");
}

Vec3 operator+(Vec3 a,Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,double s) {return {a.x*s,a.y*s,a.z*s};}
Vec3& operator+=(Vec3& a,Vec3 b) {a=a+b;return a;}
double dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
double squared(Vec3 a) {return dot(a,a);}
Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double component(Vec3 a,int axis) {return axis==0?a.x:axis==1?a.y:a.z;}
Vec3 widened(subdivision::Vec3 p) {return {p.x,p.y,p.z};}
subdivision::Vec3 narrowed(Vec3 p) {return {float(p.x),float(p.y),float(p.z)};}

struct Bounds {
    Vec3 low{infinity,infinity,infinity}, high{-infinity,-infinity,-infinity};
    void add(Vec3 p) {
        low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
        high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
    }
    double distance(Vec3 p) const {
        Vec3 delta{std::max({0.,low.x-p.x,p.x-high.x}),std::max({0.,low.y-p.y,p.y-high.y}),
                   std::max({0.,low.z-p.z,p.z-high.z})};
        return squared(delta);
    }
};

struct Coordinates {
    Vec3 origin;
    double span;
    Vec3 normalize(Vec3 p) const {return (p-origin)*(1/span);}
    Vec3 restore(Vec3 p) const {return origin+p*span;}
};

struct Primitive {
    std::array<Index,3> p;
    Index part;
};
struct Hit {
    Vec3 point;
    std::array<double,3> weights{};
    double distance=infinity;
    Index primitive=missing;
};

// Closest point on a triangle, including its boundary. Feature segments use
// the same index with the last vertex repeated.
Hit closest(Vec3 p,Vec3 a,Vec3 b,Vec3 c) {
    Hit hit;
    auto offer=[&](Vec3 q,std::array<double,3> weights){
        const double d=squared(p-q);
        if(d<hit.distance)hit={q,weights,d};
    };
    const auto u=b-a,v=c-a,n=cross(u,v);
    const auto ap=p-a;
    const double nn=squared(n);
    if(nn>0) {
        const double t=dot(cross(ap,v),n)/nn;
        const double s=dot(cross(u,ap),n)/nn;
        if(t>=0 && s>=0 && t+s<=1) {
            offer(a+u*t+v*s,{1-t-s,t,s});
            return hit;
        }
    }
    const std::array<Vec3,3> points{a,b,c};
    for(Index i=0;i<points.size();++i){
        const Index j=(i+1)%points.size();
        const auto delta=points[j]-points[i];
        const double length=squared(delta);
        const double t=length>0?std::clamp(dot(p-points[i],delta)/length,0.,1.):0.;
        std::array<double,3> weights{};
        weights[i]=1-t;
        weights[j]=t;
        offer(points[i]+delta*t,weights);
    }
    return hit;
}

class SurfaceIndex {
public:
    SurfaceIndex(const Vector& positions,const std::vector<Primitive>& primitives,Index parts)
        : points_(positions),primitives_(primitives),roots_(parts,missing) {
        std::vector<Index> offsets(parts+1);
        for(const auto& p:primitives)++offsets[p.part+1];
        std::partial_sum(offsets.begin(),offsets.end(),offsets.begin());
        auto next=offsets;
        order_.resize(primitives.size());
        centers_.reserve(primitives.size());
        for(Index i=0;i<primitives.size();++i){
            const auto& p=primitives[i];
            order_[next[p.part]++]=i;
            centers_.push_back((points_[p.p[0]]+points_[p.p[1]]+points_[p.p[2]])*(1./3));
        }
        for(Index part=0;part<parts;++part)
            if(offsets[part]!=offsets[part+1])roots_[part]=build(offsets[part],offsets[part+1]);
    }

    Hit nearest(Vec3 p,Index part) const {
        Hit best;
        if(roots_[part]!=missing)query(roots_[part],p,best);
        return best;
    }

private:
    struct Node {
        Bounds bounds;
        Index begin=0,end=0,left=missing,right=missing;
    };
    Index build(Index begin,Index end) {
        Node node;
        node.begin=begin;
        node.end=end;
        Bounds centers;
        for(Index i=begin;i<end;++i){
            const Index id=order_[i];
            centers.add(centers_[id]);
            for(Index p:primitives_[id].p)node.bounds.add(points_[p]);
        }
        const Index id=nodes_.size();
        nodes_.push_back(node);
        if(end-begin<=8)return id;
        const auto span=centers.high-centers.low;
        int axis=span.y>span.x?1:0;
        if(span.z>component(span,axis))axis=2;
        const Index middle=begin+(end-begin)/2;
        std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+end,[&](Index a,Index b){
            const auto u=component(centers_[a],axis),v=component(centers_[b],axis);
            return u<v || (u==v && a<b);
        });
        const Index left=build(begin,middle),right=build(middle,end);
        nodes_[id].left=left;
        nodes_[id].right=right;
        return id;
    }
    void query(Index index,Vec3 p,Hit& best) const {
        const auto& node=nodes_[index];
        if(node.bounds.distance(p)>best.distance)return;
        if(node.left!=missing){
            const bool leftFirst=nodes_[node.left].bounds.distance(p)<=nodes_[node.right].bounds.distance(p);
            query(leftFirst?node.left:node.right,p,best);
            query(leftFirst?node.right:node.left,p,best);
            return;
        }
        for(Index i=node.begin;i<node.end;++i){
            const Index primitive=order_[i];
            const auto& a=primitives_[primitive].p;
            auto hit=closest(p,points_[a[0]],points_[a[1]],points_[a[2]]);
            if(hit.distance<best.distance){hit.primitive=primitive;best=hit;}
        }
    }

    const Vector& points_;
    const std::vector<Primitive>& primitives_;
    Vector centers_;
    std::vector<Index> order_,roots_;
    std::vector<Node> nodes_;
};

quint64 key(EntityId a,EntityId b) {
    if(a>b)std::swap(a,b);
    return (a<<32)|b;
}

struct Topology {
    struct Edge {
        Index first=missing,second=missing;
        bool forward=false;
    };
    std::vector<Index> facePart;
    std::vector<Primitive> features;
    Index parts=0;
};

Topology topology(const ModelSnapshot& model) {
    std::vector<Index> component(model.faces.size());
    std::iota(component.begin(),component.end(),0);
    auto root=[&](Index i){
        while(component[i]!=i){component[i]=component[component[i]];i=component[i];}
        return i;
    };
    QHash<quint64,Topology::Edge> edgeFaces;
    for(Index i=0;i<Index(model.faces.size());++i) {
        const auto& p=model.faces[i].points;
        for(qsizetype j=0;j<p.size();++j){
            const EntityId a=p[j],b=p[(j+1)%p.size()];
            auto& owner=edgeFaces[key(a,b)];
            if(owner.first==missing){owner.first=i;owner.forward=a<b;}
            else {
                if(owner.second!=missing || owner.forward==(a<b))
                    fail("船壳的共享边方向不一致。请先在 Blender 中统一面朝向并修复非流形边。");
                owner.second=i;
                component[root(i)]=root(owner.first);
            }
        }
    }
    Topology result;
    QHash<std::pair<Index,EntityId>,Index> parts;
    for(Index i=0;i<Index(model.faces.size());++i) {
        const auto id=std::make_pair(root(i),model.faces[i].layer);
        auto found=parts.find(id);
        if(found==parts.end())found=parts.insert(id,result.parts++);
        result.facePart.push_back(found.value());
    }
    for(const auto& e:model.edges) {
        if(!e.crease)continue;
        const auto owner=edgeFaces.value(key(e.a,e.b));
        const Index part=result.facePart[owner.first];
        result.features.push_back({{Index(e.a-1),Index(e.b-1),Index(e.b-1)},part});
        if(owner.second!=missing && result.facePart[owner.second]!=part)
            result.features.push_back({result.features.back().p,result.facePart[owner.second]});
    }
    return result;
}

subdivision::GeometryInput geometry(const ModelSnapshot& model,unsigned level) {
    subdivision::GeometryInput out;
    out.precision=int(level)-1;
    out.desiredLevel=level;
    out.layers.resize(model.layers.size());
    for(Index i=0;i<out.layers.size();++i)out.layers[i].symmetric=model.layers[i].symmetric;
    for(const auto& p:model.points)out.points.push_back({narrowed(p.position),subdivision::VertexType(p.vertexType)});
    for(const auto& e:model.edges)out.edges.push_back({{Index(e.a-1),Index(e.b-1)},e.crease});
    for(const auto& f:model.faces){
        subdivision::ControlFaceInput face;
        face.layer=Index(f.layer-1);
        for(EntityId id:f.points)face.points.push_back(Index(id-1));
        out.faces.push_back(std::move(face));
    }
    return out;
}

// Each input vertex survives as a child vertex of every incident face.
// The child order and original-corner placement are those of refine().
std::vector<Index> descendants(const subdivision::GeometryInput& input,
                               const subdivision::SubdivisionLevel& mesh) {
    std::vector<Index> map(input.points.size(),missing);
    Index firstChild=0;
    for(const auto& face:input.faces) {
        const bool triangle=face.points.size()==3;
        for(Index i=0;i<face.points.size();++i) {
            Index child=firstChild+i;
            const Index corner=triangle?1:i%4;
            for(unsigned level=1;level<mesh.level;++level)child=child*4+corner;
            map[face.points[i]]=mesh.faces.at(child).points.at(corner);
        }
        firstChild+=triangle?4:face.points.size();
    }
    return map;
}

struct Surface {
    Surface(const subdivision::SubdivisionLevel& level,const Topology& topology)
        : pointParts(level.points.size(),missing) {
        for(const auto& face:level.faces) {
            const Index part=topology.facePart[face.rootControlFace];
            for(Index p:face.points)if(pointParts[p]==missing)pointParts[p]=part;
            // Same fan triangulation as the document surface renderer/exporter.
            for(Index j=1;j+1<face.points.size();++j)
                triangles.push_back({{face.points[0],face.points[j],face.points[j+1]},part});
        }
        for(const auto& edge:level.edges) {
            if(!edge.crease || edge.faces.empty())continue;
            const auto part=[&](Index face){return topology.facePart[level.faces[face].rootControlFace];};
            const auto p=edge.points;
            features.push_back({{p[0],p[1],p[1]},part(edge.faces[0])});
            if(edge.faces.size()==2 && part(edge.faces[1])!=features.back().part)
                features.push_back({features.back().p,part(edge.faces[1])});
        }
    }
    std::vector<Index> pointParts;
    std::vector<Primitive> triangles,features;
};

template<class Sample>
void samples(const Vector& points,const std::vector<Primitive>& faces,bool segments,
             const std::atomic_bool* cancel,Sample sample) {
    for(Index i=0;i<faces.size();++i){
        if((i&1023)==0)checkCancelled(cancel);
        const auto& f=faces[i];
        const auto a=points[f.p[0]],b=points[f.p[1]],c=points[f.p[2]];
        sample(a,f.part);
        sample(b,f.part);
        sample((a+b)*.5,f.part);
        if(segments)continue;
        sample(c,f.part);
        sample((a+c)*.5,f.part);
        sample((b+c)*.5,f.part);
        sample((a+b+c)*(1./3),f.part);
    }
}

struct Quality {
    double maximum=0,summed=0;
    qsizetype count=0;
    bool oriented=true;
    void add(double d2) {maximum=std::max(maximum,d2);summed+=d2;++count;}
    double rms() const {return std::sqrt(summed/std::max<qsizetype>(count,1));}
    double score() const {return rms()+.25*std::sqrt(maximum);}
};

struct Reference {
    Reference(Vector positions,std::vector<Primitive> faces,std::vector<Primitive> edges,Index partCount)
        : points(std::move(positions)),triangles(std::move(faces)),features(std::move(edges)),
          faceIndex(points,triangles,partCount),featureIndex(points,features,partCount) {}

    Vector points;
    std::vector<Primitive> triangles,features;
    SurfaceIndex faceIndex,featureIndex;
};

Vec3 normal(const Vector& points,const Primitive& triangle) {
    const auto& p=triangle.p;
    return cross(points[p[1]]-points[p[0]],points[p[2]]-points[p[0]]);
}

Quality measure(const Reference& reference,const Vector& positions,const Surface& surface,Index parts,
                const std::atomic_bool* cancel) {
    const SurfaceIndex fitted(positions,surface.triangles,parts);
    const SurfaceIndex fittedFeatures(positions,surface.features,parts);
    Quality quality;
    auto compare=[&](const Vector& points,const std::vector<Primitive>& mesh,const SurfaceIndex& target,bool segments){
        samples(points,mesh,segments,cancel,[&](Vec3 p,Index part){quality.add(target.nearest(p,part).distance);});
    };
    compare(positions,surface.triangles,reference.faceIndex,false);
    compare(reference.points,reference.triangles,fitted,false);
    compare(positions,surface.features,reference.featureIndex,true);
    compare(reference.points,reference.features,fittedFeatures,true);
    for(const auto& tri:surface.triangles){
        const auto n=normal(positions,tri);
        const auto p=(positions[tri.p[0]]+positions[tri.p[1]]+positions[tri.p[2]])*(1./3);
        const auto hit=reference.faceIndex.nearest(p,tri.part);
        if(hit.primitive==missing || dot(n,normal(reference.points,reference.triangles[hit.primitive]))<=0){
            quality.oriented=false;
            break;
        }
    }
    return quality;
}

struct Match {
    std::array<Index,3> nodes;
    std::array<double,3> weights;
    Vec3 residual;
    double importance;
};

Vec3 weighted(const Vector& values,const std::array<Index,3>& ids,const std::array<double,3>& w) {
    return values[ids[0]]*w[0]+values[ids[1]]*w[1]+values[ids[2]]*w[2];
}

std::vector<Match> correspondences(const Reference& reference,const Topology& topology,const Surface& surface,
                                   const Vector& positions,const Vector& initial,const std::vector<Index>& children,
                                   const std::vector<bool>& onFeature,double tolerance,const std::atomic_bool* cancel) {
    std::vector<Match> out;
    const double toleranceSquared=std::max(tolerance*tolerance,1e-20);
    const auto emphasis=[&](double distance){return 1+std::min(15.,distance/toleranceSquared);};
    auto vertex=[&](Index i,Vec3 target,double importance){
        out.push_back({{i,i,i},{1,0,0},target-positions[i],importance});
    };
    for(Index i=0;i<positions.size();++i){
        if((i&1023)==0)checkCancelled(cancel);
        const Index part=surface.pointParts[i];
        if(part!=missing){
            const auto hit=reference.faceIndex.nearest(positions[i],part);
            vertex(i,hit.point,emphasis(hit.distance));
        }
    }
    for(Index i=0;i<children.size();++i)
        if(children[i]!=missing)vertex(children[i],initial[i],onFeature[i]?5.:.18);

    const SurfaceIndex fitIndex(positions,surface.triangles,topology.parts);
    const SurfaceIndex featureIndex(positions,surface.features,topology.parts);
    auto reverse=[&](const std::vector<Primitive>& source,const std::vector<Primitive>& fitted,
                     const SurfaceIndex& index,bool segments,double importance){
        samples(reference.points,source,segments,cancel,[&](Vec3 target,Index part){
            const auto hit=index.nearest(target,part);
            if(hit.primitive!=missing)
                out.push_back({fitted[hit.primitive].p,hit.weights,target-hit.point,importance*emphasis(hit.distance)});
        });
    };
    reverse(reference.triangles,surface.triangles,fitIndex,false,.3);
    reverse(reference.features,surface.features,featureIndex,true,1.2);
    for(const auto& edge:surface.features){
        for(Index i:{edge.p[0],edge.p[1]}){
            const auto hit=reference.featureIndex.nearest(positions[i],edge.part);
            if(hit.primitive!=missing)vertex(i,hit.point,2*emphasis(hit.distance));
        }
    }
    return out;
}

// The subdivision topology is fixed through a fit. Its arithmetic trace is
// linear in the control positions, so apply A and A^T without storing a dense
// matrix. Candidate positions and their deviation are evaluated by FreeShip's
// actual float subdivision, including its original rounding rules.
class LeastSquares {
public:
    LeastSquares(const subdivision::SubdivisionProgram& trace,const std::vector<Match>& matches,
                 const std::vector<bool>& fixed,const std::vector<bool>& centerline)
        : trace_(trace),matches_(matches),fixed_(fixed),centerline_(centerline),weights_(trace.operations.size()) {
        using Op=subdivision::SubdivisionOperation;
        for(Index i=trace.controls.size()+1;i<trace.operations.size();++i){
            const auto& op=trace.operations[i];
            switch(op.code){
            case Op::Scale:weights_[i]={op.factor,0};break;
            case Op::AddScaled:weights_[i]={1,op.factor};break;
            case Op::Divide:weights_[i]={1./op.factor,0};break;
            case Op::Relax:weights_[i]={1.-op.factor,op.factor};break;
            case Op::Midpoint:weights_[i]={.5,.5};break;
            case Op::Add:weights_[i]={1,1};break;
            default:throw std::logic_error("Unexpected subdivision trace operation");
            }
        }
    }

    Vector solve(const std::atomic_bool* cancel) const {
        Vector rhsTrace(trace_.operations.size());
        for(const auto& m:matches_)scatter(rhsTrace,m,m.residual*m.importance);
        auto residue=adjoint(rhsTrace);
        auto search=residue;
        Vector solution(residue.size());
        double previous=innerProduct(residue,residue);
        const double threshold=std::max(1e-22,previous*1e-8);
        for(int iteration=0;iteration<32 && previous>threshold;++iteration) {
            checkCancelled(cancel);
            const auto normal=apply(search);
            const double projected=innerProduct(search,normal);
            if(projected<=0)break;
            const double amount=previous/projected;
            for(Index i=0;i<residue.size();++i){
                solution[i]+=search[i]*amount;
                residue[i]+=normal[i]*(-amount);
            }
            const double next=innerProduct(residue,residue);
            for(Index i=0;i<search.size();++i)search[i]=residue[i]+search[i]*(next/previous);
            previous=next;
        }
        return solution;
    }

private:
    static double innerProduct(const Vector& a,const Vector& b) {
        double total=0;
        for(Index i=0;i<a.size();++i)total+=dot(a[i],b[i]);
        return total;
    }
    void scatter(Vector& nodes,const Match& match,Vec3 value) const {
        for(Index i=0;i<match.nodes.size();++i)nodes[trace_.outputs[match.nodes[i]]]+=value*match.weights[i];
    }
    Vector adjoint(Vector& nodes) const {
        for(Index i=nodes.size();--i>trace_.controls.size();){
            const auto& op=trace_.operations[i];
            nodes[op.a]+=nodes[i]*weights_[i][0];
            nodes[op.b]+=nodes[i]*weights_[i][1];
        }
        Vector controls(trace_.controls.size());
        for(Index i=0;i<controls.size();++i){
            if(fixed_[i])continue;
            controls[i]=nodes[trace_.controls[i]];
            if(centerline_[i])controls[i].y=0;
        }
        return controls;
    }
    Vector apply(const Vector& input) const {
        Vector nodes(trace_.operations.size());
        for(Index i=0;i<input.size();++i)nodes[trace_.controls[i]]=input[i];
        for(Index i=trace_.controls.size()+1;i<nodes.size();++i){
            const auto& op=trace_.operations[i];
            nodes[i]=nodes[op.a]*weights_[i][0]+nodes[op.b]*weights_[i][1];
        }
        Vector result(nodes.size());
        for(const auto& m:matches_){
            const std::array<Index,3> ids{trace_.outputs[m.nodes[0]],trace_.outputs[m.nodes[1]],trace_.outputs[m.nodes[2]]};
            scatter(result,m,weighted(nodes,ids,m.weights)*m.importance);
        }
        auto controls=adjoint(result);
        // Damping keeps unobservable directions from moving the control net.
        for(Index i=0;i<input.size();++i)controls[i]+=input[i]*.025;
        return controls;
    }

    const subdivision::SubdivisionProgram& trace_;
    const std::vector<Match>& matches_;
    const std::vector<bool>& fixed_;
    const std::vector<bool>& centerline_;
    std::vector<std::array<double,2>> weights_;
};

struct Candidate {
    Vector controls;
    Quality quality;
    unsigned level=1;
};

struct ReducedNet {
    ModelSnapshot model;
    Topology topology;
};

struct Quadric {
    std::array<std::array<double,4>,4> matrix{};

    void addPlane(Vec3 p,Vec3 normal) {
        const double length=std::sqrt(squared(normal));
        if(length==0)return;
        const auto n=normal*(1/length);
        const std::array<double,4> plane{n.x,n.y,n.z,-dot(p,n)};
        for(Index i=0;i<4;++i)for(Index j=0;j<4;++j)matrix[i][j]+=plane[i]*plane[j];
    }
    Quadric& operator+=(const Quadric& other) {
        for(Index i=0;i<4;++i)for(Index j=0;j<4;++j)matrix[i][j]+=other.matrix[i][j];
        return *this;
    }
    double product(const std::array<double,4>& a,const std::array<double,4>& b) const {
        double result=0;
        for(Index i=0;i<4;++i)for(Index j=0;j<4;++j)result+=a[i]*matrix[i][j]*b[j];
        return result;
    }
    std::pair<Vec3,double> alongEdge(Vec3 a,Vec3 b) const {
        const Vec3 d=b-a;
        const std::array<double,4> direction{d.x,d.y,d.z,0},start{a.x,a.y,a.z,1};
        const double curvature=product(direction,direction);
        const double t=curvature>1e-20?std::clamp(-product(direction,start)/curvature,.1,.9):.5;
        const auto p=a+d*t;
        const std::array<double,4> row{p.x,p.y,p.z,1};
        return {p,std::max(0.,product(row,row))+1e-6*squared(d)};
    }
};

void appendTriangle(ReducedNet& net,std::array<EntityId,3> points,EntityId layer,Index part,
                    const QSet<quint64>& creases,QSet<quint64>& added) {
    auto& model=net.model;
    model.faces.append({EntityId(model.faces.size()+1),layer,{points[0],points[1],points[2]}});
    net.topology.facePart.push_back(part);
    for(Index i=0;i<3;++i){
        const auto a=points[i],b=points[(i+1)%3],id=key(a,b);
        if(added.contains(id))continue;
        added.insert(id);
        model.edges.append({EntityId(model.edges.size()+1),a,b,creases.contains(id)});
    }
}

// Collapse interior edges in order of accumulated squared distance to
// incident planes. Preserve boundaries, crease paths, layer seams and parts.
std::optional<ReducedNet> reduce(const ModelSnapshot& source,const Reference& reference,
                                 const Topology& original,const Coordinates& coordinates,
                                 Index faceTarget,const std::atomic_bool* cancel) {
    struct Triangle {Primitive face;EntityId layer;bool alive=true;};
    struct Edge {
        double cost;
        Index a,b,versionA,versionB;
        Vec3 position;
        bool operator<(const Edge& rhs) const {
            return std::tie(cost,a,b)>std::tie(rhs.cost,rhs.a,rhs.b);
        }
    };
    Vector points=reference.points;
    std::vector<Triangle> faces;
    std::vector<std::set<Index>> incident(points.size());
    std::vector<Index> version(points.size());
    std::vector<bool> fixed(points.size());
    std::vector<Quadric> error(points.size());
    for(const auto& feature:reference.features)for(Index p:feature.p)fixed[p]=true;
    std::vector<Index> vertexPart(points.size(),missing);
    for(Index i=0;i<reference.triangles.size();++i){
        const auto& tri=reference.triangles[i];
        const auto n=normal(points,tri);
        for(Index p:tri.p){
            incident[p].insert(i);
            fixed[p]=fixed[p]||(vertexPart[p]!=missing&&vertexPart[p]!=tri.part);
            vertexPart[p]=tri.part;
            error[p].addPlane(points[tri.p[0]],n);
        }
        faces.push_back({tri,0,true});
    }
    std::vector<EntityId> layers(original.parts);
    for(Index i=0;i<original.facePart.size();++i)layers[original.facePart[i]]=source.faces[i].layer;
    for(auto& face:faces)face.layer=layers[face.face.part];

    std::priority_queue<Edge> edges;
    auto offer=[&](Index a,Index b){
        if(a>b)std::swap(a,b);
        if(!fixed[a] && !fixed[b] && vertexPart[a]==vertexPart[b]){
            auto quadric=error[a];
            quadric+=error[b];
            const auto [position,cost]=quadric.alongEdge(points[a],points[b]);
            edges.push({cost,a,b,version[a],version[b],position});
        }
    };
    QSet<quint64> queued;
    for(const auto& f:faces)for(Index i=0;i<3;++i){
        const Index a=f.face.p[i],b=f.face.p[(i+1)%3];
        if(!queued.contains(key(a+1,b+1))){queued.insert(key(a+1,b+1));offer(a,b);}
    }
    Index count=faces.size(),visited=0;
    while(count>faceTarget && !edges.empty()){
        if((visited++&1023)==0)checkCancelled(cancel);
        const auto edge=edges.top();
        edges.pop();
        const Index a=edge.a,b=edge.b;
        if(version[a]!=edge.versionA || version[b]!=edge.versionB)continue;
        const auto& first=incident[a];
        const auto& second=incident[b];
        if(first.empty()||second.empty())continue;
        std::vector<Index> together;
        std::set_intersection(first.begin(),first.end(),second.begin(),second.end(),std::back_inserter(together));
        if(together.size()!=2)continue;
        auto neighbours=[&](const std::set<Index>& around){
            std::set<Index> nodes;
            for(Index i:around)for(Index p:faces[i].face.p)if(p!=a && p!=b)nodes.insert(p);
            return nodes;
        };
        const auto nearA=neighbours(first),nearB=neighbours(second);
        std::vector<Index> shared;
        std::set_intersection(nearA.begin(),nearA.end(),nearB.begin(),nearB.end(),std::back_inserter(shared));
        // The link condition preserves a manifold edge's two opposite vertices.
        if(shared.size()!=2)continue;
        const auto hit=reference.faceIndex.nearest(edge.position,vertexPart[a]);
        const Vec3 replacement=hit.point;
        std::vector<Index> changed;
        std::set_union(first.begin(),first.end(),second.begin(),second.end(),std::back_inserter(changed));
        bool valid=true;
        std::set<std::array<Index,3>> unique;
        for(Index i:changed){
            if(i==together[0]||i==together[1])continue;
            const auto& before=faces[i].face;
            const auto n=normal(points,before);
            auto after=before.p;
            for(auto& p:after)if(p==b)p=a;
            auto position=[&](Index p){return p==a?replacement:points[p];};
            const auto o=position(after[0]);
            const auto m=cross(position(after[1])-o,position(after[2])-o);
            std::sort(after.begin(),after.end());
            if(squared(m)<=squared(n)*1e-8 || dot(n,m)<=.25*std::sqrt(squared(n)*squared(m)) ||
               !unique.insert(after).second){valid=false;break;}
        }
        if(!valid)continue;
        for(Index i:changed){
            auto& face=faces[i];
            for(Index p:face.face.p)incident[p].erase(i);
            if(i==together[0]||i==together[1]){face.alive=false;continue;}
            for(auto& p:face.face.p)if(p==b)p=a;
            for(Index p:face.face.p)incident[p].insert(i);
        }
        count-=together.size();
        ++version[a];++version[b];
        points[a]=replacement;
        error[a]+=error[b];
        std::set<Index> neighboursNow;
        for(Index i:incident[a])for(Index p:faces[i].face.p)if(p!=a)neighboursNow.insert(p);
        for(Index p:neighboursNow)offer(a,p);
    }
    if(count>=Index(source.faces.size())*9/10)return {};
    ReducedNet result;
    result.model.layers=source.layers;
    result.topology.parts=original.parts;
    std::vector<EntityId> ids(points.size());
    for(Index i=0;i<points.size();++i){
        if(incident[i].empty())continue;
        auto p=source.points[i];
        p.id=ids[i]=result.model.points.size()+1;
        p.position=widened(narrowed(coordinates.restore(points[i])));
        result.model.points.append(p);
    }
    QSet<quint64> creases,added;
    for(const auto& old:original.features){
        if(ids[old.p[0]]==0 || ids[old.p[1]]==0)return {};
        const auto a=ids[old.p[0]],b=ids[old.p[1]];
        creases.insert(key(a,b));
        result.topology.features.push_back({{Index(a-1),Index(b-1),Index(b-1)},old.part});
    }
    for(const auto& f:faces)if(f.alive){
        const auto& p=f.face.p;
        appendTriangle(result,{ids[p[0]],ids[p[1]],ids[p[2]]},f.layer,f.face.part,creases,added);
    }
    return result;
}

// A sparse reference can need more degrees of freedom when a tight tolerance
// is requested. Split its triangles once without moving the reference itself.
ReducedNet refineControlMesh(const ModelSnapshot& source,const std::vector<ObjTriangle>& reference,
                             const Topology& topology) {
    ReducedNet result;
    result.model.layers=source.layers;
    result.model.points=source.points;
    result.topology.parts=topology.parts;
    QSet<quint64> originals,creases,added;
    for(const auto& edge:source.edges)if(edge.crease)originals.insert(key(edge.a,edge.b));
    QHash<quint64,EntityId> midpoints;
    auto midpoint=[&](EntityId a,EntityId b){
        const auto id=key(a,b);
        const auto found=midpoints.constFind(id);
        if(found!=midpoints.cend())return found.value();
        const bool crease=originals.contains(id);
        ControlPoint point;
        point.id=result.model.points.size()+1;
        point.position=widened(narrowed((source.points[a-1].position+source.points[b-1].position)*.5));
        point.vertexType=crease?1:0;
        result.model.points.append(point);
        midpoints.insert(id,point.id);
        if(crease){
            creases.insert(key(a,point.id));
            creases.insert(key(point.id,b));
        }
        return point.id;
    };
    for(const auto& tri:reference){
        const EntityId a=tri.points[0]+1,b=tri.points[1]+1,c=tri.points[2]+1;
        const auto ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);
        const auto layer=source.faces[tri.face].layer;
        const auto part=topology.facePart[tri.face];
        for(const auto& points:{std::array<EntityId,3>{a,ab,ca},{ab,b,bc},{ca,bc,c},{ab,bc,ca}})
            appendTriangle(result,points,layer,part,creases,added);
    }
    return result;
}

Candidate fit(const ModelSnapshot& model,const Topology& topology,const Reference& reference,
              const Coordinates& coordinates,unsigned level,double tolerance,int symmetrySide,
              const std::atomic_bool* cancel) {
    checkCancelled(cancel);
    auto input=geometry(model,level);
    auto trace=subdivision::compileSubdivision(input,level);
    checkCancelled(cancel);
    const Surface surface(trace.mesh,topology);
    const auto children=descendants(input,trace.mesh);
    const auto original=input.points;
    Vector initial,positions;
    std::vector<bool> fixed(original.size()),centerline(original.size()),onFeature(original.size());
    for(Index i=0;i<original.size();++i){
        const auto& p=original[i];
        initial.push_back(coordinates.normalize(widened(p.position)));
        fixed[i]=p.type==subdivision::VertexType::Corner;
        centerline[i]=symmetrySide!=0 && p.position.y==0;
    }
    for(const auto& edge:input.edges)if(edge.crease)for(Index p:edge.points)onFeature[p]=true;
    auto evaluate=[&](const Vector& controls,Quality& quality){
        for(Index i=0;i<controls.size();++i){
            const auto p=coordinates.restore(controls[i]);
            if(!std::isfinite(squared(p))||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1e9)return false;
            input.points[i].position=fixed[i]?original[i].position:narrowed(p);
            if(centerline[i])input.points[i].position.y=0;
            if(input.points[i].position.y*symmetrySide<0)return false;
        }
        trace.update(input);
        positions.clear();
        for(const auto& p:trace.mesh.points)positions.push_back(coordinates.normalize(widened(p.position)));
        quality=measure(reference,positions,surface,topology.parts,cancel);
        return std::isfinite(quality.maximum);
    };
    auto controls=initial;
    Quality current;
    if(!evaluate(controls,current))fail("船壳网格的坐标或边界不适合重建。");
    Candidate best{controls,current,level};
    for(int outer=0;outer<12 && !(best.quality.oriented && best.quality.maximum<=tolerance*tolerance);++outer) {
        checkCancelled(cancel);
        const auto matches=correspondences(reference,topology,surface,positions,initial,children,onFeature,tolerance,cancel);
        const auto delta=LeastSquares(trace,matches,fixed,centerline).solve(cancel);
        bool improved=false;
        for(double step:{1.,.5,.25,.125}){
            auto trial=controls;
            for(Index i=0;i<trial.size();++i)trial[i]+=delta[i]*step;
            Quality measured;
            if(!evaluate(trial,measured))continue;
            if((measured.oriented && !best.quality.oriented) ||
               (measured.oriented==best.quality.oriented && measured.maximum<best.quality.maximum))
                best={trial,measured,level};
            if(measured.score()<current.score()*.9999 || measured.maximum<current.maximum*.995 ||
               (measured.oriented && !current.oriented)){
                controls=std::move(trial);
                current=measured;
                improved=true;
                break;
            }
        }
        if(!improved)break;
    }
    // Return the exact control coordinates evaluated in the document's float
    // core, including any centerline and corner constraints.
    if(!evaluate(best.controls,best.quality))fail("拟合结果无法转换为 FreeShip 控制网。");
    for(Index i=0;i<best.controls.size();++i)best.controls[i]=widened(input.points[i].position);
    return best;
}
}

void reconstructObj(Document& document,const std::vector<ObjTriangle>& input,const Options& options,
                    const std::atomic_bool* cancel) {
    checkCancelled(cancel);
    auto& model=document.model;
    if(!std::isfinite(options.objToleranceMm)||options.objToleranceMm<0||options.objToleranceMm>1e9)
        fail("拟合容差须为 0 至 10 亿毫米。");

    const auto mesh=topology(model);
    std::vector<Primitive> triangles;
    triangles.reserve(input.size());
    Bounds bounds;
    for(const auto& tri:input){
        triangles.push_back({tri.points,mesh.facePart[tri.face]});
        for(Index p:tri.points)bounds.add(model.points[p].position);
    }
    const double span=std::sqrt(squared(bounds.high-bounds.low));
    if(!std::isfinite(span)||span<=0)fail("OBJ 船壳尺寸无效。");
    const Coordinates coordinates{(bounds.low+bounds.high)*.5,span};
    const double scaleMm=(model.units=="imperial"?304.8:1000.)*span;
    const double toleranceMm=options.objToleranceMm>0?options.objToleranceMm:.001*scaleMm;
    const double tolerance=toleranceMm/scaleMm;
    int symmetrySide=0;
    if(options.symmetric){
        if(bounds.low.y<0 && bounds.high.y>0)fail("半船重建要求整个网格位于中心线同一侧。");
        symmetrySide=bounds.low.y<0?-1:1;
    }
    Vector points;
    for(const auto& p:model.points)points.push_back(coordinates.normalize(p.position));
    const Reference reference(std::move(points),std::move(triangles),mesh.features,mesh.parts);

    auto fitNet=[&](const ModelSnapshot& net,const Topology& structure){
        Index corners=0;
        for(const auto& f:net.faces)corners+=Index(f.points.size());
        return fit(net,structure,reference,coordinates,corners<=12000?2:1,tolerance,symmetrySide,cancel);
    };
    auto accepted=[&](const Candidate& c){return c.quality.oriented && c.quality.maximum<=tolerance*tolerance;};
    Candidate candidate;
    std::optional<ReducedNet> reduced;
    try{
        if(model.faces.size()>1400){
            const Index count=reference.triangles.size();
            const Index initial=std::clamp(count/4,Index(1000),Index(2400));
            auto attempt=[&](Index target){
                auto net=reduce(model,reference,mesh,coordinates,target,cancel);
                if(!net)return;
                auto result=fitNet(net->model,net->topology);
                if(!reduced || accepted(result) ||
                   (result.quality.oriented && (!candidate.quality.oriented || result.quality.maximum<candidate.quality.maximum))){
                    candidate=std::move(result);
                    reduced=std::move(net);
                }
            };
            attempt(initial);
            const Index larger=std::min(count/3,Index(6800));
            if((!reduced||!accepted(candidate)) && larger>initial)attempt(larger);
        }
        if(!reduced || !accepted(candidate)){
            try{
                candidate=fitNet(model,mesh);
                reduced.reset();
            }catch(const std::length_error&){
                if(!reduced)throw;
                // Keep the measured best-effort deviation for the diagnostic.
            }
        }
        if(!accepted(candidate) && input.size()<=1500 && candidate.quality.oriented){
            auto refined=refineControlMesh(model,input,mesh);
            auto more=fitNet(refined.model,refined.topology);
            if(more.quality.oriented && more.quality.maximum<candidate.quality.maximum){
                candidate=std::move(more);
                reduced=std::move(refined);
            }
        }
    }catch(const std::length_error&){
        fail("OBJ 控制网过密。请先减少网格面数后重建。");
    }
    const double maximum=std::sqrt(candidate.quality.maximum)*scaleMm;
    if(!candidate.quality.oriented)fail("重建后出现翻面或退化面。请检查源网格和折痕。");
    if(maximum>toleranceMm){
        fail(QString("重建未达到容差。最大采样偏差 %1 mm，容差 %2 mm。")
             .arg(maximum,0,'g',7).arg(toleranceMm,0,'g',7));
    }
    if(reduced){
        model.points=std::move(reduced->model.points);
        model.edges=std::move(reduced->model.edges);
        model.faces=std::move(reduced->model.faces);
    }
    for(Index i=0;i<candidate.controls.size();++i)model.points[i].position=candidate.controls[i];
    model.precision=int(candidate.level)-1;
    model.length=bounds.high.x-bounds.low.x;
    model.beam=options.symmetric?2*std::max(std::abs(bounds.low.y),std::abs(bounds.high.y)):bounds.high.y-bounds.low.y;
    document.objFit=ObjFitReport{toleranceMm,maximum,candidate.quality.rms()*scaleMm,candidate.quality.count};
}
}
