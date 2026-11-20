// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_hull.h"
#include <manifold/manifold.h>
#include <manifold/polygon.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

namespace fs::printing {
namespace {
namespace la=manifold::la;
using manifold::vec3;
using manifold::Manifold;
using manifold::MeshGL64;
using manifold::ExecutionContext;
using Face=std::array<int,3>;
using Edge=std::pair<int,int>;
using PointKey=std::array<double,3>;
struct FaceUse {int index,from,to;};
using Edges=std::map<Edge,std::vector<FaceUse>>;
struct Skin {
    std::vector<vec3> vertices;
    std::map<PointKey,int> indices;
    std::vector<Face> faces;
    std::vector<EntityId> layers;
    int vertex(vec3 p) {
        auto [it,added]=indices.emplace(PointKey{p.x,p.y,p.z},int(vertices.size()));
        if(added) vertices.push_back(p);
        return it->second;
    }
    void add(Face f,EntityId layer) {faces.push_back(f);layers.push_back(layer);}
};
void check(const ExecutionContext& job) {if(job.Cancelled()) throw Problem{"已取消。",{}};}
QString where(const Skin& skin,int face,const ModelSnapshot& model) {
    const auto p=skin.vertices[skin.faces[face][0]];
    const auto* layer=model.layer(skin.layers[face]);
    return QString("%1  ·  X %2 mm, Y %3 mm, Z %4 mm").arg(layer?layer->name:QString())
        .arg(p.x,0,'f',2).arg(p.y,0,'f',2).arg(p.z,0,'f',2);
}
Skin readSkin(const ModelSnapshot& m,const Options& o,ExecutionContext& job) {
    std::map<EntityId,int> sides;
    double low=std::numeric_limits<double>::infinity(),high=-low,bottom=low;
    for(const auto& t:m.triangles) if(o.layers.contains(t.layer)) for(auto p:t.vertices) {
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw Problem{"船壳含无效坐标。",{}};
        low=std::min(low,p.x);high=std::max(high,p.x);bottom=std::min(bottom,p.z);
        sides[t.layer]|=p.y>0?1:(p.y<0?2:0);
    }
    if(!(high>low)) throw Problem{"请选择含船壳曲面的图层。",{}};
    const double scale=o.lengthMm/(high-low);
    QSet<EntityId> mirror;
    for(const auto& l:m.layers) if(l.symmetric && (sides[l.id]==1 || sides[l.id]==2)) mirror.insert(l.id);
    Skin skin;
    for(const auto& t:m.triangles) if(o.layers.contains(t.layer)) {
        check(job);
        const bool reflect=mirror.contains(t.layer);
        if(reflect && std::all_of(t.vertices.begin(),t.vertices.end(),[](Vec3 p){return p.y==0;})) continue;
        for(int side=0;side<=(reflect?1:0);++side) {
            Face f;
            for(int j=0;j<3;++j) {
                auto p=t.vertices[side?2-j:j];
                f[j]=skin.vertex({(p.x-low)*scale,p.y*(side?-scale:scale),(p.z-bottom)*scale});
            }
            const auto a=skin.vertices[f[0]], b=skin.vertices[f[1]], c=skin.vertices[f[2]];
            if(la::length2(la::cross(b-a,c-a))>0) skin.add(f,t.layer);
        }
    }
    if(skin.faces.empty()) throw Problem{"请选择含船壳曲面的图层。",{}};
    return skin;
}
Edges edgesOf(const Skin& skin,const ModelSnapshot& m) {
    Edges edges;
    for(int i=0;i<int(skin.faces.size());++i) for(int j=0;j<3;++j) {
        const auto f=skin.faces[i];int a=f[j],b=f[(j+1)%3];
        auto& uses=edges[std::minmax(a,b)];uses.push_back({i,a,b});
        if(uses.size()>2) throw Problem{"船壳面重叠或有分叉，请检查所选图层。",where(skin,i,m)};
    }
    return edges;
}
// Only pair opposing, geometrically mirrored boundary edges on a sloping stem.
// Bridge the original points with new faces. No original skin point moves.
bool belowSheer(const Skin& skin,vec3 midpoint) {
    for(auto face:skin.faces) for(int i=0;i<3;++i) {
        auto a=skin.vertices[face[i]],b=skin.vertices[face[(i+1)%3]];
        if((a.x<=midpoint.x)==(b.x<=midpoint.x)) continue;
        auto p=a+(b-a)*((midpoint.x-a.x)/(b.x-a.x));
        if(p.y>midpoint.y+.0001 && p.z>midpoint.z+.0001) return true;
    }
    return false;
}
double stemSeams(Skin& skin,const Edges& edges,const Options& o) {
    double widest=0,oversize=0;
    for(const auto& [key,use]:edges) {
        if(use.size()!=1) continue;
        auto [face,a,b]=use.front();auto p=skin.vertices[a],q=skin.vertices[b];
        const double width=2*std::max(p.y,q.y);
        if(std::min(p.y,q.y)<0 || width<=0 || std::abs(p.z-q.z)<1e-7 ||
           (std::min(p.x,q.x)<o.lengthMm*.8 && std::max(p.x,q.x)>o.lengthMm*.2)) continue;
        auto mirrorA=skin.indices.find({p.x,-p.y,p.z}),mirrorB=skin.indices.find({q.x,-q.y,q.z});
        if(mirrorA==skin.indices.end()||mirrorB==skin.indices.end()) continue;
        int d=mirrorA->second,c=mirrorB->second;
        auto other=edges.find(std::minmax(c,d));
        if(other==edges.end()||other->second.size()!=1 || other->first==key) continue;
        // A broad bow/transom is a modelling decision, never an automatic stitch.
        if(width>o.lengthMm*.02 || !belowSheer(skin,(p+q)*.5)) continue;
        if(width>std::min(.1,o.thicknessMm*.1)){oversize=std::max(oversize,width);continue;}
        widest=std::max(widest,width);
        if(!o.closeStemSeam) continue;
        if(a!=d) skin.add({b,a,d},skin.layers[face]);
        if(b!=c) skin.add({b,d,c},skin.layers[face]);
    }
    if(oversize>0) throw Problem{"艏艉开口过宽，请补齐船壳。",QString("%1 mm").arg(oversize,0,'f',3)};
    if(widest>0&&!o.closeStemSeam)
        throw Problem{"艏艉中心线有微缝，可接合后导出。",QString("%1 mm").arg(widest,0,'f',3),true};
    return widest;
}
std::vector<int> orientAndFindRim(Skin& skin,const ModelSnapshot& m,ExecutionContext& job) {
    auto edges=edgesOf(skin,m);
    std::vector<int> flip(skin.faces.size(),-1), stack{0};flip[0]=0;
    while(!stack.empty()) {
        int i=stack.back();stack.pop_back();check(job);
        auto f=skin.faces[i];
        for(int j=0;j<3;++j) {
            int a=f[j],b=f[(j+1)%3];const auto& pair=edges.at(std::minmax(a,b));
            if(pair.size()==1) continue;
            auto next=pair[pair[0].index==i?1:0];
            int value=flip[i]^(next.from==a);
            if(flip[next.index]<0) {flip[next.index]=value;stack.push_back(next.index);}
            else if(flip[next.index]!=value) throw Problem{"船壳面方向无法统一。",where(skin,i,m)};
        }
    }
    if(auto lost=std::find(flip.begin(),flip.end(),-1);lost!=flip.end())
        throw Problem{"所选图层包含分离的曲面，请只选一艘船的连续外壳。",where(skin,int(lost-flip.begin()),m)};
    std::map<int,int> next;
    for(const auto& [key,pair]:edges) if(pair.size()==1) {
        auto [i,a,b]=pair[0];
        if(!flip[i]) std::swap(a,b); // oriented along the temporary top
        if(!next.emplace(a,b).second) throw Problem{"开口边界有分叉，请检查船壳。",where(skin,i,m)};
    }
    if(next.empty()) throw Problem{"所选曲面已经封闭，请取消选择甲板或使用普通 STL 导出。",{}};
    std::vector<std::vector<int>> loops;
    while(!next.empty()) {
        std::vector<int> loop;int start=next.begin()->first,i=start;
        do {
            auto it=next.find(i);
            if(it==next.end()) throw Problem{"开口边界不连续，请检查船壳。",{}};
            loop.push_back(i);i=it->second;next.erase(it);
        }while(i!=start);
        loops.push_back(std::move(loop));
    }
    if(loops.size()!=1) {
        const auto& small=*std::min_element(loops.begin(),loops.end(),[](auto& a,auto& b){return a.size()<b.size();});
        auto p=skin.vertices[small.front()];
        throw Problem{"除了舷缘还有其他开口，请补齐船壳或检查图层。",QString("X %1 mm, Y %2 mm, Z %3 mm").arg(p.x,0,'f',2).arg(p.y,0,'f',2).arg(p.z,0,'f',2)};
    }
    auto rim=std::move(loops.front());double area=0;
    for(size_t i=0;i<rim.size();++i) {
        auto p=skin.vertices[rim[i]],q=skin.vertices[rim[(i+1)%rim.size()]];
        if(std::hypot(q.x-p.x,q.y-p.y)<1.e-8) throw Problem{"开口延伸到船侧或艉板，请检查缺少的外壳面。",QString("X %1 mm").arg(p.x,0,'f',2)};
        area+=p.x*q.y-p.y*q.x;
    }
    if(std::abs(area)<1e-9) throw Problem{"找不到朝上的船舱开口。",{}};
    if(area<0) std::reverse(rim.begin(),rim.end());
    for(size_t i=0;i<flip.size();++i) if(bool(flip[i])!=(area<0)) std::swap(skin.faces[i][1],skin.faces[i][2]);
    return rim;
}
void checkOutline(const manifold::SimplePolygon& poly,const ExecutionContext& job) {
    auto turn=[](auto a,auto b,auto c){return la::determinant(manifold::la::mat<double,2,2>{b-a,c-a});};
    for(size_t i=0;i<poly.size();++i) {
        check(job);auto a=poly[i],b=poly[(i+1)%poly.size()],c=poly[(i+2)%poly.size()];
        if(la::length2(b-a)<1.e-16 || (std::abs(turn(a,b,c))<1.e-10 && la::dot(b-a,c-b)<0))
            throw Problem{"开口边界折返，请检查缺少的船侧或艉板。",{}};
        for(size_t j=i+2;j<poly.size();++j) {
            if(i==0&&j+1==poly.size()) continue;
            auto p=poly[j],q=poly[(j+1)%poly.size()];
            if(la::any(la::less(la::max(a,b),la::min(p,q)))||la::any(la::less(la::max(p,q),la::min(a,b)))) continue;
            if(turn(a,b,p)*turn(a,b,q)<=0 && turn(p,q,a)*turn(p,q,b)<=0) throw Problem{"开口边界交叉，请检查船壳图层。",{}};
        }
    }
}
// A subdivided planar transom can contain folded triangles, even though its
// perimeter is simple. Retessellate that existing planar disk only. A missing
// end, an extra boundary loop, or a nonplanar surface is never capped here.
int retessellateEnd(Skin& skin,double x,int sign,const ModelSnapshot& model,ExecutionContext& job) {
    std::vector<bool> end(skin.faces.size());Edges edges;int first=-1;bool folded=false;
    for(int i=0;i<int(skin.faces.size());++i) {
        auto f=skin.faces[i];const auto a=skin.vertices[f[0]],b=skin.vertices[f[1]],c=skin.vertices[f[2]];
        if(a.x!=x||b.x!=x||c.x!=x) continue;
        end[i]=true;if(first<0) first=i;
        folded|=sign*la::cross(b-a,c-a).x<=0;
        for(int j=0;j<3;++j) {int u=f[j],v=f[(j+1)%3];edges[std::minmax(u,v)].push_back({i,u,v});}
    }
    if(!folded) return 0;
    std::map<int,int> next;
    for(const auto& [key,use]:edges) if(use.size()==1 && !next.emplace(use[0].from,use[0].to).second)
        throw Problem{"端板边界有分叉。",where(skin,first,model)};
    if(next.empty()) throw Problem{"端板边界不完整。",where(skin,first,model)};
    std::vector<int> rim;const int start=next.begin()->first;int i=start;
    do {
        auto it=next.find(i);if(it==next.end()) throw Problem{"端板边界不连续。",where(skin,first,model)};
        rim.push_back(i);i=it->second;next.erase(it);
    }while(i!=start);
    if(!next.empty()) throw Problem{"端板含多个边界，请检查孔洞或分离的曲面。",where(skin,first,model)};
    manifold::SimplePolygon poly;
    for(int v:rim){auto p=skin.vertices[v];poly.push_back({p.y,sign*p.z});}
    checkOutline(poly,job);
    auto fill=manifold::Triangulate({poly},-1,false);
    if(fill.empty()) throw Problem{"端板轮廓无法三角化。",where(skin,first,model)};
    const auto layer=skin.layers[first];std::vector<Face> faces;std::vector<EntityId> layers;
    for(size_t j=0;j<end.size();++j) if(!end[j]) {faces.push_back(skin.faces[j]);layers.push_back(skin.layers[j]);}
    skin.faces=std::move(faces);skin.layers=std::move(layers);
    for(auto f:fill) skin.add({rim[f.x],rim[f.y],rim[f.z]},layer);
    return 1;
}
bool intersects(const std::array<vec3,3>& a,const std::array<vec3,3>& b) {
    auto apart=[&](vec3 axis) {
        const auto interval=[&](const auto& tri){
            double x=la::dot(axis,tri[0]-a[0]),y=la::dot(axis,tri[1]-a[0]),z=la::dot(axis,tri[2]-a[0]);
            return std::pair{std::min({x,y,z}),std::max({x,y,z})};
        };
        auto p=interval(a),q=interval(b);return p.second<q.first || q.second<p.first;
    };
    std::array<vec3,3> u{a[1]-a[0],a[2]-a[1],a[0]-a[2]},v{b[1]-b[0],b[2]-b[1],b[0]-b[2]};
    auto n=la::cross(u[0],u[1]),m=la::cross(v[0],v[1]);
    if(apart(n)||apart(m)) return false;
    for(auto e:u) {
        if(apart(la::cross(n,e))) return false; // in-plane axes for coplanar faces
        for(auto f:v) if(apart(la::cross(e,f))) return false;
    }
    for(auto f:v) if(apart(la::cross(m,f))) return false;
    return true;
}
struct Extent {vec3 low,high;int index;};
void checkIntersections(const Skin& skin,const ModelSnapshot& m,ExecutionContext& job) {
    std::vector<Extent> bounds;
    for(int i=0;i<int(skin.faces.size());++i) {
        auto f=skin.faces[i];auto p=skin.vertices[f[0]],q=skin.vertices[f[1]],r=skin.vertices[f[2]];
        bounds.push_back({la::min(p,la::min(q,r)),la::max(p,la::max(q,r)),i});
    }
    std::sort(bounds.begin(),bounds.end(),[](const Extent& a,const Extent& b){return a.low.x<b.low.x;});
    for(size_t i=0;i<bounds.size();++i) {
        check(job);const auto& a=bounds[i];auto f=skin.faces[a.index];
        for(size_t j=i+1;j<bounds.size() && bounds[j].low.x<=a.high.x;++j) {
            const auto& b=bounds[j];
            if(a.high.y<b.low.y||b.high.y<a.low.y||a.high.z<b.low.z||b.high.z<a.low.z) continue;
            auto g=skin.faces[b.index];
            if(std::any_of(f.begin(),f.end(),[&](int v){return std::find(g.begin(),g.end(),v)!=g.end();})) continue;
            auto poly=[&](Face t){return std::array<vec3,3>{skin.vertices[t[0]],skin.vertices[t[1]],skin.vertices[t[2]]};};
            if(intersects(poly(f),poly(g)))
                throw Problem{"船壳曲面相交，请检查这些位置。",where(skin,a.index,m)+" / "+where(skin,b.index,m)};
        }
    }
}
Manifold closed(const Skin& skin,const std::vector<int>& rim,const std::vector<manifold::ivec3>& cap,
                double top,ExecutionContext& job) {
    MeshGL64 mesh;
    auto vertex=[&](vec3 p){mesh.vertProperties.insert(mesh.vertProperties.end(),{p.x,p.y,p.z});};
    auto face=[&](int a,int b,int c){for(int v:{a,b,c}) mesh.triVerts.push_back(v);};
    for(vec3 p:skin.vertices) vertex(p);
    for(auto f:skin.faces) face(f[0],f[1],f[2]);
    auto lid=rim;
    if(std::isfinite(top)) {
        for(size_t i=0;i<rim.size();++i) {
            vec3 p=skin.vertices[rim[i]];p.z=top;lid[i]=int(mesh.NumVert());vertex(p);
        }
        for(size_t i=0;i<rim.size();++i) {
            size_t j=(i+1)%rim.size();
            face(rim[i],rim[j],lid[j]);face(rim[i],lid[j],lid[i]);
        }
    }
    for(auto f:cap) face(lid[f.x],lid[f.y],lid[f.z]);
    auto solid=job.FromMeshGL(mesh);check(job);
    if(solid.Status()!=Manifold::Error::NoError||solid.Volume()<=0)
        throw Problem{"舷缘无法围成有效的上开口，请检查甲板、艉板和船壳。",{}};
    return solid;
}
// A circumscribed polyhedron bounds the Euclidean ball from outside. Its
// polar vertices are intersections of the tangent planes of the unit sphere.
// Unlike a section-wise offset this also accounts for bow and transom rake.
Manifold thicknessKernel(const Skin& skin,double radius) {
    const auto base=Manifold::Sphere(1,24).GetMeshGL64();
    std::vector<vec3> directions;
    for(size_t i=0;i<base.NumVert();++i) directions.push_back(base.GetVertPos(i));
    // For a faceted boat add its panel normals, including raked sides. A
    // smooth subdivided hull uses the uniformly sampled, circumscribed ball.
    std::map<std::array<int,3>,std::pair<double,vec3>> patches;
    for(const auto& f:skin.faces) {
        const auto a=skin.vertices[f[0]],b=skin.vertices[f[1]],c=skin.vertices[f[2]];
        auto normal=la::cross(b-a,c-a);const double area=la::length(normal);if(area==0) continue;
        normal/=area;
        auto& patch=patches[{int(std::round(normal.x*1e7)),int(std::round(normal.y*1e7)),int(std::round(normal.z*1e7))}];
        patch.first+=area;patch.second=normal;
    }
    auto sphere=base;
    if(patches.size()<=16) {
        for(const auto& item:patches){
            const auto n=item.second.second;
            if(std::none_of(directions.begin(),directions.end(),[&](vec3 v){return std::abs(la::dot(v,n))>1.-1e-9;})){
                directions.push_back(n);directions.push_back(-n);
            }
        }
        sphere=Manifold::Hull(directions).GetMeshGL64();
    }
    std::vector<vec3> dual;
    for(size_t i=0;i<sphere.NumTri();++i) {
        const auto f=sphere.GetTriVerts(i);
        const auto a=sphere.GetVertPos(f.x),b=sphere.GetVertPos(f.y),c=sphere.GetVertPos(f.z);
        const auto n=la::cross(b-a,c-a);dual.push_back(n/la::dot(n,a));
    }
    return Manifold::Hull(dual).Scale(vec3(radius));
}
}

Result build(const ModelSnapshot& source,const Options& o,ExecutionContext& job) {
    if(!std::isfinite(o.lengthMm)||!std::isfinite(o.thicknessMm)||o.lengthMm<=0||o.thicknessMm<=0)
        throw Problem{"成品船长和壁厚必须大于零。",{}};
    auto skin=readSkin(source,o,job);
    Result result;
    result.seamMm=stemSeams(skin,edgesOf(skin,source),o);
    auto rim=orientAndFindRim(skin,source,job);
    result.retessellatedEnds=retessellateEnd(skin,0,-1,source,job);
    result.retessellatedEnds+=retessellateEnd(skin,o.lengthMm,1,source,job);
    for(int i=0;i<int(skin.faces.size());++i) {
        const auto f=skin.faces[i];auto a=skin.vertices[f[0]],b=skin.vertices[f[1]],c=skin.vertices[f[2]];
        auto n=la::cross(b-a,c-a);
        if(n.z>0.8*la::length(n)) throw Problem{"所选曲面含朝上的盖面，请取消选择甲板或检查船壳图层。",where(skin,i,source)};
    }
    checkIntersections(skin,source,job);
    manifold::SimplePolygon polygon;
    for(int i:rim) polygon.push_back({skin.vertices[i].x,skin.vertices[i].y});
    checkOutline(polygon,job);
    const auto cap=manifold::Triangulate({polygon},-1,false);
    if(cap.empty()) throw Problem{"舷缘投影无法形成上开口，请检查缺失或交叉的船壳面。",{}};
    auto hull=closed(skin,rim,cap,std::numeric_limits<double>::quiet_NaN(),job);
    auto box=hull.BoundingBox();
    result.lengthMm=box.max.x-box.min.x;result.beamMm=box.max.y-box.min.y;result.heightMm=box.max.z-box.min.z;
    if(o.thicknessMm*2>=result.beamMm || o.thicknessMm>=result.heightMm)
        throw Problem{"壁厚超过可用舱宽或舱深，请减小壁厚。",{}};
    // Extending the rim before erosion keeps the printed top open. The original
    // non-planar rim defines the final cut, not a flat horizontal waterline.
    auto extended=closed(skin,rim,cap,box.max.z+3*o.thicknessMm,job);
    auto cavity=extended.WithContext(job).MinkowskiDifference(thicknessKernel(skin,o.thicknessMm));
    check(job);
    auto shell=(hull-cavity).WithContext(job).AsOriginal().Simplify(0.00005);
    check(job);
    if(cavity.Status()!=Manifold::Error::NoError||shell.Status()!=Manifold::Error::NoError||
       shell.IsEmpty()||shell.Volume()<=0||shell.Volume()>=hull.Volume()*(1.-1e-9)||
       shell.Genus()!=0||shell.Decompose().size()!=1)
        throw Problem{"扣除壁厚后无法得到连续的敞口船壳，请检查壁厚和所选图层。",{}};
    auto mesh=shell.GetMeshGL64();
    result.mesh.units="metric";Layer layer;layer.id=1;layer.symmetric=false;result.mesh.layers.push_back(layer);
    result.mesh.triangles.reserve(mesh.NumTri());
    // STL's stored coordinates are floats. Verify the actual welded float
    // mesh, not just the double-precision Boolean, before permitting a save.
    Skin rounded;
    for(size_t i=0;i<mesh.NumTri();++i) {
        check(job);auto tri=mesh.GetTriVerts(i);HullTriangle t;t.layer=1;Face index;
        for(int j=0;j<3;++j) {
            auto p=mesh.GetVertPos(tri[j]);vec3 mm{float(p.x),float(p.y),float(p.z)};
            index[j]=rounded.vertex(mm);t.vertices[j]={mm.x*.001,mm.y*.001,mm.z*.001};
        }
        if(index[0]==index[1]||index[1]==index[2]||index[2]==index[0])continue;
        rounded.add(index,1);result.mesh.triangles.append(t);
    }
    MeshGL64 verify;
    for(auto p:rounded.vertices) verify.vertProperties.insert(verify.vertProperties.end(),{p.x,p.y,p.z});
    for(auto f:rounded.faces) for(int id:f) verify.triVerts.push_back(id);
    auto serialized=job.FromMeshGL(verify);check(job);
    if(serialized.Status()!=Manifold::Error::NoError||serialized.IsEmpty())
        throw Problem{"当前尺寸超出 STL 网格精度，请调整成品船长或壁厚。",{}};
    return result;
}
}
