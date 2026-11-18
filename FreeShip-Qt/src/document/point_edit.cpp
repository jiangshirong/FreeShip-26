// GPL-2.0-or-later. Semantics from current FreeSubdivisionControlPoint.SetCoordinate,
// MoveBy and FreeGometry_Functions.ProjectPointOnLine. Float operation order is
// intentional: it matches the existing FBM Single domain, without fast-math/FMA.
#include "point_edit.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void invalid(const char *message) { throw std::invalid_argument(message); }
quint64 id(const QJsonValue &v) {
    const double n = v.toDouble(-1);
    if (!v.isDouble() || !std::isfinite(n) || n < 1 || n > 9007199254740991.0 || std::floor(n) != n)
        invalid("Invalid control point ID");
    return quint64(n);
}
void check(FbmPosition p) {
    for (float n : {p.x, p.y, p.z}) if (!std::isfinite(n) || std::abs(n) > 1e9f)
        invalid("Coordinate is not finite or exceeds 1e9 document units");
}
FbmPosition position(const QJsonValue &v) {
    if (!v.isArray() || v.toArray().size() != 3) invalid("position must have three coordinates");
    auto a = v.toArray(); float values[3];
    for (int i = 0; i < 3; ++i) {
        double n = a[i].toDouble();
        if (!a[i].isDouble() || !std::isfinite(n) || std::abs(n) > 1e9)
            invalid("Coordinate is not finite or exceeds 1e9 document units");
        values[i] = float(n);
    }
    return {values[0], values[1], values[2]};
}
bool differs(FbmPosition a, FbmPosition b) { return a.x != b.x || a.y != b.y || a.z != b.z; }
FbmPosition subtract(FbmPosition a, FbmPosition b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
FbmPosition add(FbmPosition a, FbmPosition b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
}
FbmPosition projectPointOnLine(FbmPosition p, FbmPosition a, FbmPosition b) {
    auto d1 = subtract(p,a), d2 = subtract(b,a);
    const float length = d2.x*d2.x + d2.y*d2.y + d2.z*d2.z;
    if (length <= 1e-6) return p;
    const float t = (d1.x*d2.x + d1.y*d2.y + d1.z*d2.z) / length;
    return {a.x+t*d2.x, a.y+t*d2.y, a.z+t*d2.z};
}

PointEditor::PointEditor(std::vector<FbmControlPoint>& points,const QJsonObject& bindings):points_(points),anchored_(points.size()),constrained_(points.size()) {
    for(size_t i=0;i<points.size();++i){const auto& p=points[i];
        if(p.anchor>=0)anchored_.at(p.anchor).push_back(i);
        if(p.lineA>=0)constrained_.at(p.lineA).push_back(i);
        if(p.lineB>=0)constrained_.at(p.lineB).push_back(i);}
    const auto ids=bindings["points"].toArray();QHash<quint64,size_t> lookup;
    for(qsizetype i=0;i<ids.size();++i)lookup.insert(id(ids[i]),size_t(i));
    const auto restore=[&](const char* key,auto& lists){
        if(!bindings.contains(key))return;
        auto rows=bindings[key].toArray();if(!bindings[key].isArray()||size_t(rows.size())!=points.size()||size_t(ids.size())!=points.size())invalid("Invalid dependency order");
        for(size_t i=0;i<lists.size();++i){auto row=rows[qsizetype(i)].toArray();if(!rows[qsizetype(i)].isArray()||size_t(row.size())!=lists[i].size())invalid("Incomplete dependency order");
            QSet<size_t> expected;for(auto child:lists[i])expected.insert(child);std::vector<size_t> ordered;
            for(auto value:row){auto found=lookup.constFind(id(value));if(found==lookup.cend()||!expected.remove(*found))invalid("Invalid ordered dependency");ordered.push_back(*found);}
            lists[i]=std::move(ordered);
        }
    };
    restore("point_anchors",anchored_);restore("point_lines",constrained_);
}
void PointEditor::writeOrder(QJsonObject& bindings)const{
    const auto ids=bindings["points"].toArray();if(size_t(ids.size())!=points_.size())invalid("Invalid dependency point IDs");
    const auto save=[&](const char* key,const auto& lists){QJsonArray rows;for(const auto& list:lists){QJsonArray row;for(auto child:list)row.append(ids[qsizetype(child)]);rows.append(row);}bindings[key]=rows;};
    save("point_anchors",anchored_);save("point_lines",constrained_);
}
void PointEditor::anchor(size_t i,int parent,bool hard){
    auto& p=points_.at(i);if(p.anchor!=parent){
        if(p.anchor>=0){auto& v=anchored_.at(p.anchor);v.erase(std::remove(v.begin(),v.end(),i),v.end());}
        p.anchor=parent;if(parent>=0)anchored_.at(parent).push_back(i);
    }p.hardAnchor=hard;
}
void PointEditor::line(size_t i,int a,int b){
    auto& p=points_.at(i);if(p.lineA==a&&p.lineB==b)return;
    for(int parent:{p.lineA,p.lineB})if(parent>=0){auto&v=constrained_.at(parent);v.erase(std::remove(v.begin(),v.end(),i),v.end());}
    p.lineA=a;p.lineB=b;for(int parent:{a,b})if(parent>=0)constrained_.at(parent).push_back(i);
}
bool PointEditor::move(size_t root,FbmPosition target){
    auto position=[&](size_t i){const auto& p=points_.at(i);return FbmPosition{p.x,p.y,p.z};};
    const auto& p=points_.at(root);check(target);
    if(p.locked)invalid("Control point is locked");if(p.hardAnchor)invalid("Hard-anchored point cannot be moved directly");
    if(!differs(target,position(root)))return false;
    // Explicit frames preserve depth-first update order without recursive stack
    // overflow. Relation edits share precisely the same Single arithmetic.
    struct Frame{size_t point;FbmPosition target,delta{};bool linked;int phase=0;size_t next=0;};
    std::vector<Frame> stack{{root,target,{},false}};std::vector<bool> active(points_.size(),false);size_t visits=0;
    while(!stack.empty()){
        auto& frame=stack.back();const auto i=frame.point;auto& point=points_[i];
        if(frame.phase==0){
            if(++visits>2000000)invalid("Point dependency work limit exceeded");
            if(active[i]||point.locked||(!frame.linked&&point.hardAnchor)){stack.pop_back();continue;}
            active[i]=true;const auto old=position(i);auto next=frame.target;
            if(point.lineA>=0&&point.lineB>=0)next=projectPointOnLine(next,position(point.lineA),position(point.lineB));
            check(next);point.x=next.x;point.y=next.y;point.z=next.z;frame.delta=subtract(next,old);frame.phase=1;
        }
        if(frame.phase==1){
            if(frame.next<anchored_[i].size()){auto child=anchored_[i][frame.next++];auto next=add(position(child),frame.delta);stack.push_back({child,next,{},true});continue;}
            frame.phase=2;frame.next=0;
        }
        if(frame.next<constrained_[i].size()){auto child=constrained_[i][frame.next++];stack.push_back({child,position(child),{},true});continue;}
        active[i]=false;stack.pop_back();
    }return true;
}

Fbm editPoints(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request) {
    const auto &points = source.controlPoints();
    const auto ids = bindings["points"].toArray();
    if (size_t(ids.size()) != points.size()) invalid("Point bindings do not match FBM");
    QHash<quint64, size_t> lookup;
    std::vector<FbmPosition> positions; positions.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const auto key = id(ids[qsizetype(i)]);
        if (lookup.contains(key)) invalid("Duplicate bound point ID");
        lookup.insert(key, i); positions.push_back({points[i].x, points[i].y, points[i].z});
    }
    const auto resolve = [&](const QJsonValue &v) {
        auto it = lookup.constFind(id(v)); if (it == lookup.cend()) invalid("Unknown control point ID"); return *it;
    };
    const auto method = request["method"].toString();
    if (method == "move_points") {
        if (!request["moves"].isArray()) invalid("Invalid batch point array");
        auto moves = request["moves"].toArray();
        if (moves.isEmpty() || size_t(moves.size()) > points.size()) invalid("Invalid batch point count");
        QSet<size_t> seen;
        for (auto v : moves) {
            if (!v.isObject()) invalid("Invalid batch move");
            auto move = v.toObject(); const auto i = resolve(move["point_id"]);
            if (seen.contains(i)) invalid("Duplicate control point ID");
            seen.insert(i);
            if (points[i].locked || points[i].hardAnchor || points[i].constrained)
                invalid("Batch point is locked or participates in an anchor/linear constraint");
            positions[i] = position(move["position"]);
        }
    } else if (method == "move_point") {
        auto edited=points;PointEditor editor(edited,bindings);
        if(!editor.move(resolve(request["point_id"]),position(request["position"])))return source;
        for(size_t i=0;i<edited.size();++i)positions[i]={edited[i].x,edited[i].y,edited[i].z};
    } else invalid("Not a native point edit");
    return source.withPointPositions(positions);
}
}
