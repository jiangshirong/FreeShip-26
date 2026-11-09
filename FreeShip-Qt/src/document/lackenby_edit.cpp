// GPL-2.0-or-later. Native adaptation of bridge/LackenbyEngine.pas,
// ModelTools.inc, FreeShip.SubmergedHullExtents/FindLowestHydrostaticsPoint,
// FreeIntersection.CalculateArea/CreateStarboardPart and FreeSpline.InsertSpline.
// Original FreeShip: Martijn van Engeland (2005); Plus: Timoshenko V.F.
// (2006-2011), Mark Malakanov (2015). Keep Single/Double expression order.
#include "lackenby_edit.h"
#include "geometry_input.h"
#include "native/sections.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <initializer_list>
#ifdef FREESHIP_LACKENBY_TRACE
#include <cstdio>
#endif

namespace fs::document {
namespace {
using namespace native_geometry;
void trace(const char *kind,std::initializer_list<double> values) {
#ifdef FREESHIP_LACKENBY_TRACE
    std::fprintf(stderr,"%s",kind);for (double value:values) std::fprintf(stderr," %.17g",value);std::fprintf(stderr,"\n");
#else
    (void)kind;(void)values;
#endif
}
[[noreturn]] void bad(const char *message) { throw std::invalid_argument(message); }
float finite(float value) { if (!std::isfinite(value)) bad("Nonfinite Lackenby calculation"); return value; }
double number(QJsonValue value) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || std::abs(n)>1e9) bad("Invalid Lackenby parameter");
    return n;
}
int integer(QJsonValue value,int minimum=1,int maximum=2000000) {
    const double n=number(value);if (n<minimum || n>maximum || std::floor(n)!=n) bad("Invalid Lackenby integer");return int(n);
}
float distanceSquared(Vec3 a,Vec3 b) { const float x=b.x-a.x,y=b.y-a.y,z=b.z-a.z;return x*x+y*y+z*z; }
struct Bounds {
    Vec3 lo{},hi{};bool empty=true;
    void add(Vec3 p) {
        if (empty) {lo=hi=p;empty=false;return;}
        lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
};

// The general native join deliberately repairs the old prepend-knuckle loss.
// This numerical compatibility path preserves that old behavior: changing the
// shared knuckle changes the area spline and therefore the Lackenby iteration.
void joinLegacy(std::vector<SectionCurve> &curves,float error) {
    const auto closed=[](const SectionCurve &c) {return c.points.size()>1 && std::sqrt(distanceSquared(c.points.front(),c.points.back()))<1e-5;};
    size_t at=0;
    while (at+1<curves.size()) {
        const auto &a=curves[at];if (a.points.size()<2 || closed(a)) {++at;continue;}
        float best=1e30f;size_t match=curves.size();unsigned direction=0;
        for (size_t j=0;j<curves.size();++j) if (j!=at && curves[j].points.size()>1 && !closed(curves[j])) {
            const auto &b=curves[j];const std::array<float,4> distances{distanceSquared(a.points.front(),b.points.front()),
                distanceSquared(a.points.front(),b.points.back()),distanceSquared(a.points.back(),b.points.front()),distanceSquared(a.points.back(),b.points.back())};
            const float next=*std::min_element(distances.begin(),distances.end());
            if (next<best) {best=next;match=j;direction=unsigned(std::find(distances.begin(),distances.end(),next)-distances.begin());if (next<1e-5) break;}
        }
        if (match==curves.size() || !(best<error)) {++at;continue;}
        auto &fixed=curves[at];auto moving=curves[match];
        if (direction==0 || direction==3) {std::reverse(moving.points.begin(),moving.points.end());std::reverse(moving.knuckles.begin(),moving.knuckles.end());}
        if (direction<2) {
            fixed.points.insert(fixed.points.begin(),moving.points.begin(),moving.points.end()-1);
            fixed.knuckles.insert(fixed.knuckles.begin(),moving.knuckles.begin(),moving.knuckles.end()-1);
        } else {
            fixed.knuckles.back()=fixed.knuckles.back() || moving.knuckles.front();
            fixed.points.insert(fixed.points.end(),moving.points.begin()+1,moving.points.end());
            fixed.knuckles.insert(fixed.knuckles.end(),moving.knuckles.begin()+1,moving.knuckles.end());
        }
        fixed.fragments=std::max(fixed.fragments,fixed.points.size()-1);fixed.sourceSegments.clear();
        curves.erase(curves.begin()+std::ptrdiff_t(match));at=0;
    }
}

struct Area {float value=0,x=0;};
Area stationArea(const SectionIndex &index,float x,float waterline,bool simplify) {
    auto result=index.intersect(Plane(1,0,0,-x),{true});auto curves=std::move(result.connectedCurves);
    const bool builtBeforeSimplify=curves.size()>1;
    if (builtBeforeSimplify) {
        joinLegacy(curves,.01f);
        for (size_t i=curves.size();i>0;--i) if (curves[i-1].points.size()>1) {
            Bounds b;for (auto p:curves[i-1].points) b.add(p);
            if (distanceSquared(b.lo,b.hi)<1e-3) curves.erase(curves.begin()+std::ptrdiff_t(i-1));
        }
    }
    // Surface's tiny-curve test reads Min/Max, building those splines. A sole
    // fresh curve has FTotalLength=0, so legacy Simplify returns without work.
    if (simplify && builtBeforeSimplify) for (auto &curve:curves) if (curve.points.size()>1) {
        Spline spline(curve.points,curve.knuckles,curve.fragments);spline.simplify(2.0f);
        curve.points=spline.points();curve.knuckles=spline.knuckles();
    }
    for (size_t i=curves.size();i>0;--i) {
        auto mirrored=curves[i-1];for (auto &p:mirrored.points) p.y=-p.y;curves.push_back(std::move(mirrored));
    }
    joinLegacy(curves,.05f);
    for (size_t i=curves.size();i>0;--i) {
        auto &curve=curves[i-1];if (curve.points.empty()) bad("Empty Lackenby section curve");
        Spline spline(curve.points,curve.knuckles,curve.fragments);float area=0;auto previous=curve.points.back();
        for (int j=0;j<=500;++j) {
            const auto p=spline.value(double(j)/500);const float delta=float(.5*(p.y+previous.y)*(p.z-previous.z));
            area=finite(area+delta);previous=p;
        }
        if (std::abs(area)<1e-4) curves.erase(curves.begin()+std::ptrdiff_t(i-1));
        else if (area<0) {std::reverse(curve.points.begin(),curve.points.end());std::reverse(curve.knuckles.begin(),curve.knuckles.end());}
    }
    const Plane plane(0,0,1,-waterline);Area total;
    for (const auto &curve:curves) {
        Spline spline(curve.points,curve.knuckles,500);IntersectionData hits;spline.intersectPlane(plane,hits);
        std::vector<float> parameters{0,1};parameters.insert(parameters.end(),hits.parameters.begin(),hits.parameters.end());
        std::sort(parameters.begin(),parameters.end());float t1=0,area=0;Vec3 previous{};
        for (size_t i=1;i<parameters.size();++i) {
            const float t2=parameters[i],middle=float(.5*(t1+t2));const auto mid=spline.value(middle);
            if (plane.evaluate(mid)<0) for (int j=0;j<=500;++j) {
                const float t=float(t1+(double(j)/500)*(t2-t1));const auto p=spline.value(t);
                if (j) {const float delta=float(.5*(p.y+previous.y)*(p.z-previous.z));area=finite(area+delta);}
                previous=p;
            }
            t1=t2;
        }
        total.value=finite(total.value+area);total.x=finite(total.x+area*(area!=0 ? x : 0));
    }
    total.x=total.value!=0 ? finite(total.x/total.value) : x;return total;
}

struct Body {float displacement=0,lcb=0,cp=0,length=0,k=0,p=0,z=0,a=0,b=0,c=0,dcp=0,dp=0;};
// Input-independent Power(previous, 0.9), starting at 2.5, for the permitted
// 100 updates. These exact Win64 FPC 3.2.2 binary64 values avoid host libm
// rounding drift. Generated and checked by the retained tests/qt Pascal probe
// and maintenance/lackenby_math.py; no model measurements are cached here.
// BEGIN FPC LACKENBY FACTORS
static constexpr std::array<double,100> ConvergenceFactors{
    0x1.23fb5fe030c24p+1, 0x1.0cde90f18e563p+1, 0x1.f345eeb441d87p+0, 0x1.d3032c2733828p+0,
    0x1.b7c38dfe093ecp+0, 0x1.a09a2c720f56cp+0, 0x1.8ccd370ba97d6p+0, 0x1.7bc9789615caap+0,
    0x1.6d1938ea740cap+0, 0x1.605d80be8211cp+0, 0x1.5549126305ac3p+0, 0x1.4b9c9f00309e4p+0,
    0x1.4323e38187288p+0, 0x1.3bb370d0083fep+0, 0x1.3526f3e953652p+0, 0x1.2f5fde424e432p+0,
    0x1.2a44574d29fdfp+0, 0x1.25be65ff40a5cp+0, 0x1.21bb458bdcc62p+0, 0x1.1e2adbb3b3676p+0,
    0x1.1aff495c9f557p+0, 0x1.182c8fde8c6c1p+0, 0x1.15a846bc05170p+0, 0x1.13695e74a3e01p+0,
    0x1.1167eddb572a2p+0, 0x1.0f9d07e9795dbp+0, 0x1.0e0298758695dp+0, 0x1.0c934688f6052p+0,
    0x1.0b4a5b5288544p+0, 0x1.0a23ace5ba992p+0, 0x1.091b8c205fda9p+0, 0x1.082eb52f3358ap+0,
    0x1.075a42436f92fp+0, 0x1.069ba01f8f1e3p+0, 0x1.05f0843172b98p+0, 0x1.0556e3fd1789ap+0,
    0x1.04cceda57aca9p+0, 0x1.04510169c1846p+0, 0x1.03e1abf3a1be6p+0, 0x1.037da159afe34p+0,
    0x1.0323b8bcd5f0dp+0, 0x1.02d2e86c0cc04p+0, 0x1.028a427c99eb4p+0, 0x1.0248f1c7b630fp+0,
    0x1.020e3740b20aep+0, 0x1.01d9679884a40p+0, 0x1.01a9e9253ceadp+0, 0x1.017f32051a31ep+0,
    0x1.0158c6762c6b4p+0, 0x1.0136375c4c331p+0, 0x1.011720f007a88p+0, 0x1.00fb2991d04cap+0,
    0x1.00e200bd4c882p+0, 0x1.00cb5e1930ff0p+0, 0x1.00b700a075b68p+0, 0x1.00a4ade21bf9ep+0,
    0x1.009431550e181p+0, 0x1.00855bbdeb623p+0, 0x1.007802a4d2d61p+0, 0x1.006bffd976e50p+0,
    0x1.00613103f5e8cp+0, 0x1.005777411e09ep+0, 0x1.004eb6c8e9641p+0, 0x1.0046d69e20c38p+0,
    0x1.003fc04630ff6p+0, 0x1.00395f8859374p+0, 0x1.0033a2336f7e9p+0, 0x1.002e77e98e2abp+0,
    0x1.0029d1f10e549p+0, 0x1.0025a30a44752p+0, 0x1.0021df4982922p+0, 0x1.001e7bf4f0694p+0,
    0x1.001b6f65d59c7p+0, 0x1.0018b0ecfc390p+0, 0x1.001638b9db304p+0, 0x1.0013ffc4409d1p+0,
    0x1.0011ffb83b167p+0, 0x1.001032e407edcp+0, 0x1.000e9427d226ep+0, 0x1.000d1ee713483p+0,
    0x1.000bcefb6be54p+0, 0x1.000aa0a8ce0c9p+0, 0x1.00099092d7974p+0, 0x1.00089bb33dca1p+0,
    0x1.0007bf512ece5p+0, 0x1.0006f8f9904fcp+0, 0x1.00064678050a1p+0, 0x1.0005a5d0a54e2p+0,
    0x1.0005153a58895p+0, 0x1.00049319bfa8dp+0, 0x1.00041dfca1d1ep+0, 0x1.0003b495ce5e7p+0,
    0x1.000355b9685d7p+0, 0x1.0003005991038p+0, 0x1.0002b383678a2p+0, 0x1.00026e5c55ee1p+0,
    0x1.0002301fa2da2p+0, 0x1.0001f81c41d17p+0, 0x1.0001c5b2db5dbp+0, 0x1.0001985407a58p+0,
};
// END FPC LACKENBY FACTORS
class Engine {
    GeometryInput input_;
    DocumentGeometrySettings settings_;
    std::unique_ptr<SubdivisionSurface> surface_;
    std::unique_ptr<SectionIndex> sections_;
    Vec3 minimum_{},maximum_{};float waterline_=0,mainArea_=0;
    std::array<float,41> aft_{},fore_{};
    void rebuild() {
        sections_.reset();surface_=std::make_unique<SubdivisionSurface>(input_);
        const auto &mesh=surface_->level(input_.desiredLevel);sections_=std::make_unique<SectionIndex>(input_,mesh);
    }
    Body part(const std::array<float,41> &stations) const {
        struct Sample {float area=0,distance=0,factor=0;};std::array<Sample,41> data{};Body p;
        for (size_t i=0;i<data.size();++i) {const auto a=stationArea(*sections_,stations[i],waterline_,settings_.simplifyIntersections);data[i]={a.value,a.x,0};trace("station",{-stations[i],a.value,a.x});}
        for (size_t i=0;i+1<data.size();i+=2) {
            const float distance=data[i+2].distance-data[i].distance;
            if (std::abs(distance)>1e-6) {
                float f=(data[i+1].distance-data[i].distance)/distance;
                if (std::abs(f)<1e-2) f=f<0 ? -1e-2f : 1e-2f;
                else if (std::abs(1-f)<1e-2) f=float(1-1e-2);
                // FPC folds an exactly representable real literal into the
                // surrounding Single expression; this is not a Double sum.
                data[i].factor=data[i].factor+.5f*distance*((3*f-1)/f);
                data[i+1].factor=data[i+1].factor+.5f*distance*(1/(f*(1-f)));
                data[i+2].factor=data[i+2].factor+.5f*distance*((2-3*f)/(1-f));
            }
        }
        for (const auto &s:data) {const float product=s.factor*s.area;p.displacement+=product;p.lcb+=product*s.distance;p.k+=product*s.distance*s.distance;
            trace("accum",{s.area,s.distance,s.factor,product,p.lcb});}
        p.displacement=finite(p.displacement/3);
        if (p.displacement!=0) {p.lcb=finite(p.lcb/(3*p.displacement));p.k=finite(std::sqrt(p.k/(3*p.displacement)));}
        else {p.lcb=0;p.k=0;}
        p.length=data.back().distance-data.front().distance;
        if (mainArea_!=0 && p.length!=0) p.cp=finite(p.displacement/(p.length*mainArea_));
        trace("part",{p.displacement,p.lcb,p.cp,p.length,p.k});return p;
    }
    static void initialize(Body &p) {
        p.a=p.cp*(1-2*p.lcb)-p.p*(1-p.cp);
        if (std::abs(p.a)<1e-10) bad("Lackenby transformation is singular for this hull");
        p.b=(p.cp*(2*p.lcb-3*p.k*p.k-p.p*(1-2*p.lcb))/p.a);
        p.c=(p.b*(1-p.cp)-p.cp*(1-2*p.lcb))/(1-p.p);finite(p.b);finite(p.c);
    }
public:
    explicit Engine(const Fbm &source):input_(documentGeometry(source)),settings_(documentGeometrySettings(source)) {
        if (settings_.draft<=0) bad("Set a positive design draft first");
        rebuild();
        const auto &mesh=surface_->level(input_.desiredLevel);float lowest=mesh.minimum.z;bool first=true;
        for (const auto &f:mesh.faces) if (mesh.layers[f.layer].useInHydrostatics) for (size_t p:f.points) {
            const float z=mesh.points[p].position.z;if (first || z<lowest) lowest=z;first=false;
        }
        waterline_=finite(lowest+settings_.draft);const Plane plane(0,0,1,-waterline_);Bounds bounds;
        for (const auto &f:mesh.faces) if (mesh.layers[f.layer].useInHydrostatics) {
            auto previous=mesh.points[f.points.back()].position;float s1=plane.evaluate(previous);
            for (size_t p:f.points) {
                auto next=mesh.points[p].position;const float s2=plane.evaluate(next);
                if ((s1<0 && s2>0) || (s1>0 && s2<0)) {
                    const float t=s1==s2 ? .5f : -s1/(s2-s1);
                    Vec3 crossing{previous.x+t*(next.x-previous.x),previous.y+t*(next.y-previous.y),previous.z+t*(next.z-previous.z)};
                    bounds.add(crossing);if (mesh.layers[f.layer].symmetric) {crossing.y=-crossing.y;bounds.add(crossing);}
                }
                if (s2<=0) {bounds.add(next);if (mesh.layers[f.layer].symmetric) {next.y=-next.y;bounds.add(next);}}
                // The original routine assigns its mirrored P2 to P1 here.
                previous=next;s1=s2;
            }
        }
        if (bounds.empty) {minimum_={};maximum_={1,1,1};} else {minimum_=bounds.lo;maximum_=bounds.hi;}
        if (settings_.split<=minimum_.x+1e-6 || settings_.split>=maximum_.x-1e-6 || maximum_.y-minimum_.y<=1e-6)
            bad("Midship must be inside the submerged hull");
        mainArea_=stationArea(*sections_,settings_.split,waterline_,settings_.simplifyIntersections).value;
        if (mainArea_<=1e-8) bad("No usable submerged midship area");
        for (int i=0;i<=40;++i) {
            aft_[i]=float(minimum_.x+(double(i)/40)*(settings_.split-minimum_.x));
            fore_[i]=float(settings_.split+(double(i)/40)*(maximum_.x-settings_.split));
        }
    }
    Body measure() const {
        const auto aft=part(aft_),fore=part(fore_);Body p;p.displacement=aft.displacement+fore.displacement;p.length=aft.length+fore.length;
        if (p.displacement<=0 || p.length<=0) bad("No usable submerged volume");
        p.lcb=finite((aft.displacement*aft.lcb+fore.displacement*fore.lcb)/p.displacement);
        p.cp=finite(p.displacement/(p.length*mainArea_));return p;
    }
    QJsonObject inspect() const {
        const auto p=measure();float displacement=p.displacement*settings_.appendage*settings_.density;
        if (settings_.imperial) displacement/=2240;
        finite(displacement);
        return {{"volume",double(p.displacement)},{"displacement",double(displacement)},{"lcb",double(p.lcb)},
                {"cp",double(p.cp)},{"length",double(p.length)},{"midship",double(settings_.split)}};
    }
    std::vector<FbmPosition> transform(const std::vector<size_t> &points,double displacement,float targetLCB,int limit,bool &changed) {
        float desiredVolume=float(displacement);
        desiredVolume=settings_.imperial ? desiredVolume*2240/settings_.density : desiredVolume/settings_.density;
        desiredVolume=finite(desiredVolume/settings_.appendage);
        if (points.empty() || desiredVolume<=0) bad("Invalid Lackenby parameters");
        if (targetLCB<=minimum_.x || targetLCB>=maximum_.x) bad("Target LCB must be inside the submerged hull");
        int iteration=1;size_t updates=0;float displacementError=1,lcbError=1;changed=false;
        while ((displacementError>1e-4 || lcbError>5e-5) && iteration<=limit) {
            auto aft=part(aft_),fore=part(fore_);Body total;total.displacement=aft.displacement+fore.displacement;
            if (total.displacement<=0) {++iteration;continue;}
            total.lcb=(aft.displacement*aft.lcb+fore.displacement*fore.lcb)/total.displacement;
            total.length=aft.length+fore.length;total.cp=total.displacement/(total.length*mainArea_);
            Body desired=total;desired.displacement=desiredVolume;desired.cp=desired.displacement/(desired.length*mainArea_);desired.lcb=targetLCB;
            displacementError=std::abs((desired.displacement-total.displacement)/desired.displacement);
            lcbError=std::abs(desired.lcb-total.lcb)<=2e-3 ? float(5e-5-1e-6) : std::abs((desired.lcb-total.lcb)/total.length);
            if (displacementError<=1e-4 && lcbError<=5e-5) break;
            if (aft.length<=0 || fore.length<=0) bad("Invalid underwater half-body length");
            aft.displacement/=aft.length*(maximum_.y-minimum_.y)*settings_.draft;
            aft.lcb=(settings_.split-aft.lcb)/aft.length;aft.k=(settings_.split-aft.k)/aft.length;
            fore.displacement/=fore.length*(maximum_.y-minimum_.y)*settings_.draft;
            fore.lcb=(fore.lcb-settings_.split)/fore.length;fore.k=(fore.k-settings_.split)/fore.length;
            desired.displacement/=desired.length*(maximum_.y-minimum_.y)*settings_.draft;
            desired.lcb=(desired.lcb-minimum_.x)/desired.length;desired.dcp=desired.cp-total.cp;
            initialize(aft);initialize(fore);
            total.z=(total.lcb-minimum_.x-.5f*total.length)/(.5f*total.length);desired.z=(desired.lcb-.5f)/.5f;
            float dz=desired.z-total.z;const double convergence=ConvergenceFactors.at(updates++);dz=float(convergence*dz);
            if (std::abs(fore.b+aft.b)<1e-10) bad("Lackenby transformation is singular for this hull");
            aft.dcp=(2*(desired.dcp*(fore.b-total.z)-dz*(total.cp+desired.dcp))-fore.c*fore.dp+aft.c*aft.dp)/(fore.b+aft.b);
            fore.dcp=(2*(desired.dcp*(aft.b+total.z)+dz*(total.cp+desired.dcp))+fore.c*fore.dp-aft.c*aft.dp)/(fore.b+aft.b);
            trace("iteration",{double(iteration),convergence,dz,aft.a,aft.b,aft.dcp,fore.a,fore.b,fore.dcp});
            for (size_t i:points) {
                auto &position=input_.points.at(i).position;const bool behind=position.x<settings_.split;const auto &p=behind ? aft : fore;
                float x=behind ? (settings_.split-position.x)/p.length : (position.x-settings_.split)/p.length;
                const float dx=(1-x)*(p.dp/(1-p.p)+(x-p.p)/p.a*(p.dcp-p.dp*(1-p.cp)/(1-p.p)));x=x+dx;
                position.x=behind ? settings_.split-x*p.length : x*p.length+settings_.split;
                if (!std::isfinite(position.x) || std::abs(position.x)>1e9) bad("Lackenby produced an invalid coordinate");
            }
            changed=true;rebuild();++iteration;
        }
        if (displacementError>1e-4 || lcbError>5e-5 || iteration>limit) bad("Lackenby transformation did not converge; all changes have been discarded");
        std::vector<FbmPosition> positions;positions.reserve(input_.points.size());
        for (const auto &p:input_.points) positions.push_back({p.position.x,p.position.y,p.position.z});
        return positions;
    }
};
}

