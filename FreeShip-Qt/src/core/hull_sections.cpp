// SPDX-License-Identifier: GPL-2.0-or-later
// Native displacement-hull sections, shared by specialist format writers.
// All coordinates remain in source units and at the original model origin.
#include "hull_sections.h"
#include "native/sections.h"
#include <QJsonArray>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <set>
#include <limits>

namespace fs::hull {
namespace {
namespace ng=fs::native_geometry;
[[noreturn]] void bad(const QString& s){throw std::runtime_error(s.toStdString());}
QString n(double v){if(!std::isfinite(v))bad("船体数据包含无效数值。");return QString::number(v==0?0:v,'g',17);}
bool same(ng::Vec3 a,ng::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void reverse(ng::SectionCurve& c){std::reverse(c.points.begin(),c.points.end());std::reverse(c.knuckles.begin(),c.knuckles.end());}
// Only identical endpoints join here. In particular, do not inherit the
// general section display's 0.1-unit near-neighbour joins or tiny-loop removal.
std::vector<ng::SectionCurve> connected(std::vector<ng::SectionCurve> curves){
    for(auto& c:curves)for(size_t i=1;i<c.points.size();)if(same(c.points[i-1],c.points[i])){
        c.knuckles[i-1]=c.knuckles[i-1]||c.knuckles[i];c.points.erase(c.points.begin()+i);c.knuckles.erase(c.knuckles.begin()+i);
    }else ++i;
    curves.erase(std::remove_if(curves.begin(),curves.end(),[](const auto& c){return c.points.size()<2;}),curves.end());
    bool changed=true;
    while(changed){changed=false;
        for(size_t i=0;i<curves.size()&&!changed;++i)for(size_t j=i+1;j<curves.size();++j){auto& a=curves[i];auto& b=curves[j];
            bool ff=same(a.points.front(),b.points.front()),fb=same(a.points.front(),b.points.back());
            bool bf=same(a.points.back(),b.points.front()),bb=same(a.points.back(),b.points.back());
            if(!(ff||fb||bf||bb))continue;
            if(!bf){if(bb)reverse(b);else{reverse(a);if(ff){}else reverse(b);}}
            a.knuckles.back()=a.knuckles.back()||b.knuckles.front();
            a.points.insert(a.points.end(),b.points.begin()+1,b.points.end());a.knuckles.insert(a.knuckles.end(),b.knuckles.begin()+1,b.knuckles.end());
            curves.erase(curves.begin()+j);changed=true;break;
        }
    }return curves;
}
using P=Point;
P mid(P a,P b){return {(a.y+b.y)/2,(a.z+b.z)/2};}
double distance(P p,P a,P b){double y=b.y-a.y,z=b.z-a.z,l=y*y+z*z;double t=l?std::clamp(((p.y-a.y)*y+(p.z-a.z)*z)/l,0.,1.):0;return std::hypot(p.y-a.y-t*y,p.z-a.z-t*z);}
void append(std::vector<P>& points,P p){if(!points.empty()&&points.back().y==p.y&&points.back().z==p.z)return;if(points.size()>=100000)bad("单个断面的点数超过安全上限，未简化或截断数据。");points.push_back(p);}
void flatten(std::vector<P>& out,P a,P b,P c,P d,double tolerance,unsigned depth=0){
    if(std::max(distance(b,a,d),distance(c,a,d))<=tolerance){append(out,d);return;}
    if(depth==30)bad("断面曲线无法在指定误差内离散。");
    auto ab=mid(a,b),bc=mid(b,c),cd=mid(c,d),abc=mid(ab,bc),bcd=mid(bc,cd),center=mid(abc,bcd);
    flatten(out,a,ab,abc,center,tolerance,depth+1);flatten(out,center,bcd,cd,d,tolerance,depth+1);
}
std::vector<P> sample(const ng::SectionCurve& c,double tolerance){
    ng::Spline spline(c.points,c.knuckles);auto& t=spline.parameters();auto& d=spline.secondDerivatives();std::vector<P> out;
    append(out,{c.points.front().y,c.points.front().z});
    for(size_t i=1;i<c.points.size();++i){auto a=c.points[i-1],b=c.points[i];float h=t[i]-t[i-1];
        if(h<1e-6f)bad("断面含有过短的样条参数段，请检查重复或极近的型值点。");
        const double h2=float(h*h/6.f);
        auto control=[&](double x,double y,double d0,double d1){if(c.knuckles[i-1]&&c.knuckles[i])d0=d1=0;return std::pair{x+(y-x-h2*(2*d0+d1))/3,y-(y-x+h2*(d0+2*d1))/3};};
        auto y=control(a.y,b.y,d[i-1].y,d[i].y),z=control(a.z,b.z,d[i-1].z,d[i].z);
        flatten(out,{a.y,a.z},{y.first,z.first},{y.second,z.second},{b.y,b.z},tolerance);
    }return out;
}
double cross(P a,P b,P c){return (b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y);}
bool intersects(P a,P b,P c,P d){
    if(std::max(a.y,b.y)<std::min(c.y,d.y)||std::max(c.y,d.y)<std::min(a.y,b.y)||std::max(a.z,b.z)<std::min(c.z,d.z)||std::max(c.z,d.z)<std::min(a.z,b.z))return false;
    auto ab0=cross(a,b,c),ab1=cross(a,b,d),cd0=cross(c,d,a),cd1=cross(c,d,b);
    return ((ab0<=0&&ab1>=0)||(ab0>=0&&ab1<=0))&&((cd0<=0&&cd1>=0)||(cd0>=0&&cd1<=0));
}
double area(const std::vector<P>& p){double a=0;for(size_t i=0;i<p.size();++i){auto x=p[i],y=p[(i+1)%p.size()];a+=x.y*y.z-y.y*x.z;}return a/2;}
void validatePolygon(const std::vector<P>& p){
    if(p.size()<3||area(p)<=0)bad("断面面积为零，或不能形成逆时针的半船轮廓。");
    // Sweep bounding boxes before exact orientation tests. Adjacent edges may
    // share their endpoint; all other intersections are rejected.
    struct Edge {P a,b;size_t id;double lo,hi;};std::vector<Edge> edges;
    for(size_t i=0;i<p.size();++i){auto a=p[i],b=p[(i+1)%p.size()];edges.push_back({a,b,i,std::min(a.y,b.y),std::max(a.y,b.y)});}
    std::sort(edges.begin(),edges.end(),[](auto a,auto b){return a.lo<b.lo;});quint64 checks=0;
    for(size_t i=0;i<edges.size();++i)for(size_t j=i+1;j<edges.size()&&edges[j].lo<=edges[i].hi;++j){
        auto& a=edges[i];auto& b=edges[j];if((a.id+1)%p.size()==b.id||(b.id+1)%p.size()==a.id)continue;
        if(++checks>20000000)bad("断面过于复杂，无法在安全预算内完成自交检查。");
        if(intersects(a.a,a.b,b.a,b.b))bad("断面存在自交或重叠边，不能作为 排水体轮廓。");
    }
}
}
std::vector<std::array<double,2>> sampleSection(const ng::SectionCurve& c,double tolerance){
    if(!std::isfinite(tolerance)||tolerance<=0||c.points.size()<2)bad("断面离散参数无效。");
    std::vector<std::array<double,2>> out;for(auto p:sample(c,tolerance))out.push_back({p.y,p.z});return out;
}
Sections build(const ng::DocumentInput& input,const ModelSnapshot& m,bool closeDeck){
    if(input.documentId!=m.documentId||input.revision!=m.revision||input.imperial!=(m.units=="imperial"))bad("船体几何与当前模型版本或单位不一致。");
    if(!m.particularsSet||m.length<=0||m.beam<=0)bad("请先在工程设置中填写船长和船宽。");
    const double meters=input.imperial?.3048:1.;
    const double density=input.imperial?m.waterDensity/(1000*.3048*.3048*.3048/.45359237):m.waterDensity;
    if(!std::isfinite(density)||density<=0)bad("工程水密度无效。");
    std::set<size_t> used;for(const auto& f:input.geometry.faces)used.insert(f.layer);bool hydro=false,positive=false,negative=false;
    for(auto l:used){if(l>=input.geometry.layers.size())bad("断面图层不存在。");const auto& layer=input.geometry.layers[l];if(!layer.useInHydrostatics)continue;
        hydro=true;if(!layer.symmetric)bad("当前导出入口要求参与静水力计算的图层为对称半船。非对称和多船体需分别定义部件。");
    }
    if(!hydro)bad("没有参与静水力计算的曲面。");
    for(const auto& f:input.geometry.faces)if(input.geometry.layers[f.layer].useInHydrostatics)for(auto id:f.points){
        if(id>=input.geometry.points.size())bad("断面控制点不存在。");auto y=input.geometry.points[id].position.y;positive|=y>0;negative|=y<0;
    }
    if(positive&&negative)bad("参与静水力的控制网跨越中心线两侧，不能再按对称半船导出。");
    ng::SubdivisionSurface surface(input.geometry);const auto& mesh=surface.level(input.geometry.desiredLevel);ng::SectionIndex index(surface.input(),mesh);
    std::vector<Station> stations;QJsonArray empty,closed,lobes;size_t total=0;std::set<float> seen;
    for(const auto& section:input.sections)if(section.kind==1){
        auto p=section.plane.coefficients();if(p[0]!=1||p[1]!=0||p[2]!=0)bad("横剖站的剖切平面无效。");float x=-p[3];
        if(!seen.insert(x).second)bad("存在重复横剖站，请先整理剖面线。");
        auto cuts=connected(index.intersect(section.plane,{true}).connectedCurves);
        if(cuts.empty()){empty.append(double(x));continue;}
        try{
            for(auto& c:cuts){
                if(negative)for(auto& p:c.points)p.y=-p.y;
                if(same(c.points.front(),c.points.back())){
                    c.knuckles.front()=c.knuckles.front()||c.knuckles.back();c.points.pop_back();c.knuckles.pop_back();
                    std::vector<P> polygon;for(auto p:c.points)polygon.push_back({p.y,p.z});
                    if(area(polygon)<0)reverse(c);
                    size_t start=c.points.size();
                    for(size_t i=0;i<c.points.size();++i)if(c.points[i].y==0&&(start==c.points.size()||c.points[i].z<c.points[start].z))start=i;
                    if(start==c.points.size())bad("闭合轮廓不接触中心线，需要使用独立部件。");
                    std::rotate(c.points.begin(),c.points.begin()+start,c.points.end());std::rotate(c.knuckles.begin(),c.knuckles.begin()+start,c.knuckles.end());
                    c.points.push_back(c.points.front());c.knuckles.push_back(c.knuckles.front());
                }else if(c.points.front().z>c.points.back().z)reverse(c);
            }
            std::sort(cuts.begin(),cuts.end(),[](const auto& a,const auto& b){return a.points.front().z<b.points.front().z;});
            std::vector<P> points;double previousMax=-std::numeric_limits<double>::infinity();
            for(size_t ci=0;ci<cuts.size();++ci){auto& c=cuts[ci];
                if(c.points.front().y!=0)bad("轮廓底部端点不在中心线上，不能自动补齐横向缺口。");
                const bool open=c.points.back().y!=0;
                if(open&&ci+1!=cuts.size())bad("下部独立轮廓未收回中心线，不能作为零面积连接。");
                if(open&&!closeDeck)bad("甲板端点未到中心线；如确需水平封顶，请勾选封闭甲板选项。");
                std::vector<P> contour;for(auto p:sampleSection(c,.0001/meters))contour.push_back({p[0],p[1]});double low=contour.front().z,high=low;
                if(contour.front().y==contour.back().y&&contour.front().z==contour.back().z)contour.pop_back();
                for(auto p:contour){if(p.y<0)bad("断面样条越过中心线，请先修整曲线。");low=std::min(low,p.z);high=std::max(high,p.z);}
                if(low<previousMax)bad("独立轮廓的高度范围重叠，需要分别定义 独立部件。");previousMax=high;
                if(open){append(contour,{0,contour.back().z});closed.append(double(x));}
                validatePolygon(contour);
                // A closed half-section may include a centerplane face. Keep
                // its ordinates and require a CL endpoint for the next lobe's
                // zero-area connector.
                if(ci+1<cuts.size()&&contour.back().y!=0)bad("下部轮廓没有可用的中心线连接端点。");
                // A centerline connector and its implicit return have zero
                // area and first/second moments. Never connect across breadth.
                for(auto p:contour)append(points,p);
            }
            if(cuts.size()>1)lobes.append(QJsonObject{{"x",double(x)},{"contours",int(cuts.size())}});
            total+=points.size();if(total>2000000)bad("船体总点数超过安全上限。");
            stations.push_back({double(x),std::move(points)});
        }catch(const std::exception& e){bad("横剖站 X="+n(x)+"："+QString::fromUtf8(e.what()));}
    }
    if(stations.size()<2)bad("至少需要两个与船体相交的有效横剖站，请先设置剖面线。");
    std::sort(stations.begin(),stations.end(),[](const auto& a,const auto& b){return a.x<b.x;});
    QJsonObject diagnostics{{"stations",int(stations.size())},{"points",int(total)},{"empty_stations",empty},{"closed_decks",closed},{"centerline_lobes",lobes},{"chord_tolerance_m",.0001},{"source_side",negative?"port":"starboard"}};
    return {std::move(stations),meters,density,diagnostics};
}
}
