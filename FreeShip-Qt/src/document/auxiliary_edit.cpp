// GPL-2.0-or-later; see COPYING. Definition and setter behavior adapted from
// Plus FreeVisibility.inc / FreeSubdivisionControlCurve.inc / FreeFlowline.inc
// and the isolated DisplayEdits.inc / AuxiliaryCurves.inc contracts.
// FREE!ship/Plus authors: Martijn van Engeland, Timoshenko V.F., Mark Malakanov.
#include "auxiliary_edit.h"
#include "fbm_text.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) {throw std::invalid_argument(message);}
int integer(QJsonValue value,int low=0,int high=2000000) {
    const double n=value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n<low || n>high || std::floor(n)!=n) bad("Invalid auxiliary integer");
    return int(n);
}
float number(QJsonValue value) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || std::abs(n)>1e9) bad("Invalid auxiliary coordinate");
    return float(n);
}
bool flag(QJsonValue value) {if (!value.isBool()) bad("Invalid display Boolean");return value.toBool();}
void putInt(QByteArray &bytes,int n) {char v[4];qToLittleEndian<qint32>(n,v);bytes.append(v,4);}
void patchInt(QByteArray &bytes,qsizetype at,int n) {
    if (at<0 || at>bytes.size()-4) bad("Invalid auxiliary span");
    qToLittleEndian<qint32>(n,bytes.data()+at);
}
void putFloat(QByteArray &bytes,float n) {quint32 v;std::memcpy(&v,&n,4);putInt(bytes,qint32(v));}
struct Reader {
    QByteArray bytes;qsizetype at=0;
    QByteArray take(qsizetype n) {
        if (n<0 || n>bytes.size()-at) bad("Truncated auxiliary block");
        auto v=bytes.mid(at,n);at+=n;return v;
    }
    int int32() {const auto v=take(4);return qFromLittleEndian<qint32>(v.constData());}
    float real() {const auto bits=quint32(int32());float v;std::memcpy(&v,&bits,4);return v;}
    QByteArray string() {const int n=int32();return take(n);}
    void end() const {if (at!=bytes.size()) bad("Unexpected auxiliary block data");}
};
std::vector<QByteArray> flows(const Fbm &source) {
    std::vector<QByteArray> result;if (source.version()<20) return result;
    Reader r{source.blockBytes("flowlines")};const int count=r.int32();
    for (int i=0;i<count;++i) {
        const auto begin=r.at;r.take(14);const int n=r.int32();r.take(qsizetype(n)*13);
        result.push_back(r.bytes.mid(begin,r.at-begin));
    }
    r.end();return result;
}
QByteArray flowBytes(const std::vector<QByteArray> &records) {
    QByteArray bytes;putInt(bytes,int(records.size()));for (const auto &r:records) bytes+=r;return bytes;
}
struct Display {
    QByteArray bytes;int version,minimum;bool changed=false,clearCurveSelection=false,clearFlowSelection=false;
    explicit Display(const Fbm &source):bytes(source.blockBytes("visibility")),version(source.version()),minimum(version) {}
    void boolean(int at,bool value,int available=0) {
        const bool old=available>version ? true : bytes.at(at)!=0;
        if (old==value) return;
        changed=true;minimum=std::max(minimum,available);
        if (at==18 && !value) clearCurveSelection=true;
        if (at==29 && !value) clearFlowSelection=true;
        if (available<=version) bytes[at]=char(value);
    }
    void patch(const QJsonObject &changes) {
        for (auto it=changes.begin();it!=changes.end();++it) {
            const auto key=it.key();const auto value=it.value();
            if (key=="both_sides") {
                const int n=flag(value)?1:0;Reader r{bytes};
                if (r.int32()!=n) {patchInt(bytes,0,n);changed=true;}
            } else if (key=="show_normals") boolean(9,flag(value));
            else if (key=="show_markers") boolean(12,flag(value));
            else if (key=="show_curvature") boolean(13,flag(value));
            else if (key=="show_control_curves") boolean(18,flag(value),12);
            else if (key=="show_flowlines") boolean(29,flag(value),20);
            else if (key=="curvature_scale") {
                const float n=number(value);if (value.toDouble()<0) bad("Curvature scale must not be negative");
                Reader r{bytes,14};const float old=r.real();
                if (std::abs(n-old)>1e-7) {QByteArray v;putFloat(v,n);bytes.replace(14,4,v);changed=true;}
            } else bad("Unsupported display property");
        }
    }
    TopologyEdit finish(const Fbm &source,const QJsonObject &bindings) const {
        if (!changed) return {source,bindings,false};
        if (minimum>source.version()) bad("Display fields require format promotion");
        std::map<QString,QByteArray> blocks{{"visibility",bytes}};
        if (clearCurveSelection && source.version()>=12) {
            Reader r{source.blockBytes("control_curves")};const int count=r.int32();
            for (int i=0;i<count;++i) {const int n=r.int32();r.take(qsizetype(n)*4);r.bytes[r.at]=0;r.take(1);}
            r.end();blocks["control_curves"]=r.bytes;
        }
        if (clearFlowSelection && source.version()>=20) {
            auto records=flows(source);for (auto &record:records) record[13]=0;blocks["flowlines"]=flowBytes(records);
        }
        return {source.withBlocks(blocks),bindings,true};
    }
};
std::vector<int> bindingIds(const QJsonObject &bindings,const char *key,size_t count) {
    const auto array=bindings[key].toArray();
    if (size_t(array.size())!=count) bad("Auxiliary binding count mismatch");
    std::vector<int> result;QSet<int> seen;
    const int high=integer(bindings["high_water"].toObject()[key]);
    for (auto v:array) {const int id=integer(v,1,high);if (seen.contains(id)) bad("Duplicate auxiliary binding");seen.insert(id);result.push_back(id);}
    return result;
}
std::vector<int> select(const QJsonObject &request,const std::vector<int> &ids) {
    if (!request["ids"].isArray()) bad("Missing auxiliary selection");
    const auto array=request["ids"].toArray();if (array.isEmpty() || array.size()>100000) bad("Invalid auxiliary selection size");
    QHash<int,int> lookup;for (size_t i=0;i<ids.size();++i) lookup[ids[i]]=int(i);
    std::vector<int> selected;QSet<int> seen;
    for (auto v:array) {const int id=integer(v,1);if (seen.contains(id) || !lookup.contains(id)) bad("Deleted, duplicate or unknown auxiliary entity ID");seen.insert(id);selected.push_back(lookup[id]);}
    return selected;
}
quint64 pair(int a,int b) {if (a>b) std::swap(a,b);return (quint64(quint32(a))<<32)|quint32(b);}
struct Auxiliary {
    const Fbm &source;FbmControlNet net;QJsonObject bindings;Display display;
    std::vector<int> curveIds,flowIds;std::vector<QByteArray> flowRecords;
    std::map<int,QByteArray> names;int minimum;bool changed=false,netChanged=false,flowChanged=false,namesChanged=false;
    Auxiliary(const Fbm &s,const QJsonObject &b):source(s),net(s.controlNet()),bindings(b),display(s),minimum(s.version()) {
        curveIds=bindingIds(b,"curves",net.curves.size());flowRecords=flows(s);flowIds=bindingIds(b,"flows",flowRecords.size());
        if (s.version()>=41) {
            Reader r{s.blockBytes("curve_names")};const int n=r.int32();
            for (int i=0;i<n;++i) {const int index=r.int32();names[index]=r.string();}r.end();
        }
    }
    void appendId(const char *key,std::vector<int> &ids) {
        auto high=bindings["high_water"].toObject();const int id=integer(high[key]);
        if (id>=2000000) bad("Auxiliary stable ID budget exhausted");
        ids.push_back(id+1);high[key]=id+1;bindings["high_water"]=high;
    }
    void createControl(const QJsonObject &request) {
        auto indices=select(request,bindingIds(bindings,"edges",net.edges.size()));
        QSet<quint64> assigned;
        for (const auto &curve:net.curves) for (size_t i=1;i<curve.points.size();++i) assigned.insert(pair(curve.points[i-1],curve.points[i]));
        indices.erase(std::remove_if(indices.begin(),indices.end(),[&](int e){const auto &v=net.edges[size_t(e)];return assigned.contains(pair(v.a,v.b));}),indices.end());
        if (indices.empty()) return;
        const auto pointIds=bindingIds(bindings,"points",net.points.size());
        std::vector<std::vector<int>> adjacent(net.points.size());std::vector<bool> used(indices.size(),false);
        for (size_t i=0;i<indices.size();++i) {const auto &e=net.edges[size_t(indices[i])];adjacent[size_t(e.a)].push_back(int(i));adjacent[size_t(e.b)].push_back(int(i));}
        const auto trace=[&](int start,int first) {
            if (used[size_t(first)]) return;
            if (net.curves.size()>=4096) bad("Control curve count limit exceeded");
            FbmNetCurve curve;int point=start,next=first;curve.points.push_back(point);
            do {
                used[size_t(next)]=true;const auto &edge=net.edges[size_t(indices[size_t(next)])];point=edge.a==point?edge.b:edge.a;curve.points.push_back(point);
                if (point==start || adjacent[size_t(point)].size()!=2) break;
                next=-1;for (int j:adjacent[size_t(point)]) if (!used[size_t(j)]) next=j;
            } while (next>=0);
            net.curves.push_back(std::move(curve));appendId("curves",curveIds);
        };
        std::vector<int> order(net.points.size());std::iota(order.begin(),order.end(),0);
        std::sort(order.begin(),order.end(),[&](int a,int b){return pointIds[size_t(a)]<pointIds[size_t(b)];});
        for (int point:order) if (adjacent[size_t(point)].size()!=2) for (int edge:adjacent[size_t(point)]) trace(point,edge);
        for (size_t i=0;i<indices.size();++i) if (!used[i]) trace(net.edges[size_t(indices[i])].a,int(i));
        display.boolean(18,true,12);minimum=std::max(minimum,12);changed=netChanged=true;
    }
    void rename(const QJsonObject &request) {
        const auto selected=select(request,curveIds);if (selected.size()!=1) bad("Select one control curve");
        if (!request["name"].isString()) bad("Invalid curve name");
        const auto name=request["name"].toString();const auto encoding=bindings["fbm_encoding"].toString();
        const auto encoded=encodeFbmText(name,encoding);
        if (name.toUtf8().size()>4096) bad("Curve name exceeds 4096 UTF-8 bytes");
        const int index=selected.front();const auto old=names.find(index);
        const auto previous=old==names.end()?QString{}:decodeFbmText(old->second,encoding);
        if (name==previous) return;
        if (name.isEmpty()) names.erase(index);else names[index]=encoded;
        minimum=std::max(minimum,41);changed=namesChanged=true;
    }
    void createFlow(const QJsonObject &request) {
        if (flowRecords.size()>=4096) bad("Flowline count limit exceeded");
        const int view=integer(request["view"],0,2);const auto p=request["source"].toArray();
        if (!request["source"].isArray() || p.size()!=2) bad("Flow source requires two coordinates");
        QByteArray record;putFloat(record,number(p[0]));putFloat(record,number(p[1]));putInt(record,view);record+=QByteArray(6,'\0');
        flowRecords.push_back(record);appendId("flows",flowIds);display.boolean(29,true,20);
        minimum=std::max(minimum,20);changed=flowChanged=true;
        // The service validates the newly computed trace before committing.
        // Definition ownership is native; surface tracing remains a read-only
        // geometry service until that separate numerical module is migrated.
    }
    void remove(const QJsonObject &request) {
        const auto kind=request["kind"].toString();if (kind!="curve" && kind!="flow") bad("Invalid auxiliary kind");
        auto selected=select(request,kind=="curve"?curveIds:flowIds);std::sort(selected.rbegin(),selected.rend());
        for (int i:selected) {
            if (kind=="curve") {net.curves.erase(net.curves.begin()+i);curveIds.erase(curveIds.begin()+i);netChanged=true;}
            else {flowRecords.erase(flowRecords.begin()+i);flowIds.erase(flowIds.begin()+i);flowChanged=true;}
        }
        changed=true;
    }
    void apply(const QJsonObject &request) {
        const auto op=request["operation"].toString();
        if (op=="create_control") createControl(request);
        else if (op=="rename_control") rename(request);
        else if (op=="create_flow") createFlow(request);
        else if (op=="delete") remove(request);
        else bad("Unsupported auxiliary operation");
    }
    void normalizeFreeCurves() {
        // SubdivideFreeStanding(0) changes persistent vertex types as well as
        // samples. Include those changes in the native candidate, so a
        // read-only rebuild cannot silently change the committed control net.
        std::vector<bool> onFace(net.points.size(),false);
        for (const auto &face:net.faces) for (int p:face.points) onFace[size_t(p)]=true;
        QHash<quint64,int> lookup;std::vector<int> creases(net.points.size(),0);
        for (size_t i=0;i<net.edges.size();++i) {
            const auto &edge=net.edges[i];lookup[pair(edge.a,edge.b)]=int(i);
            if (edge.crease) {++creases[size_t(edge.a)];++creases[size_t(edge.b)];}
        }
        for (const auto &curve:net.curves) {
            if (std::any_of(curve.points.begin(),curve.points.end(),[&](int p){return onFace[size_t(p)];})) continue;
            for (size_t j=1;j<curve.points.size();++j) {
                const int a=curve.points[j-1],b=curve.points[j];const auto it=lookup.constFind(pair(a,b));
                if (it==lookup.cend()) bad("Free control curve has a missing edge");
                auto &edge=net.edges[size_t(*it)];
                if (!edge.crease) {
                    edge.crease=true;
                    for (int p:{a,b}) {
                        const int count=++creases[size_t(p)];auto &type=net.points[size_t(p)].value.vertexType;
                        if (type!=3) type=count==1?2:count==2?1:3;
                    }
                }
                auto &first=net.points[size_t(a)].value.vertexType;auto &second=net.points[size_t(b)].value.vertexType;
                if (j==1) first=3;
                if (j+1==curve.points.size()) second=3;
                if (first!=3) first=1;
                if (second!=3) second=1;
            }
        }
    }
    TopologyEdit finish() {
        if (!changed) return {source,bindings,false};
        if (minimum>source.version()) bad("Auxiliary edit requires format promotion");
        if (netChanged) normalizeFreeCurves();
        Fbm candidate=netChanged?source.withControlNet(net,true):source;
        std::map<QString,QByteArray> blocks;
        if (display.changed) blocks["visibility"]=display.bytes;
        if (flowChanged) blocks["flowlines"]=flowBytes(flowRecords);
        if (namesChanged) {
            QByteArray data;putInt(data,int(names.size()));
            for (const auto &[index,name]:names) {putInt(data,index);putInt(data,int(name.size()));data+=name;}
            blocks["curve_names"]=data;
        }
        const auto ids=[](const std::vector<int> &values){QJsonArray result;for (int id:values) result.append(id);return result;};
        bindings["curves"]=ids(curveIds);bindings["flows"]=ids(flowIds);
        return {candidate.withBlocks(blocks),bindings,true};
    }
};
}
bool nativeAuxiliaryCommand(const QJsonObject &request) {return request["method"]=="display" || request["method"]=="auxiliary";}
int auxiliaryMinimumVersion(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if (!nativeAuxiliaryCommand(request)) bad("Unsupported native auxiliary edit");
    if (request["method"]=="display") {
        if (!request["changes"].isObject()) bad("Expected display changes object");
        Display plan(source);plan.patch(request["changes"].toObject());return plan.minimum;
    }
    Auxiliary plan(source,bindings);plan.apply(request);return plan.minimum;
}
TopologyEdit editAuxiliary(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if (!nativeAuxiliaryCommand(request)) bad("Unsupported native auxiliary edit");
    if (request["method"]=="display") {
        if (!request["changes"].isObject()) bad("Expected display changes object");
        Display plan(source);plan.patch(request["changes"].toObject());return plan.finish(source,bindings);
    }
    Auxiliary plan(source,bindings);plan.apply(request);return plan.finish();
}
}
