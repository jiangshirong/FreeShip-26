// GPL-2.0-or-later. Operation semantics follow TopologyEdits/ModelingEdits and
// FreeSubdivisionControl{Point,Edge,Face,Curve}. Geometry remains in FBM Singles.
// Mutations happen on a private net. Only a fully serialized candidate is returned.
#include "topology_edit.h"
#include "point_edit.h"
#include "construction.h"
#include "geometry_input.h"
#include "native/geometry.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *s) { throw std::invalid_argument(s); }
quint64 integer(QJsonValue v, quint64 minimum=1) {
    const double d=v.toDouble(-1);
    if(!v.isDouble()||!std::isfinite(d)||d<double(minimum)||d>2000000||std::floor(d)!=d) bad("Invalid entity ID");
    return quint64(d);
}
bool flag(const QJsonObject &r,const char *key) {
    if(!r[key].isBool()) bad("Invalid boolean field"); return r[key].toBool();
}
quint64 pairKey(int a,int b) { if(a>b)std::swap(a,b);return (quint64(a)<<32)|quint32(b); }
bool samePair(int a,int b,int c,int d){return (a==c&&b==d)||(a==d&&b==c);}
bool coincident(const FbmControlPoint &a,const FbmControlPoint &b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
FbmPosition position(const FbmControlPoint&p){return {p.x,p.y,p.z};}
void check(FbmPosition p){for(float v:{p.x,p.y,p.z})if(!std::isfinite(v)||std::abs(v)>1e9f)bad("Invalid coordinate");}
FbmPosition vector(const QJsonObject&r,const char*key){
    if(!r[key].isArray()||r[key].toArray().size()!=3)bad("Vector requires three coordinates");auto a=r[key].toArray();float v[3];
    for(int i=0;i<3;++i){double d=a[i].toDouble();if(!a[i].isDouble()||!std::isfinite(d)||std::abs(d)>1e9)bad("Invalid coordinate");v[i]=float(d);}return {v[0],v[1],v[2]};
}
FbmControlPoint at(FbmPosition p){FbmControlPoint value;value.x=p.x;value.y=p.y;value.z=p.z;return value;}
FbmPosition offset(const FbmControlPoint&p,FbmPosition d){FbmPosition out{p.x+d.x,std::max(0.0f,p.y+d.y),p.z+d.z};check(out);return out;}
template<class T> bool contains(const std::vector<T> &v,T x){return std::find(v.begin(),v.end(),x)!=v.end();}

struct Projected {
    std::vector<std::array<double,2>> p;
    explicit Projected(const std::vector<int>& indices,const FbmControlNet& net){
        double n[3]{};
        for(size_t i=0;i<indices.size();++i){const auto&a=net.points[indices[i]].value;const auto&b=net.points[indices[(i+1)%indices.size()]].value;
            n[0]+=(double(a.y)-b.y)*(double(a.z)+b.z);n[1]+=(double(a.z)-b.z)*(double(a.x)+b.x);n[2]+=(double(a.x)-b.x)*(double(a.y)+b.y);}
        int axis=0;if(std::abs(n[1])>std::abs(n[axis]))axis=1;if(std::abs(n[2])>std::abs(n[axis]))axis=2;
        if(n[axis]==0)bad("Face is degenerate or has crossing edges");
        for(int i:indices){const auto&a=net.points[i].value;p.push_back({axis==0?double(a.y):double(a.x),axis==2?double(a.y):double(a.z)});}
    }
    double turn(int a,int b,int c)const{return (p[b][0]-p[a][0])*(p[c][1]-p[a][1])-(p[b][1]-p[a][1])*(p[c][0]-p[a][0]);}
    bool intersects(int a,int b,int c,int d)const{
        for(int k=0;k<2;++k)if(std::max(p[a][k],p[b][k])<std::min(p[c][k],p[d][k])||std::max(p[c][k],p[d][k])<std::min(p[a][k],p[b][k]))return false;
        const double t1=turn(a,b,c),t2=turn(a,b,d),t3=turn(c,d,a),t4=turn(c,d,b);
        return ((t1<=0&&t2>=0)||(t1>=0&&t2<=0))&&((t3<=0&&t4>=0)||(t3>=0&&t4<=0));
    }
    void diagonal(int a,int b)const{
        for(int i=0;i<int(p.size());++i){int j=(i+1)%p.size();
            if(i!=a&&i!=b&&turn(a,b,i)==0&&p[i][0]>=std::min(p[a][0],p[b][0])&&p[i][0]<=std::max(p[a][0],p[b][0])&&p[i][1]>=std::min(p[a][1],p[b][1])&&p[i][1]<=std::max(p[a][1],p[b][1]))bad("Diagonal touches another face vertex");
            if(i==a||i==b||j==a||j==b)continue;
            double t1=turn(a,b,i),t2=turn(a,b,j),t3=turn(i,j,a),t4=turn(i,j,b);
            if(((t1<0&&t2>0)||(t1>0&&t2<0))&&((t3<0&&t4>0)||(t3>0&&t4<0)))bad("Diagonal crosses a face boundary");
        }
        double x=(p[a][0]+p[b][0])/2,y=(p[a][1]+p[b][1])/2;bool inside=false;
        for(int i=0;i<int(p.size());++i){int j=(i+1)%p.size();if((p[i][1]>y)!=(p[j][1]>y))if(x<p[i][0]+(y-p[i][1])*(p[j][0]-p[i][0])/(p[j][1]-p[i][1]))inside=!inside;}
        if(!inside)bad("Diagonal lies outside the face");
    }
};

class Net {
public:
    FbmControlNet n;
    QJsonObject bindings,high;
    std::vector<bool> pointLive,edgeLive,faceLive,curveLive;
    std::vector<std::vector<int>> pointEdges,pointFaces,edgeFaces;
    QHash<quint64,int> edgeLookup;
    std::array<std::vector<quint64>,4> ids;
    std::array<QHash<quint64,int>,4> lookup;
    const std::array<QString,4> kinds{"points","edges","faces","curves"};
    bool changed=false,geometry=true;
    explicit Net(const Fbm& source,const QJsonObject& b):n(source.controlNet()),bindings(b),high(b["high_water"].toObject()){
        std::array<size_t,4> sizes{n.points.size(),n.edges.size(),n.faces.size(),n.curves.size()};
        for(int k=0;k<4;++k){auto a=b[kinds[k]].toArray();if(size_t(a.size())!=sizes[k])bad("Control net binding count mismatch");
            integer(high[kinds[k]],0);
            for(int i=0;i<a.size();++i){auto id=integer(a[i]);if(lookup[k].contains(id)||id>integer(high[kinds[k]],0))bad("Invalid control net bindings");ids[k].push_back(id);lookup[k].insert(id,i);}}
        pointLive.resize(n.points.size(),true);edgeLive.resize(n.edges.size(),true);faceLive.resize(n.faces.size(),true);curveLive.resize(n.curves.size(),true);adjacency();
        if(b.contains("point_edges")){
            auto order=b["point_edges"].toArray();if(size_t(order.size())!=n.points.size())bad("Invalid point edge order");
            for(int p=0;p<order.size();++p){auto values=order[p].toArray();if(size_t(values.size())!=pointEdges[p].size())bad("Incomplete point edge order");std::vector<int> sorted;QSet<int> seen;
                for(auto value:values){int e=resolve(1,value);if(!contains(pointEdges[p],e)||seen.contains(e))bad("Invalid ordered incident edge");seen.insert(e);sorted.push_back(e);}pointEdges[p]=std::move(sorted);}
        }
    }
    void adjacency(){
        auto previous=std::move(pointEdges);
        pointEdges.assign(n.points.size(),{});pointFaces.assign(n.points.size(),{});edgeFaces.assign(n.edges.size(),{});edgeLookup.clear();
        for(int i=0;i<int(n.edges.size());++i)if(edgeLive[i]){auto e=n.edges[i];auto key=pairKey(e.a,e.b);if(edgeLookup.contains(key))bad("Duplicate control edge");edgeLookup.insert(key,i);pointEdges[e.a].push_back(i);pointEdges[e.b].push_back(i);}
        for(int i=0;i<int(n.faces.size());++i)if(faceLive[i]){const auto& p=n.faces[i].points;for(size_t j=0;j<p.size();++j){pointFaces[p[j]].push_back(i);int e=edge(p[j],p[(j+1)%p.size()]);if(e<0)bad("Face boundary has no edge");edgeFaces[e].push_back(i);}}
        for(size_t p=0;p<std::min(previous.size(),pointEdges.size());++p){std::vector<int> ordered;for(int e:previous[p])if(contains(pointEdges[p],e))ordered.push_back(e);for(int e:pointEdges[p])if(!contains(ordered,e))ordered.push_back(e);pointEdges[p]=std::move(ordered);}
    }
    int edge(int a,int b)const{return edgeLookup.value(pairKey(a,b),-1);}
    int resolve(int kind,QJsonValue v)const{auto it=lookup[kind].constFind(integer(v));if(it==lookup[kind].cend())bad("Unknown or deleted entity ID");return *it;}
    std::vector<int> selection(int kind,const QJsonObject&r)const{
        if(!r["ids"].isArray())bad("Invalid selection");auto a=r["ids"].toArray();if(a.isEmpty()||a.size()>100000)bad("Invalid selection size");
        QSet<int> seen;std::vector<int> out;for(auto v:a){int i=resolve(kind,v);if(seen.contains(i))bad("Duplicate entity ID");seen.insert(i);out.push_back(i);}return out;
    }
    int layer(QJsonValue v)const{auto id=integer(v);auto a=bindings["layers"].toArray();for(int i=0;i<a.size();++i)if(integer(a[i])==id)return i;bad("Unknown layer ID");}
    void editable(int i)const{const auto&p=n.points[i].value;if(p.locked||p.hardAnchor||p.constrained)bad("Topology touches a locked or constrained point");}
    void editableFace(int f)const{for(int i:n.faces[f].points)editable(i);}
    int faceCount(int e)const{int c=0;for(int f:edgeFaces[e])if(faceLive[f])++c;return c;}
    int pointFaceCount(int p)const{int c=0;for(int f:pointFaces[p])if(faceLive[f])++c;return c;}
    bool visibleEdge(int e)const{
        if(faceCount(e)>0||n.edges[e].selected)return true;
        bool selected=false;const auto endpoints=n.edges[e];
        for(int c=0;c<int(n.curves.size());++c)if(curveLive[c]){const auto&curve=n.curves[c];
            for(std::size_t j=1;j<curve.points.size();++j)if(samePair(curve.points[j-1],curve.points[j],endpoints.a,endpoints.b))selected=curve.selected;}
        return selected;
    }
    void crease(int e,bool value){
        if(faceCount(e)<2)value=true;if(n.edges[e].crease==value)return;n.edges[e].crease=value;
        for(int p:{n.edges[e].a,n.edges[e].b}){int count=0;for(int i:pointEdges[p])if(edgeLive[i]&&n.edges[i].crease)++count;auto& type=n.points[p].value.vertexType;
            if(type==3){if(pointFaceCount(p)>1&&count==2)type=1;}else type=count==0?0:count==1?2:count==2?1:3;}
    }
    int addPoint(FbmControlPoint p){int i=n.points.size();n.points.push_back({p,-1});pointLive.push_back(true);pointEdges.emplace_back();pointFaces.emplace_back();ids[0].push_back(0);return i;}
    int addEdge(int a,int b){int existing=edge(a,b);if(existing>=0)return existing;int e=n.edges.size();n.edges.push_back({a,b,false,false,-1});edgeLive.push_back(true);edgeFaces.emplace_back();pointEdges[a].push_back(e);pointEdges[b].push_back(e);edgeLookup.insert(pairKey(a,b),e);ids[1].push_back(0);return e;}
    int weldPoint(FbmPosition p){
        // Original AddControlPoint searches backward, preferring connected points.
        for(bool connected:{true,false})for(int i=int(n.points.size())-1;i>=0;--i)if(pointLive[i]){
            bool hasEdge=false;for(int e:pointEdges[i])if(edgeLive[e])hasEdge=true;if(hasEdge!=connected)continue;
            const auto&q=n.points[i].value;float x=p.x-q.x,y=p.y-q.y,z=p.z-q.z;
            if(x*x+y*y+z*z<=3e-12)return i;
        }return addPoint(at(p));
    }
    int direction(int f,int a,int b)const{const auto&p=n.faces[f].points;for(size_t j=0;j<p.size();++j){if(p[j]==a&&p[(j+1)%p.size()]==b)return 1;if(p[j]==b&&p[(j+1)%p.size()]==a)return -1;}return 0;}
    int edgeCount(int p)const{int count=0;for(int e:pointEdges[p])if(edgeLive[e])++count;return count;}
    int addFace(const std::vector<int>& p,int layer,bool check){
        auto sorted=p;std::sort(sorted.begin(),sorted.end());
        for(int f:pointFaces[p[0]])if(faceLive[f]&&n.faces[f].points.size()==p.size()){auto other=n.faces[f].points;std::sort(other.begin(),other.end());if(other==sorted)bad("Face already exists");}
        int f=n.faces.size();n.faces.push_back({p,layer,false,-1});faceLive.push_back(true);ids[2].push_back(0);for(int i:p)pointFaces[i].push_back(f);
        int previous=p.back();for(int current:p){int e=edge(previous,current);bool fresh=e<0;if(fresh)e=addEdge(previous,current);edgeFaces[e].push_back(f);if(fresh)crease(e,false);else if(check)crease(e,n.edges[e].crease||faceCount(e)<2);previous=current;}return f;
    }
    void detachCurve(int e){
        const auto endpoints=n.edges[e];const int count=n.curves.size();
        for(int c=0;c<count;++c)if(curveLive[c]){
            const auto original=n.curves[c];for(size_t j=1;j<original.points.size();++j)if(samePair(original.points[j-1],original.points[j],endpoints.a,endpoints.b)){
                auto append=[&](auto first,auto last){if(last-first<2)return;FbmNetCurve part;part.points.assign(first,last);part.selected=original.selected;n.curves.push_back(std::move(part));curveLive.push_back(true);ids[3].push_back(0);};
                append(original.points.begin(),original.points.begin()+j);append(original.points.begin()+j,original.points.end());curveLive[c]=false;break;
            }
        }
    }
    void deleteFace(int f){
        if(!faceLive[f])return;faceLive[f]=false;const auto p=n.faces[f].points;int previous=p.back();
        // Current Plus Unreference removes the face from the surface before
        // Delete's cleanup block. Its wire edges survive; preserve that behavior.
        // UnreferenceFace changes the raw crease flag, not endpoint vertex types.
        for(int current:p){int e=edge(previous,current);if(e>=0){int count=faceCount(e);if(count<=1)n.edges[e].crease=count==1;}previous=current;}
    }
    void deleteEdge(int e){
        if(!edgeLive[e])return;detachCurve(e);edgeLive[e]=false;edgeLookup.remove(pairKey(n.edges[e].a,n.edges[e].b));
        for(int f:edgeFaces[e])if(faceLive[f])deleteFace(f);
    }
    void deletePoint(int p){
        if(!pointLive[p])return;for(int e:pointEdges[p])if(edgeLive[e])detachCurve(e);
        for(int f:pointFaces[p])if(faceLive[f])deleteFace(f);for(int e:pointEdges[p])if(edgeLive[e])deleteEdge(e);pointLive[p]=false;
    }
    int splitAt(int e,FbmPosition coordinate){
        auto old=n.edges[e];auto a=n.points[old.a].value,b=n.points[old.b].value;auto p=at(coordinate);check(coordinate);
        auto distance=[&](const auto& q){float x=p.x-q.x,y=p.y-q.y,z=p.z-q.z;return std::sqrt(x*x+y*y+z*z);};
        if(distance(a)<1e-4||distance(b)<1e-4)return -1;p.vertexType=old.crease?1:0;int index=addPoint(p);
        for(size_t c=0;c<n.curves.size();++c)if(curveLive[c]){auto& v=n.curves[c].points;for(size_t j=1;j<v.size();++j)if(samePair(v[j-1],v[j],old.a,old.b)){v.insert(v.begin()+j,index);++j;}}
        for(int f:edgeFaces[e])if(faceLive[f]){auto&v=n.faces[f].points;for(size_t j=0;j<v.size();++j)if(samePair(v[(j+v.size()-1)%v.size()],v[j],old.a,old.b)){v.insert(v.begin()+j,index);break;}}
        n.edges[e].b=index;int next=n.edges.size();n.edges.push_back({index,old.b,old.crease,false,-1});edgeLive.push_back(true);ids[1].push_back(0);pointEdges[index]={next,e};adjacency();changed=true;return index;
    }
    void split(int e){
        const auto old=n.edges[e];editable(old.a);editable(old.b);const auto a=n.points[old.a].value,b=n.points[old.b].value;
        if(splitAt(e,{float((double(a.x)+b.x)/2),float((double(a.y)+b.y)/2),float((double(a.z)+b.z)/2)})<0)bad("Edge is too short to split");
    }
    int splitFace(int f,int a,int b){
        if(edge(a,b)>=0)return -1;const auto original=n.faces[f];const auto&v=original.points;
        const auto first=std::find(v.begin(),v.end(),a),last=std::find(v.begin(),v.end(),b);
        if(first==v.end()||last==v.end()||a==b)bad("Invalid face split endpoints");
        auto path=[&](int start,int end){std::vector<int> p{v[start]};for(int j=(start+1)%v.size();;j=(j+1)%v.size()){p.push_back(v[j]);if(j==end)break;}return p;};
        const int ia=first-v.begin(),ib=last-v.begin();
        addFace(path(ia,ib),original.layer,false);addFace(path(ib,ia),original.layer,false);deleteFace(f);int e=edge(a,b);crease(e,false);return e;
    }
    std::vector<int> connectIntersections(const std::vector<int>& points,bool selectFaces){
        std::vector<int> added;
        for(int a:points){std::size_t j=0;while(j<pointFaces[a].size()){
            const int f=pointFaces[a][j];if(!faceLive[f]){++j;continue;}
            const auto members=n.faces[f].points;bool inserted=false;
            for(int b:members)if(b!=a&&contains(points,b)&&edge(a,b)<0){
                int e=splitFace(f,a,b);if(e<0)continue;n.edges[e].selected=true;added.push_back(e);
                if(selectFaces)for(int face:edgeFaces[e])if(faceLive[face])n.faces[face].selected=true;
                inserted=true;break;
            }
            if(!inserted)++j;
        }}
        return added;
    }
    void curvesFromEdges(std::vector<int> remaining){
        // Plus IsolateEdges/SortEdges also changes edge orientation. Preserve it
        // so curve endpoints, serialization and exported spline direction agree.
        while(!remaining.empty()){
            std::vector<int> chain{remaining.front()};remaining.erase(remaining.begin());
            for(bool found=true;found;){found=false;for(std::size_t i=0;i<remaining.size();++i){
                const auto candidate=n.edges[remaining[i]],first=n.edges[chain.front()],last=n.edges[chain.back()];
                auto touches=[&](const FbmNetEdge&e){return candidate.a==e.a||candidate.a==e.b||candidate.b==e.a||candidate.b==e.b;};
                if(touches(first)){chain.insert(chain.begin(),remaining[i]);found=true;}
                else if(touches(last)){chain.push_back(remaining[i]);found=true;}
                if(found){remaining.erase(remaining.begin()+i);break;}
            }}
            auto reverse=[&](int e){std::swap(n.edges[e].a,n.edges[e].b);};
            if(chain.size()>1){auto a=n.edges[chain[0]],b=n.edges[chain[1]];
                if(a.a==b.a)reverse(chain[0]);else if(a.a==b.b){reverse(chain[0]);reverse(chain[1]);}
                else if(a.b==b.b)reverse(chain[1]);
                for(std::size_t i=2;i<chain.size();++i)if(n.edges[chain[i-1]].b==n.edges[chain[i]].b)reverse(chain[i]);}
            FbmNetCurve curve;curve.points.push_back(n.edges[chain[0]].a);for(int e:chain){
                if(curve.points.back()!=n.edges[e].a)bad("Intersection curve edges are disconnected");curve.points.push_back(n.edges[e].b);}
            n.curves.push_back(std::move(curve));curveLive.push_back(true);ids[3].push_back(0);
        }
    }
    void importGrid(const ConstructionGrid& grid,int layer){
        const int start=n.points.size(),oldEdges=n.edges.size();for(auto p:grid.points)addPoint(at(p));
        const int width=grid.columns+(grid.cylinder?1:0),height=grid.rows;
        auto point=[&](int col,int row){return start+(col%grid.columns)*height+row;};
        for(int row=1;row<height;++row)for(int col=1;col<width;++col)
            addFace({point(col-1,row-1),point(col,row-1),point(col,row),point(col-1,row)},layer,true);
        for(int row=0;row<height;++row)for(int col=0;col<width;++col){int p=point(col,row);for(int e:pointEdges[p])crease(e,false);if(pointFaceCount(p)<2)n.points[p].value.vertexType=3;}
        if(grid.cylinder){
            for(int e=oldEdges;e<int(n.edges.size());++e)crease(e,false);
            if(grid.closedStart){std::vector<int> cap;for(int i=grid.columns-1;i>=0;--i)cap.push_back(point(i,0));addFace(cap,layer,true);}
            if(grid.closedEnd){std::vector<int> cap;for(int i=0;i<grid.columns;++i)cap.push_back(point(i,1));addFace(cap,layer,true);}
        }changed=true;
    }
    void allocate(){
        std::array<const std::vector<bool>*,4> live{&pointLive,&edgeLive,&faceLive,&curveLive};
        for(int k=0;k<4;++k){auto top=integer(high[kinds[k]],0);for(size_t i=0;i<ids[k].size();++i)if((*live[k])[i]&&!ids[k][i]){if(top>=2000000)bad("Document entity ID budget exhausted");ids[k][i]=++top;}high[kinds[k]]=double(top);}
    }
    void collapsePoint(int p,bool validate){
        std::vector<int> edges;for(int e:pointEdges[p])if(edgeLive[e])edges.push_back(e);if(edges.size()!=2)bad("Collapse requires a point with exactly two edges");
        int e1=edges[0],e2=edges[1],a=n.edges[e1].a==p?n.edges[e1].b:n.edges[e1].a,b=n.edges[e2].a==p?n.edges[e2].b:n.edges[e2].a;
        if(validate){editable(p);editable(a);editable(b);}if(a==b||edge(a,b)>=0)bad("Collapse would create a duplicate edge");
        for(int f:pointFaces[p])if(faceLive[f]){if(validate)editableFace(f);if(n.faces[f].points.size()<4)bad("Collapse would create a degenerate face");}
        auto curve=[&](int e){int result=-1;for(int c=0;c<int(n.curves.size());++c)if(curveLive[c]){auto&v=n.curves[c].points;for(size_t j=1;j<v.size();++j)if(samePair(v[j-1],v[j],n.edges[e].a,n.edges[e].b))result=c;}return result;};
        int c=curve(e1);if(c!=curve(e2))bad("Collapse would join different control curves");if(c>=0){auto&v=n.curves[c].points;v.erase(std::remove(v.begin(),v.end(),p),v.end());}
        bool value=n.edges[e1].crease||n.edges[e2].crease;
        for(int f:pointFaces[p])if(faceLive[f]){auto&v=n.faces[f].points;v.erase(std::remove(v.begin(),v.end(),p),v.end());}
        if(n.edges[e1].a==p)n.edges[e1].a=b;else n.edges[e1].b=b;edgeLive[e2]=false;pointLive[p]=false;adjacency();if(validate)crease(e1,value);changed=true;
    }
    void collapseEdge(int e){
        if(!edgeLive[e])bad("Unknown or deleted edge");auto endpoints=n.edges[e];std::vector<int> faces;for(int f:edgeFaces[e])if(faceLive[f])faces.push_back(f);
        if(faces.size()!=2||edgeCount(endpoints.a)<=2||edgeCount(endpoints.b)<=2)bad("Edge merge requires two adjacent faces and nonterminal endpoints");
        int f1=faces[0],f2=faces[1];editableFace(f1);editableFace(f2);if(n.faces[f1].layer!=n.faces[f2].layer)bad("Assign the faces to the same layer before merging");
        int shared=0;for(int p:n.faces[f1].points)if(contains(n.faces[f2].points,p))++shared;if(shared!=2)bad("Faces share more than the selected edge");
        detachCurve(e);QHash<int,bool> oldCreases;for(int p:n.faces[f1].points)for(int i:pointEdges[p])if(edgeLive[i])oldCreases.insert(i,n.edges[i].crease);
        if(direction(f1,endpoints.a,endpoints.b)==direction(f2,endpoints.a,endpoints.b))std::reverse(n.faces[f2].points.begin(),n.faces[f2].points.end());
        std::vector<int> boundary;for(int f:faces){const auto&v=n.faces[f].points;size_t cut=0;for(;cut<v.size();++cut)if(samePair(v[cut],v[(cut+1)%v.size()],endpoints.a,endpoints.b))break;
            if(cut==v.size())bad("Missing shared boundary");for(size_t j=1;j<=v.size();++j){int p=v[(cut+j)%v.size()];if(!contains(boundary,p))boundary.push_back(p);}}
        int merged=addFace(boundary,n.faces[f1].layer,false);deleteFace(f1);deleteFace(f2);edgeLive[e]=false;edgeLookup.remove(pairKey(endpoints.a,endpoints.b));adjacency();
        for(int p:{endpoints.a,endpoints.b})if(pointFaceCount(p)>1&&edgeCount(p)==2)collapsePoint(p,false);
        for(int p:n.faces[merged].points)for(int i:pointEdges[p])if(edgeLive[i]&&oldCreases.contains(i))crease(i,oldCreases[i]);
        for(int p:n.faces[merged].points)for(int i:pointEdges[p])if(edgeLive[i])crease(i,n.edges[i].crease||faceCount(i)<2);
        changed=true;allocate();
    }
    TopologyEdit finish(const Fbm& source){
        if(!changed)return {source,bindings,false};
        // The legacy rebuild normalizes free-curve creases and endpoints before
        // capture. Do this in the candidate too; publication must not mutate it.
        for(size_t c=0;c<n.curves.size();++c)if(curveLive[c]){
            const auto& v=n.curves[c].points;bool free=true;for(int p:v)if(pointFaceCount(p))free=false;
            if(free)for(size_t j=1;j<v.size();++j){int e=edge(v[j-1],v[j]);if(e<0)bad("Control curve has a missing edge");crease(e,true);
                auto& a=n.points[v[j-1]].value.vertexType;auto& b=n.points[v[j]].value.vertexType;
                if(j==1)a=3;if(j+1==v.size())b=3;if(a!=3)a=1;if(b!=3)b=1;}
        }
        allocate();FbmControlNet out;std::vector<int> map(n.points.size(),-1);
        for(int i=0;i<int(n.points.size());++i)if(pointLive[i]){map[i]=out.points.size();out.points.push_back(n.points[i]);}
        auto mapped=[&](int i){if(i<0)return -1;if(i>=int(map.size())||map[i]<0)bad("Deleted point still referenced");return map[i];};
        for(auto& p:out.points){p.value.anchor=mapped(p.value.anchor);p.value.lineA=mapped(p.value.lineA);p.value.lineB=mapped(p.value.lineB);}
        for(size_t i=0;i<n.edges.size();++i)if(edgeLive[i]){auto e=n.edges[i];e.a=mapped(e.a);e.b=mapped(e.b);out.edges.push_back(e);}
        for(size_t i=0;i<n.faces.size();++i)if(faceLive[i]){auto f=n.faces[i];for(auto& p:f.points)p=mapped(p);out.faces.push_back(std::move(f));}
        for(size_t i=0;i<n.curves.size();++i)if(curveLive[i]){auto c=n.curves[i];for(auto& p:c.points)p=mapped(p);out.curves.push_back(std::move(c));}
        const auto previousBindings=bindings;
        std::array<const std::vector<bool>*,4> live{&pointLive,&edgeLive,&faceLive,&curveLive};
        for(int k=0;k<4;++k){QJsonArray a;for(size_t i=0;i<ids[k].size();++i)if((*live[k])[i])a.append(double(ids[k][i]));bindings[kinds[k]]=a;}bindings["high_water"]=high;
        QJsonArray order;for(size_t p=0;p<pointEdges.size();++p)if(pointLive[p]){QJsonArray edges;for(int e:pointEdges[p])if(edgeLive[e])edges.append(double(ids[1][e]));order.append(edges);}bindings["point_edges"]=order;
        // Keep live dependency insertion order across native publication. New
        // points append; deletion removes only the actual removed relations.
        std::vector<FbmControlPoint> values;for(const auto& p:out.points)values.push_back(p.value);
        PointEditor defaults(values);defaults.writeOrder(bindings);
        const auto oldIds=previousBindings["points"].toArray(),newIds=bindings["points"].toArray();QHash<quint64,int> oldIndex;
        for(int i=0;i<oldIds.size();++i)oldIndex.insert(integer(oldIds[i]),i);
        for(const auto* key:{"point_anchors","point_lines"})if(previousBindings.contains(key)){
            const auto oldRows=previousBindings[key].toArray();auto rows=bindings[key].toArray();
            if(oldRows.size()!=oldIds.size())bad("Invalid dependency order count");
            for(int i=0;i<rows.size();++i){auto found=oldIndex.constFind(integer(newIds[i]));if(found==oldIndex.cend())continue;
                auto remaining=rows[i].toArray();QJsonArray ordered;for(auto child:oldRows[*found].toArray()){auto it=std::find(remaining.begin(),remaining.end(),child);if(it!=remaining.end()){ordered.append(child);remaining.erase(it);}}
                for(auto child:remaining)ordered.append(child);rows[i]=ordered;}
            bindings[key]=rows;
        }
        if(previousBindings.contains("layer_faces")){
            const auto old=previousBindings["layer_faces"].toArray();
            if(old.size()!=bindings["layers"].toArray().size())bad("Invalid layer face order count");
            QHash<quint64,int> layers;QJsonArray rows;
            for(size_t f=0;f<n.faces.size();++f)if(faceLive[f])layers.insert(ids[2][f],n.faces[f].layer);
            for(int l=0;l<old.size();++l){QJsonArray row;QSet<quint64> seen;
                for(auto value:old[l].toArray()){auto id=integer(value);if(layers.value(id,-1)==l&&!seen.contains(id)){row.append(double(id));seen.insert(id);}}
                for(size_t f=0;f<n.faces.size();++f)if(faceLive[f]&&n.faces[f].layer==l&&!seen.contains(ids[2][f]))row.append(double(ids[2][f]));
                rows.append(row);}
            bindings["layer_faces"]=rows;
        }
        return {source.withControlNet(out,geometry),bindings,true};
    }
};

using Triangle=std::array<fs::native_geometry::Vec3,3>;
bool inTriangle(fs::native_geometry::Vec3 p,const Triangle& t){
    using fs::native_geometry::Vec3;
    auto sameSide=[](Vec3 p1,Vec3 p2,Vec3 a,Vec3 b){
        const Vec3 edge{b.x-a.x,b.y-a.y,b.z-a.z};
        auto cross=[&](Vec3 p){p={p.x-a.x,p.y-a.y,p.z-a.z};return Vec3{p.y*edge.z-p.z*edge.y,-(p.x*edge.z-p.z*edge.x),p.x*edge.y-p.y*edge.x};};
        const auto x=cross(p1),y=cross(p2);double dot=x.x*y.x+x.y*y.y+x.z*y.z;if(std::abs(dot)<1e-5)dot=0;return dot>=0;
    };
    return sameSide(p,t[0],t[1],t[2])&&sameSide(p,t[1],t[0],t[2])&&sameSide(p,t[2],t[0],t[1]);
}

void intersectLayers(Net& net,const Fbm& source,int target,int other){
    if(target==other)bad("Choose two distinct layers");bool targetPresent=false,otherPresent=false;
    for(int f=0;f<int(net.n.faces.size());++f){if(net.n.faces[f].layer==target){targetPresent=true;net.editableFace(f);}if(net.n.faces[f].layer==other)otherPresent=true;}
    if(!targetPresent||!otherPresent)bad("Intersection layers must contain faces");
    auto input=documentGeometry(source);fs::native_geometry::SubdivisionSurface surface(input);const auto&mesh=surface.level(input.desiredLevel);
    struct Cut {Triangle triangle;fs::native_geometry::Plane plane;};std::vector<Cut> cuts;
    for(const auto& face:mesh.faces)if(face.layer==std::size_t(other))for(std::size_t i=2;i<face.points.size();++i){
        Triangle t{mesh.points[face.points[0]].position,mesh.points[face.points[i-1]].position,mesh.points[face.points[i]].position};
        auto plane=fs::native_geometry::Plane::fromPoints(t[0],t[1],t[2]);if(plane.valid())cuts.push_back({t,plane});}
    std::vector<int> points;
    for(bool restart=true;restart;){restart=false;
        std::vector<int> edges;for(int e=0;e<int(net.n.edges.size());++e)if(net.edgeLive[e])
            for(int f:net.edgeFaces[e])if(net.faceLive[f]&&net.n.faces[f].layer==target){edges.push_back(e);break;}
        for(int e:edges){const auto edge=net.n.edges[e];const auto a=position(net.n.points[edge.a].value),b=position(net.n.points[edge.b].value);
            for(const auto&cut:cuts){float s1=cut.plane.evaluate({a.x,a.y,a.z}),s2=cut.plane.evaluate({b.x,b.y,b.z});
                if(!((s1<0&&s2>0)||(s1>0&&s2<0)))continue;const float t=-s1/(s2-s1);
                FbmPosition p{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)};
                if(!inTriangle({p.x,p.y,p.z},cut.triangle))continue;
                int created=net.splitAt(e,p);if(created<0)continue;
                auto& value=net.n.points[created];value.value.selected=true;value.name=QByteArray("np")+QByteArray::number(points.size());points.push_back(created);restart=true;break;
            }if(restart)break;
        }
        if(points.size()>100000)bad("Layer intersection exceeds the point budget");
    }
    // Original pairwise Sort3DPoints, including its equal-coordinate behavior.
    for(std::size_t i=0;i<points.size();++i)for(std::size_t j=i+1;j<points.size();++j){const auto&a=net.n.points[points[i]].value,&b=net.n.points[points[j]].value;
        if(a.x>b.x||(a.x==b.x&&(a.y>b.y||(a.y==b.y&&a.z>b.z))))std::swap(points[i],points[j]);}
    net.connectIntersections(points,true);
}
}

