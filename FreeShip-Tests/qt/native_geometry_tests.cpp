// Test-only JSON adapter for the production-candidate pure C++ geometry library.
#include "native/geometry.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ng=fs::native_geometry;

namespace {
[[noreturn]] void bad(const char* message) { throw std::invalid_argument(message); }
double number(QJsonValue value) {
    if (!value.isDouble() || !std::isfinite(value.toDouble())) bad("Expected finite JSON number");
    return value.toDouble();
}
float scalar(QJsonValue value) {
    const double n=number(value);const float s=static_cast<float>(n);
    if (!std::isfinite(s)) bad("Input exceeds Single range");
    return s;
}
std::size_t index(QJsonValue value) {
    const double n=number(value);if (n<0 || n>1000000 || std::floor(n)!=n) bad("Invalid index or sample count");
    return static_cast<std::size_t>(n);
}
ng::Vec3 vector(QJsonValue value) {
    if (!value.isArray() || value.toArray().size()!=3) bad("Expected three coordinates");
    const auto a=value.toArray();return {scalar(a[0]),scalar(a[1]),scalar(a[2])};
}
QJsonArray json(ng::Vec3 p) { return {double(p.x),double(p.y),double(p.z)}; }
QJsonValue json(const std::optional<ng::Vec3>& p) { return p ? QJsonValue(json(*p)) : QJsonValue(QJsonValue::Null); }
ng::Plane plane(QJsonValue value) {
    if (!value.isArray() || value.toArray().size()!=4) bad("Expected four plane coefficients");
    const auto a=value.toArray();return ng::Plane(scalar(a[0]),scalar(a[1]),scalar(a[2]),scalar(a[3]));
}
ng::Plane plane(QJsonObject item) {
    if (item.contains("coefficients")) return plane(item["coefficients"]);
    if (item.contains("points")) {
        const auto p=item["points"].toArray();if(p.size()!=3)bad("Plane needs three points");
        return ng::Plane::fromPoints(vector(p[0]),vector(p[1]),vector(p[2]));
    }
    return ng::Plane::fromPointNormal(vector(item["point"]),vector(item["normal"]));
}
QString relation(ng::LineKind kind) {
    switch (kind) {
    case ng::LineKind::Point:return "point";
    case ng::LineKind::Parallel:return "parallel";
    case ng::LineKind::Coplanar:return "coplanar";
    case ng::LineKind::Invalid:return "invalid";
    }
    throw std::runtime_error("Unknown line relation");
}
QJsonObject cut(const ng::IntersectionData& data,bool found) {
    QJsonArray points,parameters;
    for(auto p:data.points)points.append(json(p));
    for(auto t:data.parameters)parameters.append(double(t));
    return {{"points",points},{"parameters",parameters},{"found",found},{"count",int(data.points.size())}};
}
QJsonObject planeResult(const QJsonObject& item) {
    const auto p=plane(item);QJsonArray coefficients,evaluations,projections,mirrors,boxes,lines;
    for(float c:p.coefficients())coefficients.append(double(c));
    for(const auto& point:item["query_points"].toArray()) {
        const auto v=vector(point);
        evaluations.append(p.valid()?QJsonValue(double(p.evaluate(v))):QJsonValue(QJsonValue::Null));
        projections.append(json(p.project(v)));mirrors.append(json(p.mirror(v)));
    }
    for(const auto& value:item["boxes"].toArray()) {
        const auto box=value.toObject();
        boxes.append(p.valid()?QJsonValue(p.intersectsBox(vector(box["min"]),vector(box["max"]))):QJsonValue(QJsonValue::Null));
    }
    for(const auto& value:item["lines"].toArray()) {
        const auto line=value.toObject();const auto hit=p.intersectLine(vector(line["point"]),vector(line["direction"]));
        QJsonObject result{{"relation",relation(hit.kind)}};
        if(hit.kind==ng::LineKind::Point) {result.insert("point",json(hit.point));result.insert("parameter",double(hit.parameter));}
        lines.append(result);
    }
    return {{"valid",p.valid()},{"coefficients",coefficients},{"evaluations",evaluations},
            {"projections",projections},{"mirrors",mirrors},{"boxes",boxes},{"lines",lines}};
}
QJsonObject splineResult(ng::Spline& spline,const QJsonObject& item) {
    QJsonArray knots,values,first,second,cuts,repeated;
    for(float t:spline.parameters())knots.append(double(t));
    for(const auto& t:item["parameters"].toArray()) {
        const double parameter=number(t);
        values.append(json(spline.value(parameter)));
        first.append(json(spline.firstDerivative(static_cast<float>(parameter))));
        second.append(json(spline.secondDerivative(static_cast<float>(parameter))));
    }
    ng::IntersectionData output;
    output.points.push_back({999,999,999});output.parameters.push_back(999);
    for(const auto& value:item["planes"].toArray()) {
        const auto p=plane(value);const bool found=spline.intersectPlane(p,output);cuts.append(cut(output,found));
        const bool foundAgain=spline.intersectPlane(p,output);repeated.append(cut(output,foundAgain));
    }
    return {{"knots",knots},{"total_length",double(spline.totalChordLength())},
            {"values",values},{"first",first},{"second",second},{"cuts",cuts},{"cuts_repeat",repeated}};
}
QJsonObject splineResult(const QJsonObject& item) {
    std::vector<ng::Vec3> points;std::vector<bool> knuckles;
    for(const auto& point:item["points"].toArray())points.push_back(vector(point));
    for(const auto& flag:item["knuckles"].toArray()) {
        if(!flag.isBool())bad("Knuckles must be Boolean");
        knuckles.push_back(flag.toBool());
    }
    ng::Spline spline(points,knuckles,item.contains("fragments")?index(item["fragments"]):0);
    auto result=splineResult(spline,item);QJsonArray mutations;
    for(const auto& value:item["mutations"].toArray()) {
        const auto change=value.toObject();const auto op=change["op"].toString();
        if(op=="set_point")spline.setPoint(index(change["index"]),vector(change["point"]));
        else if(op=="set_knuckle") {if(!change["value"].isBool())bad("Knuckle must be Boolean");spline.setKnuckle(index(change["index"]),change["value"].toBool());}
        else if(op=="set_fragments")spline.setFragments(index(change["value"]));
        else bad("Unknown spline mutation");
        mutations.append(splineResult(spline,item));
    }
    if(item.contains("mutations"))result.insert("mutations",mutations);
    return result;
}
QJsonArray apiChecks() {
    QJsonArray cases;
    auto check=[&](const char* name,bool pass){cases.append(QJsonObject{{"id",name},{"status",pass?"pass":"fail"}});};
    auto throws=[](auto operation){try{operation();return false;}catch(const std::exception&){return true;}};
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    check("plane-rejects-nan",throws([&]{ng::Plane p(nan,0,1,0);(void)p;}));
    check("plane-rejects-infinity",throws([&]{ng::Plane p(0,0,1,inf);(void)p;}));
    check("spline-rejects-nonfinite-control-point",throws([&]{ng::Spline s({{0,0,0},{1,inf,0}});(void)s;}));
    ng::Spline spline({{0,0,0},{1,1,0},{2,0,0}});
    check("spline-rejects-nonfinite-parameter",throws([&]{spline.value(nan);}));
    check("spline-rejects-nonfinite-derivative",throws([&]{spline.firstDerivative(inf);}));
    check("spline-rejects-bad-point-index",throws([&]{spline.setPoint(3,{1,2,3});}));
    check("spline-rejects-bad-knuckle-index",throws([&]{spline.setKnuckle(3,true);}));
    check("spline-rejects-unbounded-fragments",throws([&]{spline.setFragments(1000001);}));
    const auto before=spline.value(.25L);const auto builds=spline.rebuildCount();
    for(int i=0;i<10;++i)(void)spline.value(.25L);
    check("unchanged-evaluations-reuse-built-data",spline.built()&&spline.rebuildCount()==builds);
    spline.setFragments(16);(void)spline.value(.25L);
    check("sampling-change-keeps-geometry-cache",spline.rebuildCount()==builds);
    spline.setPoint(1,{1,2,0});check("point-change-invalidates-cache",!spline.built());
    const auto changed=spline.value(.25L);
    check("point-change-rebuilds-once",spline.rebuildCount()==builds+1&&changed.y!=before.y);
    spline.setKnuckle(1,true);check("knuckle-change-invalidates-cache",!spline.built());
    (void)spline.value(.25L);check("knuckle-change-rebuilds-once",spline.rebuildCount()==builds+2);
    ng::Plane plane(0,0,1,0);
    check("plane-rejects-inverted-box",throws([&]{plane.intersectsBox({1,0,0},{0,1,1});}));
    check("spline-rejects-nonfinite-second-derivative",throws([&]{spline.secondDerivative(nan);}));
    const auto stable=spline.value(.25L);const auto stableBuilds=spline.rebuildCount();
    check("failed-point-mutation-rejected",throws([&]{spline.setPoint(1,{0,nan,0});}));
    check("failed-index-mutation-rejected",throws([&]{spline.setPoint(999,{0,1,0});}));
    const auto preserved=spline.value(.25L);
    check("failed-mutation-preserves-value-and-cache",spline.built()&&spline.rebuildCount()==stableBuilds&&stable.x==preserved.x&&stable.y==preserved.y&&stable.z==preserved.z);
    spline.setPoint(1,spline.points()[1]);spline.setKnuckle(1,spline.knuckles()[1]);(void)spline.value(.25L);
    check("no-op-point-and-knuckle-keep-cache",spline.built()&&spline.rebuildCount()==stableBuilds);
    check("mismatched-knuckle-count-rejected",throws([&]{ng::Spline s({{0,0,0},{1,0,0}},{true});(void)s;}));
    ng::Plane invalid(0,0,0,1);
    check("invalid-plane-evaluation-rejected",throws([&]{invalid.evaluate({1,2,3});}));
    check("invalid-plane-box-rejected",throws([&]{invalid.intersectsBox({0,0,0},{1,1,1});}));
    ng::IntersectionData output{{{999,999,999}},{999}};
    check("invalid-plane-spline-cut-rejected",throws([&]{spline.intersectPlane(invalid,output);}));
    check("failed-cut-clears-partial-output",output.points.empty()&&output.parameters.empty());
    ng::Spline empty,single({{9,8,7}});
    output={{{999,999,999}},{999}};const bool emptyFound=empty.intersectPlane(plane,output);
    check("empty-spline-has-no-origin-intersection",!emptyFound&&output.points.empty()&&output.parameters.empty());
    output={{{999,999,999}},{999}};const bool singleFound=single.intersectPlane(plane,output);
    check("single-point-spline-has-no-origin-intersection",!singleFound&&output.points.empty()&&output.parameters.empty());
    const float maximum=std::numeric_limits<float>::max();
    check("finite-plane-overflow-rejected",throws([&]{ng::Plane p(maximum,maximum,maximum,maximum);p.evaluate({2,2,2});}));
    check("finite-spline-overflow-rejected",throws([&]{ng::Spline s({{-maximum,0,0},{maximum,0,0}});s.value(.5L);}));
    const auto large=ng::Spline({{1000000000,0,0},{1000000000,4,0}}).value(.5L);
    check("large-representable-coordinate",large.x==1000000000&&large.y==2&&large.z==0);
    ng::Spline unsampled({{0,0,0},{1,1,0}}, {}, 0);
    output={{{999,999,999}},{999}};
    check("zero-fragment-cut-rejected",throws([&]{unsampled.intersectPlane(plane,output);}));
    check("zero-fragment-cut-clears-output",output.points.empty()&&output.parameters.empty());
    const auto unsampledValue=unsampled.value(.5L);
    check("zero-fragment-evaluation-remains-available",unsampledValue.x==.5f&&unsampledValue.y==.5f);
    ng::Spline recoverable({{0,0,0},{1,1,0},{2,0,0}});
    const auto recoveryBefore=recoverable.value(.25);const auto recoveryBuilds=recoverable.rebuildCount();
    recoverable.setPoint(1,{maximum,maximum,maximum});
    check("failed-rebuild-rejected",throws([&]{recoverable.value(.25);}));
    check("failed-rebuild-does-not-publish-cache",!recoverable.built()&&recoverable.rebuildCount()==recoveryBuilds);
    recoverable.setPoint(1,{1,1,0});const auto recoveryAfter=recoverable.value(.25);
    check("failed-rebuild-can-recover",recoverable.built()&&recoverable.rebuildCount()==recoveryBuilds+1&&recoveryBefore.x==recoveryAfter.x&&recoveryBefore.y==recoveryAfter.y&&recoveryBefore.z==recoveryAfter.z);
    // Independently solve x=0: t=-x/dx. Scaling the plane normal must not
    // change that classification, even when a Single dot product underflows.
    const float tinyDirection=1e-30f;
    const auto tinyHit=ng::Plane(1e-18f,0,0,0).intersectLine({1,0,0},{tinyDirection,0,0});
    check("tiny-dot-product-remains-intersection",tinyHit.kind==ng::LineKind::Point&&tinyHit.parameter==static_cast<float>(-1.0/double(tinyDirection)));
    check("tiny-dot-product-intersection-point",tinyHit.point.x==0&&tinyHit.point.y==0&&tinyHit.point.z==0);
    const auto hugeHit=ng::Plane(1e18f,0,0,0).intersectLine({1e30f,0,0},{1e30f,0,0});
    check("large-products-finite-intersection",hugeHit.kind==ng::LineKind::Point&&hugeHit.parameter==-1&&hugeHit.point.x==0&&hugeHit.point.y==0&&hugeHit.point.z==0);
    ng::Plane xZero(1,0,0,0);const float subnormal=std::numeric_limits<float>::denorm_min();
    check("unrepresentable-line-parameter-overflow-rejected",throws([&]{xZero.intersectLine({1,0,0},{subnormal,0,0});}));
    check("unrepresentable-line-parameter-underflow-rejected",throws([&]{xZero.intersectLine({subnormal,0,0},{1e30f,0,0});}));
    // Exact powers of two: dot = 1 + 2^-160 - 1, so the line intersects.
    // The result t=-2^100, (-2^40,0,2^40) is exactly representable in Single.
    const float p60=std::ldexp(1.0f,60),m60=std::ldexp(1.0f,-60),m100=std::ldexp(1.0f,-100);
    const auto cancellationHit=ng::Plane(p60,m60,p60,0).intersectLine({0,1,0},{m60,m100,-m60});
    check("cancellation-retains-small-nonzero-dot",cancellationHit.kind==ng::LineKind::Point&&cancellationHit.parameter==-std::ldexp(1.0f,100));
    check("cancellation-intersection-point-exact",cancellationHit.point.x==-std::ldexp(1.0f,40)&&cancellationHit.point.y==0&&cancellationHit.point.z==std::ldexp(1.0f,40));
    return cases;
}
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);if(argc!=3)return 3;
    QJsonObject report{{"format","FreeShipNativeGeometryResults"},{"version",1}};
    try {
        QFile input(app.arguments()[1]);if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open cases");
        QJsonParseError error;const auto document=QJsonDocument::fromJson(input.readAll(),&error);
        if(error.error!=QJsonParseError::NoError||!document.isObject())throw std::runtime_error("Invalid case JSON");
        const auto root=document.object();
        if(root["format"]!="FreeShipNativeGeometryCases"||root["version"].toInt()!=1||!root["cases"].isArray())throw std::runtime_error("Unsupported case schema");
        QJsonArray results;QElapsedTimer timer;timer.start();
        for(const auto& value:root["cases"].toArray()) {
            const auto item=value.toObject();QJsonObject result{{"id",item["id"]},{"kind",item["kind"]}};
            try {
                const auto kind=item["kind"].toString();
                if(kind=="plane")result=planeResult(item);else if(kind=="spline")result=splineResult(item);else bad("Unknown geometry kind");
                result.insert("id",item["id"]);result.insert("kind",kind);result.insert("status","ok");
            } catch(const std::exception& ex) {
                result.insert("status","error");result.insert("error",QString::fromUtf8(ex.what()));
                result.insert("error_kind",dynamic_cast<const std::domain_error*>(&ex)?"domain_error":"exception");
            }
            results.append(result);
        }
        report.insert("cases",results);report.insert("api_checks",apiChecks());
        report.insert("elapsed_ms",timer.nsecsElapsed()/1e6);
        report.insert("numeric_types",QJsonObject{{"float_bytes",int(sizeof(float))},{"double_bytes",int(sizeof(double))},{"long_double_bytes",int(sizeof(long double))},{"value_parameter","JSON Double passed to C++ double, matching Win64 FPC Extended"}});
    } catch(const std::exception& ex) {report.insert("error",QString::fromUtf8(ex.what()));}
    QSaveFile output(app.arguments()[2]);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly)||output.write(QJsonDocument(report).toJson())<0||!output.commit())return 3;
    return report.contains("error")?3:0;
}
