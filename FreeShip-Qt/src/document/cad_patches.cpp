// GPL-2.0-or-later. Adapted from FREE!ship / Plus AssembleFacesToPatches,
// File_ExportIGES.ProcessGrid and FreeNURBSurface (Martijn van Engeland, 2005;
// Timoshenko V.F.; Mark Malakanov). See docs/migration/NATIVE-CAD.md.
#include "cad_patches.h"
#include "geometry_input.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>

namespace fs::document {
namespace {
using namespace native_geometry;
using Id=std::size_t;
using Key=std::pair<Id,Id>;
using Grid=std::vector<std::vector<Id>>;
constexpr Id Missing=std::numeric_limits<Id>::max();
[[noreturn]] void bad(const char *s) { throw std::runtime_error(s); }
Key key(Id a,Id b) { return std::minmax(a,b); }
struct Job {
    QString cancel; QElapsedTimer timer; Id points=0,patches=0;
    explicit Job(QString path):cancel(std::move(path)) {timer.start();check();}
    void check() const {
        if(QFileInfo::exists(cancel))bad("IGES 曲面准备已取消。");
        if(timer.elapsed()>90000)bad("IGES 曲面准备超过 90 秒，请分图层导出。");
    }
    void reserve(Id cols,Id rows) {
        check();
        if(cols>2000000 || rows>2000000 || cols*rows>2000000-points || ++patches>100000)
            bad("IGES 曲面数量超过安全限制。");
        points+=cols*rows;
    }
};
struct Mesh {
    const SubdivisionLevel &data;
    std::map<Key,Id> edgeIds;
    std::vector<std::vector<Id>> faces,edges;
    explicit Mesh(const SubdivisionLevel &m):data(m),faces(m.points.size()),edges(m.points.size()) {
        for(Id i=0;i<m.faces.size();++i)for(auto p:m.faces[i].points)faces[p].push_back(i);
        for(Id i=0;i<m.edges.size();++i) {
            const auto &e=m.edges[i];edgeIds[key(e.points[0],e.points[1])]=i;
            if(m.level==0)for(auto p:e.points)edges[p].push_back(i);
        }
        if(m.level==1) {
            // Limit-point sums visit the first and last crease neighbor in
            // construction order. The display mesh reorders edge IDs; recover
            // the subdivision insertion order instead of using that ordering.
            std::vector<bool> seen(m.edges.size(),false);
            for(const auto &f:m.faces) {
                Id k=Id(f.childIndex)%4;const auto &p=f.points;
                for(auto pair:{key(p[(k+3)%4],p[k]),key(p[k],p[(k+1)%4]),
                               key(p[(k+1)%4],p[(k+2)%4]),key(p[(k+3)%4],p[(k+2)%4])}) {
                    const Id i=edgeIds.at(pair);if(seen[i])continue;seen[i]=true;
                    for(auto point:m.edges[i].points)edges[point].push_back(i);
                }
            }
        }
    }
    const SubdivisionEdge &edge(Id a,Id b) const { return data.edges.at(edgeIds.at(key(a,b))); }
    bool contains(Id f,Id p) const {const auto &v=data.faces[f].points;return std::find(v.begin(),v.end(),p)!=v.end();}
    bool regular(Id p,const std::set<Id> &component) const {
        const auto type=data.points[p].type;
        if(type==VertexType::Regular || type==VertexType::Dart)return faces[p].size()==4;
        if(type!=VertexType::Crease)return false;
        return std::count_if(faces[p].begin(),faces[p].end(),[&](Id f){return component.count(f);})==2;
    }
    // Opposite vertices of a quad, retaining the order of the boundary a,b.
    Key outer(Id f,Id a,Id b) const {
        const auto &p=data.faces[f].points;
        if(p.size()!=4)bad("IGES 曲面边界不是四边形。");
        auto it=std::find(p.begin(),p.end(),a);if(it==p.end())bad("IGES 曲面邻接损坏。");
        Id i=Id(it-p.begin());
        if(p[(i+1)%4]==b)return {p[(i+3)%4],p[(i+2)%4]};
        if(p[(i+3)%4]==b)return {p[(i+1)%4],p[(i+2)%4]};
        bad("IGES 曲面边界不连续。");
    }
    Key opposing(Id insideA,Id a,Id b,Id insideB) const {
        const auto &e=edge(a,b);if(e.crease)return {Missing,Missing};
        Key result{Missing,Missing};
        for(auto f:e.faces)if(!(contains(f,insideA)&&contains(f,a)&&contains(f,b)&&contains(f,insideB)))result=outer(f,a,b);
        return result;
    }
    Id fourth(Id a,Id b,Id c) const {
        Id result=Missing;if(a==b||b==c||a==c)return result;
        for(auto f:faces[b])if(contains(f,a)&&contains(f,c))
            for(auto p:data.faces[f].points)if(p!=a&&p!=b&&p!=c){result=p;break;}
        return result;
    }
    Vec3 limit(Id id) const {
        const auto &v=data.points[id];auto p=v.position;
        if(v.type==VertexType::Corner)return p;
        if(v.type==VertexType::Crease) {
            Id a=Missing,b=Missing;
            for(auto ei:edges[id]){const auto &e=data.edges[ei];if(e.crease){Id q=e.points[0]==id?e.points[1]:e.points[0];if(a==Missing)a=q;else b=q;}}
            if(a==Missing||b==Missing)bad("IGES 折角点缺少边界邻点。");
            auto u=data.points[a].position,w=data.points[b].position;
            return {(u.x+4*p.x+w.x)/6,(u.y+4*p.y+w.y)/6,(u.z+4*p.z+w.z)/6};
        }
        const float n=float(faces[id].size());if(!n)bad("IGES 极限点没有关联面。");
        Vec3 result{};
        for(auto fi:faces[id]) {
            const auto &f=data.faces[fi].points;Id i=Id(std::find(f.begin(),f.end(),id)-f.begin());
            auto u=data.points[f[(i+1)%f.size()]].position,w=data.points[f[(i+2)%f.size()]].position;
            result.x=result.x+n*p.x+4*u.x+w.x;result.y=result.y+n*p.y+4*u.y+w.y;result.z=result.z+n*p.z+4*u.z+w.z;
        }
        const float d=n*(n+5);return {result.x/d,result.y/d,result.z/d};
    }
};
Grid seed(const SubdivisionFace &f) {
    if(f.points.size()!=4)bad("IGES 曲面网格需要四边形。");
    return {{f.points[1],f.points[0]},{f.points[2],f.points[3]}};
}
Id cornerFace(const Mesh &m,const std::set<Id> &remaining) {
    for(auto f:remaining)for(auto p:m.data.faces[f].points)
        if(std::count_if(m.faces[p].begin(),m.faces[p].end(),[&](Id x){return remaining.count(x);})==1)return f;
    return *remaining.rbegin();
}
// Grow complete rectangular strips. IDs, not allocation addresses, determine
// traversal. A layer boundary ends a patch so visibility/color remain accurate.
Grid assemble(const Mesh &m,Id first,std::set<Id> &remaining,const std::set<Id> &component,Job &job) {
    Grid g=seed(m.data.faces[first]);remaining.erase(first);
    std::array<bool,4> search{true,true,true,true}; // bottom, right, top, left
    auto corners=[&]{
        if(!m.regular(g.front().front(),component)){search[2]=false;search[3]=false;}
        if(!m.regular(g.back().front(),component)){search[0]=false;search[3]=false;}
        if(!m.regular(g.back().back(),component)){search[0]=false;search[1]=false;}
        if(!m.regular(g.front().back(),component)){search[2]=false;search[1]=false;}
    };
    corners();
    while(!remaining.empty() && std::any_of(search.begin(),search.end(),[](bool b){return b;})) {
        for(int dir=0;dir<4;++dir) {
            if(!search[dir])continue;
            job.check();
            const bool horizontal=dir%2==0;std::vector<Id> boundary;
            if(horizontal)boundary=dir==0?g.back():g.front();
            else for(const auto &row:g)boundary.push_back(dir==1?row.back():row.front());
            std::vector<Id> strip,outer(boundary.size(),Missing);bool ok=true;
            for(Id i=1;i<boundary.size() && ok;++i) {
                const auto &e=m.edge(boundary[i-1],boundary[i]);Id found=Missing;
                if(!e.crease)for(auto f:e.faces)if(remaining.count(f) && m.data.faces[f].points.size()==4 &&
                    m.data.faces[f].layer==m.data.faces[first].layer && std::find(strip.begin(),strip.end(),f)==strip.end()) {
                    if(!strip.empty()) {
                        int shared=0;for(auto p:m.data.faces[f].points)if(m.contains(strip.back(),p))++shared;
                        if(shared<2)continue;
                    }
                    found=f;break;
                }
                if(found==Missing){ok=false;break;}
                auto [a,b]=m.outer(found,boundary[i-1],boundary[i]);
                if(i>1 && outer[i-1]!=a){ok=false;break;}
                outer[i-1]=a;outer[i]=b;strip.push_back(found);
            }
            if(ok) {
                auto inward=[&](Id i)->Id {
                    if(horizontal)return dir==0?g[g.size()-2][i]:g[1][i];
                    return dir==1?g[i][g[i].size()-2]:g[i][1];
                };
                for(Id i:{Id(0),boundary.size()-1})
                    if(m.edge(outer[i],boundary[i]).crease!=m.edge(boundary[i],inward(i)).crease)ok=false;
                // Retain Plus's column-end regularity convention; changing
                // the chosen patches also changes its corner interpolation.
                for(Id i=1;i+1<outer.size();++i)if(!m.regular(outer[horizontal?i:i-1],component))ok=false;
            }
            if(!ok){search[dir]=false;continue;}
            for(auto f:strip)remaining.erase(f);
            if(horizontal){if(dir==0)g.push_back(std::move(outer));else g.insert(g.begin(),std::move(outer));}
            else for(Id i=0;i<g.size();++i){if(dir==1)g[i].push_back(outer[i]);else g[i].insert(g[i].begin(),outer[i]);}
            corners();
        }
    }
    return g;
}
Grid canonical(const Grid &g,const Mesh &m) {
    const Id rows=g.size(),cols=g[0].size();
    auto at=[&](int d,Id x,Id y){switch(d){case 0:return g[y][x];case 1:return g[rows-1-x][y];case 2:return g[rows-1-y][cols-1-x];default:return g[x][cols-1-y];}};
    int best=0;
    for(int d=1;d<4;++d) {
        const Id c=d%2?rows:cols,bc=best%2?rows:cols;if(c>bc)continue;
        bool less=c<bc;
        if(c==bc)for(Id i=0;i<rows*cols;++i) {
            auto a=m.data.points[at(d,i%c,i/c)].position,b=m.data.points[at(best,i%c,i/c)].position;
            auto u=std::tie(a.x,a.y,a.z),v=std::tie(b.x,b.y,b.z);
            if(u!=v){less=u<v;break;}
        }
        if(less)best=d;
    }
    const Id c=best%2?rows:cols,r=best%2?cols:rows;Grid out(r,std::vector<Id>(c));
    for(Id y=0;y<r;++y)for(Id x=0;x<c;++x)out[y][x]=at(best,x,y);
    return out;
}
struct Refinement {
    std::vector<Id> vertex,center;
    std::map<Key,Id> midpoint;
    std::vector<std::vector<Id>> children;
    Refinement(const Mesh &control,const Mesh &fine):vertex(control.data.points.size(),Missing),center(control.data.faces.size(),Missing),children(control.data.faces.size()) {
        for(Id i=0;i<fine.data.faces.size();++i) {
            const auto &child=fine.data.faces[i];Id f=child.rootControlFace,k=Id(child.childIndex);const auto &p=control.data.faces[f].points;
            if(child.points.size()!=4 || k>=p.size())bad("IGES 细分网格损坏。");
            vertex[p[k]]=child.points[k%4];midpoint[key(p[k],p[(k+1)%p.size()])]=child.points[(k+1)%4];
            center[f]=child.points[(k+2)%4];children[f].push_back(i);
        }
    }
    Grid grid(const Grid &g,const Mesh &control) const {
        const Id rows=g.size(),cols=g[0].size();Grid out(2*rows-1,std::vector<Id>(2*cols-1,Missing));
        for(Id y=0;y<rows;++y)for(Id x=0;x<cols;++x) {
            out[2*y][2*x]=vertex.at(g[y][x]);
            if(x)out[2*y][2*x-1]=midpoint.at(key(g[y][x-1],g[y][x]));
            if(y)out[2*y-1][2*x]=midpoint.at(key(g[y-1][x],g[y][x]));
            if(x&&y) {
                Id f=Missing;for(auto candidate:control.faces[g[y][x]])
                    if(control.contains(candidate,g[y][x-1])&&control.contains(candidate,g[y-1][x])&&control.contains(candidate,g[y-1][x-1])){f=candidate;break;}
                if(f==Missing)bad("IGES 网格没有完整覆盖控制面。");
                out[2*y-1][2*x-1]=center.at(f);
            }
        }
        return out;
    }
};
Vec3 phantom(Vec3 a,Vec3 b) {return {2*a.x-b.x,2*a.y-b.y,2*a.z-b.z};}
Vec3 corner(Vec3 p5,Vec3 p6,Vec3 p8,Vec3 p9) {
    auto p2=phantom(p5,p8),p3=phantom(p6,p9),p4=phantom(p5,p6),p7=phantom(p8,p9);
    return {20*p5.x-4*p2.x-p3.x-4*p4.x-4*p6.x-p7.x-4*p8.x-p9.x,
            20*p5.y-4*p2.y-p3.y-4*p4.y-4*p6.y-p7.y-4*p8.y-p9.y,
            20*p5.z-4*p2.z-p3.z-4*p4.z-4*p6.z-p7.z-4*p8.z-p9.z};
}
struct Nurbs {
    std::vector<std::vector<Vec3>> p;std::vector<float> u,v;
    Nurbs(Id cols,Id rows):p(rows,std::vector<Vec3>(cols)),u(cols+4),v(rows+4) {
        for(auto *knots:{&u,&v})for(Id i=0;i<knots->size();++i)(*knots)[i]=float(double(i)/double(knots->size()-1));
    }
    void insert(bool column,float t) {
        auto &knots=column?u:v;const Id n=column?p[0].size():p.size();Id index=Missing;
        // The legacy row and column insertion use opposite sides of repeated
        // knots. Preserve both, including Single storage / Double blending.
        for(Id j=1;j<knots.size();++j)if(column?(knots[j-1]<t&&knots[j]>=t):(knots[j-1]<=t&&knots[j]>t)){index=j-1;break;}
        if(index==Missing)bad("IGES 节点插入参数无效。");
        const auto boundary=column?std::max<std::ptrdiff_t>(0,std::ptrdiff_t(index)-3):std::ptrdiff_t(index)-3;
        std::vector<double> alpha(n+1);
        for(Id j=0;j<=n;++j) {
            if(std::ptrdiff_t(j)<=boundary)alpha[j]=1;
            else if(j<=index) {
                const float den=knots[j+3]-knots[j];
                // FPC evaluates the Single quotient before assigning Double.
                alpha[j]=den==0?0:double((t-knots[j])/den);
            }
        }
        auto next=p;if(column)for(auto &row:next)row.resize(n+1);else next.emplace_back(p[0].size());
        for(Id y=0;y<next.size();++y)for(Id x=0;x<next[y].size();++x) {
            const Id i=column?x:y;const double a=alpha[i];
            if(a==1){if(i>=n)bad("IGES 节点索引越界。");next[y][x]=p[y][x];}
            else if(a==0){if(!i)bad("IGES 节点索引越界。");next[y][x]=column?p[y][x-1]:p[y-1][x];}
            else {
                if(!i||i>=n)bad("IGES 节点索引越界。");
                auto before=column?p[y][x-1]:p[y-1][x],after=p[y][x];
                next[y][x]={float((1-a)*before.x+a*after.x),float((1-a)*before.y+a*after.y),float((1-a)*before.z+a*after.z)};
            }
        }
        p=std::move(next);knots.insert(knots.begin()+std::ptrdiff_t(index+1),t);
    }
    void clamp() {
        for(int i=0;i<3;++i)insert(true,u[3]);
        for(int i=0;i<3;++i)insert(true,u[p[0].size()]);
        for(int i=0;i<3;++i)insert(false,v[3]);
        for(int i=0;i<3;++i)insert(false,v[p.size()]);
        for(auto &row:p)row=std::vector<Vec3>(row.begin()+3,row.end()-3);
        p=std::vector<std::vector<Vec3>>(p.begin()+3,p.end()-3);
        for(auto *k:{&u,&v}) {
            Id n=k==&u?p[0].size():p.size();k->assign(n+4,1);
            for(Id i=0;i<3;++i)(*k)[i]=0;
            for(Id i=0;i<n-2;++i)(*k)[i+3]=float(double(i)/double(n-3));
        }
    }
};
QJsonObject patch(Grid g,const Mesh &m,qint64 layer,Job &job) {
    g=canonical(g,m);const Id rows=g.size(),cols=g[0].size();job.reserve(cols+2,rows+2);
    Nurbs n(cols+2,rows+2);auto &p=n.p;
    std::vector<Id> bottom(cols,Missing),top(cols,Missing),left(rows,Missing),right(rows,Missing);
    bool bp=true,tp=true,lp=true,rp=true;
    for(Id i=1;i<cols;++i) {
        std::tie(bottom[i-1],bottom[i])=m.opposing(g[rows-2][i-1],g[rows-1][i-1],g[rows-1][i],g[rows-2][i]);
        bp=bp&&bottom[i-1]!=Missing&&bottom[i]!=Missing;
        std::tie(top[i],top[i-1])=m.opposing(g[1][i],g[0][i],g[0][i-1],g[1][i-1]);
        tp=tp&&top[i-1]!=Missing&&top[i]!=Missing;
    }
    for(Id i=1;i<rows;++i) {
        std::tie(right[i],right[i-1])=m.opposing(g[i][cols-2],g[i][cols-1],g[i-1][cols-1],g[i-1][cols-2]);
        rp=rp&&right[i-1]!=Missing&&right[i]!=Missing;
        std::tie(left[i-1],left[i])=m.opposing(g[i-1][1],g[i-1][0],g[i][0],g[i][1]);
        lp=lp&&left[i-1]!=Missing&&left[i]!=Missing;
    }
    for(Id y=0;y<rows;++y)for(Id x=0;x<cols;++x)p[y+1][x+1]=m.data.points[g[y][x]].position;
    auto position=[&](Id i){return m.data.points.at(i).position;};
    for(Id x=1;x<=cols;++x){p[0][x]=tp?position(top[x-1]):phantom(p[1][x],p[2][x]);p[rows+1][x]=bp?position(bottom[x-1]):phantom(p[rows][x],p[rows-1][x]);}
    for(Id y=1;y<=rows;++y){p[y][0]=lp?position(left[y-1]):phantom(p[y][1],p[y][2]);p[y][cols+1]=rp?position(right[y-1]):phantom(p[y][cols],p[y][cols-1]);}
    auto cornerId=[&](bool a,bool b,Id p1,Id p2,Id p3){return a&&b?m.fourth(p1,p2,p3):Missing;};
    const Id tl=cornerId(tp,lp,left[0],g[0][0],top[0]),tr=cornerId(tp,rp,right[0],g[0][cols-1],top[cols-1]);
    const Id bl=cornerId(bp,lp,left[rows-1],g[rows-1][0],bottom[0]),br=cornerId(bp,rp,right[rows-1],g[rows-1][cols-1],bottom[cols-1]);
    p[0][0]=tl!=Missing?position(tl):corner(p[1][1],p[1][2],p[2][1],p[2][2]);
    p[0][cols+1]=tr!=Missing?position(tr):corner(p[1][cols],p[1][cols-1],p[2][cols],p[2][cols-1]);
    p[rows+1][0]=bl!=Missing?position(bl):corner(p[rows][1],p[rows][2],p[rows-1][1],p[rows-1][2]);
    p[rows+1][cols+1]=br!=Missing?position(br):corner(p[rows][cols],p[rows][cols-1],p[rows-1][cols],p[rows-1][cols-1]);
    n.clamp();p[0][0]=m.limit(g[0][0]);p[0][cols+1]=m.limit(g[0][cols-1]);p[rows+1][0]=m.limit(g[rows-1][0]);p[rows+1][cols+1]=m.limit(g[rows-1][cols-1]);
    QJsonArray points,u,v;
    for(const auto &row:p)for(auto q:row) {
        if(!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z))bad("IGES 曲面坐标不是有限数。");
        points.append(QJsonArray{double(q.x),double(q.y),double(q.z)});
    }
    for(auto k:n.u)u.append(double(k));
    for(auto k:n.v)v.append(double(k));
    return {{"layer",layer},{"cols",qint64(cols+2)},{"rows",qint64(rows+2)},{"degree_u",3},{"degree_v",3},{"u",u},{"v",v},{"points",points}};
}
}
QJsonObject cadPatches(const Fbm &source,const QJsonObject &bindings,const QString &cancelFile) {
    Job job(cancelFile);auto input=documentGeometry(source);input.subdivisionMode=SubdivisionMode::CatmullClark;input.desiredLevel=1;
    const auto ids=bindings["layers"].toArray();if(ids.size()!=qsizetype(input.layers.size()))bad("IGES 图层标识不完整。");
    SubdivisionSurface surface(std::move(input));Mesh control(surface.level(0)),fine(surface.level(1));job.check();
    Refinement refined(control,fine);std::set<Id> pending;
    for(Id i=0;i<control.data.faces.size();++i)pending.insert(i);
    QJsonArray patches;
    while(!pending.empty()) {
        job.check();std::set<Id> component;std::vector<Id> stack{*pending.rbegin()};pending.erase(stack.back());
        while(!stack.empty()) {
            auto f=stack.back();stack.pop_back();component.insert(f);const auto &p=control.data.faces[f].points;
            for(Id i=0;i<p.size();++i){const auto &e=control.edge(p[i],p[(i+1)%p.size()]);if(!e.crease)for(auto neighbor:e.faces)if(pending.erase(neighbor))stack.push_back(neighbor);}
            if(component.size()%1024==0)job.check();
        }
        auto remaining=component;
        while(!remaining.empty()) {
            job.check();Id first=cornerFace(control,remaining);const auto &face=control.data.faces[first];qint64 layer=ids[qsizetype(face.layer)].toInteger();
            if(layer<=0)bad("IGES 图层标识无效。");
            if(face.points.size()==4)patches.append(patch(refined.grid(assemble(control,first,remaining,component,job),control),fine,layer,job));
            else {remaining.erase(first);for(auto child:refined.children[first])patches.append(patch(seed(fine.data.faces[child]),fine,layer,job));}
        }
    }
    job.check();if(patches.empty())bad("模型没有可导出的 IGES 曲面。");
    return {{"schema",1},{"patches",patches}};
}
}