bool nativeTopologyCommand(const QJsonObject&r){
    if(r["method"]=="topology")return true;
    if(r["method"]=="advanced")return r["operation"]=="extrude"||r["operation"]=="mirror";
    static const QSet<QString> operations{"split_edges","connect_points","flip_faces","remove_unused","delete_negative","collapse_points","collapse_edges","extrude_points","anchor","linear_constraint","clear_constraints","align","coincide","unlock_all","grid_panel","cylinder","insert_plane","intersect_layers"};
    return r["method"]=="modeling"&&operations.contains(r["operation"].toString());
}

TopologyEdit constructHullNet(const Fbm& source,const QJsonObject& bindings,
                             const std::vector<FbmPosition>& points,int columns,int rows){
    Net net(source,bindings);
    if(!net.n.points.empty()||columns<2||rows<2||points.size()!=size_t(columns*rows))bad("Invalid new hull grid");
    for(auto p:points){check(p);net.addPoint(at(p));}
    for(int i=1;i<rows;++i)for(int j=1;j<columns;++j)
        net.addFace({i*columns+j-1,i*columns+j,(i-1)*columns+j,(i-1)*columns+j-1},0,true);
    for(int e=0;e<int(net.n.edges.size());++e)net.crease(e,false);
    for(int e=0;e<int(net.n.edges.size());++e)if(net.faceCount(e)!=2)net.crease(e,true);
    for(int p=0;p<int(net.n.points.size());++p)if(net.pointFaceCount(p)<2)net.n.points[p].value.vertexType=3;
    if(net.n.points[columns-1].value.vertexType==3)net.n.points[columns-1].value.vertexType=1;
    net.changed=true;return net.finish(source);
}
TopologyEdit editTopology(const Fbm& source,const QJsonObject& bindings,const QJsonObject& r){
    static const QSet<QString> basic{"add_point","split_edge","set_crease","set_locked","add_face","assign_layer","delete"};
    if(!nativeTopologyCommand(r)||(r["method"]=="topology"&&!basic.contains(r["operation"].toString())))bad("Unsupported native topology operation");
    Net net(source,bindings);auto& n=net.n;const auto op=r["operation"].toString();
    if(r["method"]=="advanced"&&r.contains("pivot"))vector(r,"pivot");
    if(op=="grid_panel"||op=="cylinder"){
        int layer=net.layer(r["layer_id"]);net.importGrid(constructionGrid(r),layer);
    }else if(op=="insert_plane"){
        const int axis=integer(r["axis"],0);if(axis>2)bad("Invalid plane axis");auto origin=vector(r,"position");bool curves=flag(r,"create_curve");
        const float distance=axis==0?origin.x:axis==1?origin.y:origin.z;std::vector<int> points;const int count=n.edges.size();
        for(int e=0;e<count;++e){auto edge=n.edges[e];if(!net.visibleEdge(e))continue;
            const auto a=position(n.points[edge.a].value),b=position(n.points[edge.b].value);float s1=(axis==0?a.x:axis==1?a.y:a.z)-distance,s2=(axis==0?b.x:axis==1?b.y:b.z)-distance;
            if(s1*s2<0){net.editable(edge.a);net.editable(edge.b);for(int f:net.edgeFaces[e])if(net.faceLive[f])net.editableFace(f);}
            if(!((s1<-1e-5&&s2>1e-5)||(s1>1e-5&&s2<-1e-5)))continue;
            const float t=-s1/(s2-s1);int p=net.splitAt(e,{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)});if(p>=0)points.push_back(p);
        }
        // Plus sorts raw addresses here. Use coordinates and creation index so
        // the same cut has one reproducible face/curve order across restarts.
        std::sort(points.begin(),points.end(),[&](int a,int b){const auto&p=n.points[a].value,&q=n.points[b].value;
            if(p.x!=q.x)return p.x<q.x;if(p.y!=q.y)return p.y<q.y;if(p.z!=q.z)return p.z<q.z;return a<b;});
        auto edges=net.connectIntersections(points,false);if(curves)net.curvesFromEdges(std::move(edges));
    }else if(op=="intersect_layers"){
        intersectLayers(net,source,net.layer(r["layer_id"]),net.layer(r["other_layer_id"]));
    }else if(op=="anchor"||op=="linear_constraint"||op=="align"||op=="coincide"){
        auto selection=net.selection(0,r);if(selection.size()<2)bad("Select at least two ordered points");int a=selection.front(),b=selection.back();bool line=op=="align"||op=="linear_constraint";size_t end=selection.size()-(line?1:0);
        auto first=position(n.points[a].value),last=position(n.points[b].value);
        if(line){if(selection.size()<3)bad("Select endpoints and at least one intermediate point");double x=double(first.x)-last.x,y=double(first.y)-last.y,z=double(first.z)-last.z;if(x*x+y*y+z*z<=1e-12)bad("Line endpoints must be distinct");}
        for(size_t j=1;j<end;++j){const auto&p=n.points[selection[j]].value;if(p.locked||p.hardAnchor)bad("Point is locked or hard anchored");if(op=="align"||op=="coincide")net.editable(selection[j]);}
        if(op=="anchor"||op=="linear_constraint"){
            std::vector<int> stack{a};if(line)stack.push_back(b);std::vector<bool> seen(n.points.size());
            while(!stack.empty()){int i=stack.back();stack.pop_back();if(i<0||seen[i])continue;seen[i]=true;auto&p=n.points[i].value;for(int parent:{p.anchor,p.lineA,p.lineB})if(parent>=0)stack.push_back(parent);}
            for(size_t j=1;j<end;++j){int i=selection[j];const auto&p=n.points[i].value;if(seen[i])bad("Constraint would create a dependency cycle");if(op=="anchor"&&p.lineA>=0)bad("Remove the linear constraint before anchoring");if(line&&p.anchor>=0)bad("Remove the anchor before constraining to a line");}
        }
        std::vector<FbmControlPoint> points;for(const auto&p:n.points)points.push_back(p.value);PointEditor editor(points,net.bindings);bool hard=op=="anchor"?flag(r,"hard"):false;
        for(size_t j=1;j<end;++j){int i=selection[j];auto&p=points[i];
            if(op=="anchor"){if(p.anchor!=a||p.hardAnchor!=hard){editor.anchor(i,a,hard);net.changed=true;}}
            else if(op=="linear_constraint"){if(p.lineA!=a||p.lineB!=b){editor.line(i,a,b);net.changed=true;}net.changed=editor.move(i,projectPointOnLine(position(p),first,last))||net.changed;}
            else net.changed=editor.move(i,line?projectPointOnLine(position(p),first,last):position(points[a]))||net.changed;
        }
        for(size_t i=0;i<points.size();++i)n.points[i].value=points[i];
        editor.writeOrder(net.bindings);
    }else if(op=="clear_constraints"||op=="unlock_all"){
        std::vector<int> points;if(op=="unlock_all"){for(int i=0;i<int(n.points.size());++i)points.push_back(i);}else points=net.selection(0,r);
        if(op=="clear_constraints")for(int i:points)if(n.points[i].value.locked)bad("Unlock points before removing their constraints");
        for(int i:points){auto&p=n.points[i].value;if(op=="unlock_all"){if(p.locked){p.locked=false;net.changed=true;}}
            else if(p.anchor>=0||p.hardAnchor||p.lineA>=0||p.lineB>=0){p.anchor=p.lineA=p.lineB=-1;p.hardAnchor=false;net.changed=true;}}
    }else if(op=="extrude_points"){
        auto selected=net.selection(0,r);auto d=vector(r,"position");if(d.x==0&&d.y==0&&d.z==0)return net.finish(source);
        for(int i:selected){net.editable(i);auto&p=n.points[i].value;for(double v:{double(p.x)+d.x,double(p.y)+d.y,double(p.z)+d.z})if(!std::isfinite(v)||std::abs(v)>1e9)bad("Invalid extruded coordinate");}
        for(int i:selected)net.weldPoint(offset(n.points[i].value,d));net.changed=true;
    }else if(op=="extrude"){
        if(r["kind"]!="edge")bad("Extrusion requires edges");auto edges=net.selection(1,r);int layer=net.layer(r["layer_id"]);auto d=vector(r,"vector");
        std::vector<int> points,adjacent;for(int e:edges){auto value=n.edges[e];net.editable(value.a);net.editable(value.b);if(!contains(points,value.a))points.push_back(value.a);if(!contains(points,value.b))points.push_back(value.b);}
        if(d.x==0&&d.y==0&&d.z==0)return net.finish(source);
        for(int e:edges){auto value=n.edges[e];if(net.faceCount(e)>1)bad("Extrusion requires boundary or free edges");int f=-1;for(int i:net.edgeFaces[e])if(net.faceLive[i])f=i;adjacent.push_back(f);
            auto a=n.points[value.a].value,b=n.points[value.b].value;if(double(a.y)+d.y<0||double(b.y)+d.y<0)bad("Extrusion crosses the centerline");auto c=at(offset(a,d)),target=at(offset(b,d));
            if(coincident(a,c)||coincident(b,target)||((double(b.y)-a.y)*(c.z-a.z)-(double(b.z)-a.z)*(c.y-a.y)==0&&(double(b.z)-a.z)*(c.x-a.x)-(double(b.x)-a.x)*(c.z-a.z)==0&&(double(b.x)-a.x)*(c.y-a.y)-(double(b.y)-a.y)*(c.x-a.x)==0))bad("Extrusion creates a degenerate face");
        }
        int oldPoints=n.points.size(),oldEdges=n.edges.size();QHash<int,int> targets;for(int p:points){auto created=at(offset(n.points[p].value,d));created.vertexType=n.points[p].value.vertexType;targets.insert(p,net.addPoint(created));}
        std::vector<int> newFaces;for(int e:edges){auto value=n.edges[e];int a=targets[value.a],b=targets[value.b];newFaces.push_back(net.addFace({value.b,value.a,a,b},layer,false));net.crease(net.edge(value.a,a),false);net.crease(net.edge(value.b,b),false);}
        for(int e=oldEdges;e<int(n.edges.size());++e)if(net.faceCount(e)!=2)net.crease(e,true);
        for(int p=oldPoints;p<int(n.points.size());++p)if(net.pointFaceCount(p)<2)n.points[p].value.vertexType=3;
        for(size_t j=0;j<edges.size();++j)if(adjacent[j]>=0){auto value=n.edges[edges[j]];if(net.direction(newFaces[j],value.a,value.b)==net.direction(adjacent[j],value.a,value.b))std::reverse(n.faces[newFaces[j]].points.begin(),n.faces[newFaces[j]].points.end());}
        net.changed=true;
    }else if(op=="mirror"){
        if(r["kind"]!="face")bad("Mirror requires faces");auto faces=net.selection(2,r);int axis=integer(r["axis"],0);if(axis>2)bad("Invalid mirror axis");
        auto value=r["distance"].toDouble();if(!r["distance"].isDouble()||!std::isfinite(value)||std::abs(value)>1e9)bad("Invalid mirror distance");float distance=float(value);bool weld=flag(r,"weld");std::vector<int> sourceFaces,points;
        for(int f:faces){net.editableFace(f);bool inPlane=true;for(int p:n.faces[f].points){auto v=position(n.points[p].value);if((axis==0?v.x:axis==1?v.y:v.z)-distance!=0)inPlane=false;}if(!inPlane){sourceFaces.push_back(f);for(int p:n.faces[f].points)if(!contains(points,p))points.push_back(p);}}
        if(sourceFaces.empty())return net.finish(source);std::sort(points.begin(),points.end(),[&](int a,int b){return net.ids[0][a]<net.ids[0][b];});QHash<int,int> targets;
        for(int p:points){auto v=position(n.points[p].value);float x=axis==0?1:0,y=axis==1?1:0,z=axis==2?1:0;float r=x*v.x+y*v.y+z*v.z-distance;
            FbmPosition projected{v.x-x*r,v.y-y*r,v.z-z*r};FbmPosition reflected{v.x+2*(projected.x-v.x),v.y+2*(projected.y-v.y),v.z+2*(projected.z-v.z)};check(reflected);
            int target=weld?net.weldPoint(reflected):net.addPoint(at(reflected));if(n.points[target].sourceIndex>=0)net.editable(target);targets.insert(p,target);}
        for(int f:sourceFaces){auto original=n.faces[f];std::vector<int> reversed;for(auto it=original.points.rbegin();it!=original.points.rend();++it){int target=targets[*it];if(contains(reversed,target))bad("Welded mirrored face is degenerate");reversed.push_back(target);}
            for(size_t j=0;j<reversed.size();++j){int e=net.edge(reversed[j],reversed[(j+1)%reversed.size()]);if(e>=0&&net.faceCount(e)>=2)bad("Mirror would create a nonmanifold edge");}net.addFace(reversed,original.layer,false);}
        for(int f:sourceFaces){auto face=n.faces[f];for(size_t j=0;j<face.points.size();++j){int a=face.points[j],b=face.points[(j+1)%face.points.size()];int e=net.edge(a,b),copy=net.edge(targets[a],targets[b]);if(e>=0&&copy>=0&&e!=copy)net.crease(copy,n.edges[e].crease);}}
        for(int p:points){int copy=targets[p];if(p!=copy){if(n.points[p].value.vertexType==3)n.points[copy].value.vertexType=3;n.points[copy].value.locked=n.points[p].value.locked;}}net.changed=true;
    }else if(op=="collapse_edges"){
        auto edges=net.selection(1,r);for(int e:edges)net.collapseEdge(e);
    }else if(op=="add_point"){
        if(!r["position"].isArray()||r["position"].toArray().size()!=3)bad("position must have three coordinates");auto a=r["position"].toArray();float v[3];
        for(int i=0;i<3;++i){double d=a[i].toDouble();if(!a[i].isDouble()||!std::isfinite(d)||std::abs(d)>1e9)bad("Invalid coordinate");v[i]=float(d);}FbmControlPoint p;p.x=v[0];p.y=v[1];p.z=v[2];net.addPoint(p);net.changed=true;
    }else if(op=="split_edge"||op=="split_edges"){
        auto edges=op=="split_edge"?std::vector<int>{net.resolve(1,r["edge_id"])}:net.selection(1,r);for(int e:edges){net.editable(n.edges[e].a);net.editable(n.edges[e].b);}
        for(int e:edges)net.split(e);
    }else if(op=="set_crease"){
        auto edges=net.selection(1,r);bool value=flag(r,"crease");for(int e:edges){net.editable(n.edges[e].a);net.editable(n.edges[e].b);if(!value&&net.faceCount(e)<2)bad("Boundary edges must remain creases");}
        for(int e:edges)if(n.edges[e].crease!=value){net.crease(e,value);net.changed=true;}
    }else if(op=="set_locked"){
        auto points=net.selection(0,r);bool value=flag(r,"locked");net.geometry=false;for(int p:points)if(n.points[p].value.locked!=value){n.points[p].value.locked=value;net.changed=true;}
    }else if(op=="add_face"){
        auto points=net.selection(0,r);if(points.size()<3||points.size()>4096)bad("A face requires 3..4096 ordered points");int layer=net.layer(r["layer_id"]);
        for(size_t i=0;i<points.size();++i){int a=points[i],b=points[(i+1)%points.size()];net.editable(a);if(coincident(n.points[a].value,n.points[b].value))bad("Face has coincident adjacent points");int e=net.edge(a,b);if(e>=0&&net.faceCount(e)>=2)bad("Face would create a nonmanifold edge");}
        Projected p(points,n);for(int i=0;i<int(points.size());++i)for(int j=i+1;j<int(points.size());++j)if(j!=i+1&&!(i==0&&j==int(points.size())-1)&&p.intersects(i,(i+1)%points.size(),j,(j+1)%points.size()))bad("Face has crossing edges");
        net.addFace(points,layer,true);net.changed=true;
    }else if(op=="assign_layer"||op=="flip_faces"){
        auto faces=net.selection(2,r);int layer=op=="assign_layer"?net.layer(r["layer_id"]):-1;for(int f:faces)net.editableFace(f);
        for(int f:faces)if(op=="flip_faces"){std::reverse(n.faces[f].points.begin(),n.faces[f].points.end());net.changed=true;}else if(n.faces[f].layer!=layer){n.faces[f].layer=layer;net.changed=true;}
    }else if(op=="delete"){
        auto kind=r["kind"].toString();int k=kind=="point"?0:kind=="edge"?1:kind=="face"?2:-1;if(k<0)bad("Invalid selection kind");auto items=net.selection(k,r);
        for(int i:items){if(k==0){net.editable(i);for(int f:net.pointFaces[i])net.editableFace(f);for(int e:net.pointEdges[i]){net.editable(n.edges[e].a);net.editable(n.edges[e].b);}}
            else if(k==1){net.editable(n.edges[i].a);net.editable(n.edges[i].b);for(int f:net.edgeFaces[i])net.editableFace(f);}else net.editableFace(i);}
        for(int i:items){if(k==0)net.deletePoint(i);else if(k==1)net.deleteEdge(i);else net.deleteFace(i);}net.changed=true;
    }else if(op=="connect_points"){
        auto points=net.selection(0,r);if(points.size()<2)bad("Select at least two ordered points");for(int p:points)net.editable(p);
        for(size_t i=1;i<points.size();++i){int a=points[i-1],b=points[i];if(net.edge(a,b)>=0)continue;if(coincident(n.points[a].value,n.points[b].value))bad("Cannot connect coincident points");
            if(net.pointFaceCount(a)==0&&net.pointFaceCount(b)==0){int e=net.addEdge(a,b);net.crease(e,true);}
            else{int f=-1;for(int candidate:net.pointFaces[a])if(net.faceLive[candidate]&&contains(n.faces[candidate].points,b)){if(f>=0)bad("Ambiguous shared face");f=candidate;}if(f<0)bad("Points must share one face");net.editableFace(f);
                const auto original=n.faces[f];const auto& v=original.points;int ia=std::find(v.begin(),v.end(),a)-v.begin(),ib=std::find(v.begin(),v.end(),b)-v.begin();Projected(v,n).diagonal(ia,ib);
                auto path=[&](int start,int end){std::vector<int> p{v[start]};for(int j=(start+1)%v.size();;j=(j+1)%v.size()){p.push_back(v[j]);if(j==end)break;}return p;};
                net.addFace(path(ia,ib),original.layer,false);net.addFace(path(ib,ia),original.layer,false);net.deleteFace(f);net.crease(net.edge(a,b),false);
            }net.changed=true;net.allocate();
        }
    }else if(op=="remove_unused"||op=="delete_negative"){
        if(op=="delete_negative"){
            std::vector<int> faces;for(int f=0;f<int(n.faces.size());++f){bool negative=true;for(int p:n.faces[f].points)if(n.points[p].value.y>1e-7)negative=false;if(negative){net.editableFace(f);faces.push_back(f);}}
            for(auto it=faces.rbegin();it!=faces.rend();++it){net.deleteFace(*it);net.changed=true;}
        }
        std::vector<int> points;for(int p=0;p<int(n.points.size());++p)if(net.pointLive[p]&&net.pointFaceCount(p)==0&&(op=="remove_unused"||n.points[p].value.y<-1e-4)){net.editable(p);points.push_back(p);}
        if(op=="delete_negative")std::reverse(points.begin(),points.end());
        for(int p:points){net.deletePoint(p);net.changed=true;}
    }else if(op=="collapse_points"){
        auto points=net.selection(0,r);for(int p:points){net.collapsePoint(p,true);net.allocate();}
    }else bad("Unsupported native topology operation");
    return net.finish(source);
}
}