bool nativeLackenbyCommand(const QJsonObject &request) {return request["method"]=="modeling" && request["operation"]=="lackenby";}
QJsonObject inspectLackenby(const Fbm &source) {return Engine(source).inspect();}
Fbm editLackenby(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request,bool *changed) {
    if (changed) *changed=false;
    if (!nativeLackenbyCommand(request)) bad("Unsupported native Lackenby request");
    const auto net=source.controlNet();const auto layers=documentGeometry(source).layers.size();
    const auto layerIds=bindings["layers"].toArray();if (size_t(layerIds.size())!=layers) bad("Lackenby layer bindings do not match document");
    QHash<int,size_t> lookup;for (qsizetype i=0;i<layerIds.size();++i) {const int id=integer(layerIds[i]);if (lookup.contains(id)) bad("Duplicate layer binding");lookup.insert(id,size_t(i));}
    const auto selection=request["ids"].toArray();if (!request["ids"].isArray() || selection.isEmpty() || selection.size()>100000) bad("Invalid Lackenby selection size");
    QSet<int> seen;std::vector<bool> selected(net.points.size(),false);std::vector<size_t> points;
    for (const auto value:selection) {
        const int id=integer(value);if (!lookup.contains(id) || seen.contains(id)) bad("Unknown or duplicate Lackenby layer");seen.insert(id);
        for (const auto &face:net.faces) if (size_t(face.layer<0 ? 0 : face.layer)==lookup[id]) for (int point:face.points) {
            const auto &p=net.points[size_t(point)].value;if (p.locked || p.hardAnchor || p.constrained) bad("Lackenby selection contains a locked or constrained point");
            if (!selected[size_t(point)]) {selected[size_t(point)]=true;points.push_back(size_t(point));}
        }
    }
    const double displacement=number(request["displacement"]);const float lcb=float(number(request["lcb"]));
    const int iterations=integer(request["iterations"],1,100);if (displacement<=0) bad("Target displacement must be positive");
    bool modified=false;auto positions=Engine(source).transform(points,displacement,lcb,iterations,modified);
    const auto result=modified ? source.withPointPositions(positions) : source;if (changed) *changed=modified;return result;
}
}
