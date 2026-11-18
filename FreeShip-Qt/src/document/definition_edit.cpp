// GPL-2.0-or-later; see COPYING. Format and setter behavior adapted from
// FREE!ship/Plus FreeSubdivisionLayer.inc, FreeIntersection.inc and the
// migration adapter ModelEdits.pas / SectionEdits.inc. Original authors:
// Martijn van Engeland, Timoshenko V.F. and Mark Malakanov.
#include "definition_edit.h"
#include "fbm_text.h"
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) { throw std::invalid_argument(message); }
int integer(QJsonValue value, int low, int high) {
    const double n=value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n<low || n>high || n!=std::floor(n))
        bad("Invalid definition integer");
    return int(n);
}
float number(QJsonValue value, bool nonnegative=false) {
    const double n=value.toDouble();
    if (!value.isDouble() || !std::isfinite(n) || std::abs(n)>1e9 || (nonnegative && n<0))
        bad("Invalid bounded definition number");
    return float(n);
}
bool boolean(QJsonValue value) {
    if (!value.isBool()) bad("Expected definition boolean");
    return value.toBool();
}
QString text(QJsonValue value) {
    if (!value.isString()) bad("Expected definition text");
    const auto s=value.toString();
    if (!s.isValidUtf16() || s.contains(QChar(0))) bad("Invalid definition text");
    return s;
}
void putInt(QByteArray &data, int value) {
    char bytes[4];qToLittleEndian<qint32>(value,bytes);data.append(bytes,4);
}
void patchInt(QByteArray &data, qsizetype at, int value) {
    if (at<0 || at>data.size()-4) bad("Invalid definition field span");
    qToLittleEndian<qint32>(value,data.data()+at);
}
int getInt(const QByteArray &data, qsizetype at) {
    if (at<0 || at>data.size()-4) bad("Invalid definition field span");
    return qFromLittleEndian<qint32>(data.constData()+at);
}
float getFloat(const QByteArray &data, qsizetype at) {
    const auto bits=quint32(getInt(data,at));float value;std::memcpy(&value,&bits,4);return value;
}
void patchFloat(QByteArray &data, qsizetype at, float value) {
    quint32 bits;std::memcpy(&bits,&value,4);patchInt(data,at,qint32(bits));
}
struct Reader {
    QByteArray data;qsizetype at=0;
    QByteArray take(qsizetype n) {
        if (n<0 || n>data.size()-at) bad("Truncated definition record");
        const auto result=data.mid(at,n);at+=n;return result;
    }
    int int32() { const int result=getInt(data,at);at+=4;return result; }
    QByteArray string() { return take(int32()); }
    void end() const { if (at!=data.size()) bad("Unexpected definition data"); }
};

struct Layer {
    QByteArray name,fields; // Full field layout through 2.6, retaining raw bytes.
    int stable=0,original=-1;
    QJsonArray faces;
    bool flag(int at) const { return fields[at]!=0; }
    void flag(int at,bool value) { if (flag(at)!=value) fields[at]=char(value); }
};
QByteArray defaultFields() {
    QByteArray fields(26,'\0');
    for (int at:{8,9,11,12,21}) fields[at]=1;
    patchInt(fields,22,255);return fields;
}
struct LayerPlan {
    std::vector<Layer> layers;
    QJsonObject bindings;
    int active=0,minimum=0,last=-1,high=0;
    bool changed=false;
};
int resolvedColor(int raw,const QJsonObject &bindings) {
    if (raw>=0 && raw<=0xffffff) return raw;
    return integer(bindings["resolved_colors"].toObject()[QString::number(raw)],0,0xffffff);
}
bool patchLayer(Layer &layer,const QJsonObject &changes,const QJsonObject &bindings) {
    const auto oldName=layer.name,oldFields=layer.fields;
    const auto encoding=bindings["fbm_encoding"].toString();
    const auto storedName=decodeFbmText(layer.name,encoding);
    for (auto it=changes.begin();it!=changes.end();++it) {
        const auto key=it.key();const auto value=it.value();
        if (key=="name") {
            const auto name=text(value);
            const auto generated=bindings["layer_name_prefix"].toString()+" "+QString::number(getInt(layer.fields,0));
            if (!(storedName.isEmpty() && name==generated)) layer.name=encodeFbmText(name,encoding);
        } else if (key=="color") {
            const int color=integer(value,0,0xffffff);
            if (resolvedColor(getInt(layer.fields,4),bindings)!=color) patchInt(layer.fields,4,color);
        } else if (key=="alpha_blend") {
            patchInt(layer.fields,22,integer(value,0,255));
        } else if (key=="thickness" || key=="material_density") {
            const int at=key=="thickness" ? 17 : 13;const float n=number(value,true);
            if (getFloat(layer.fields,at)!=n) patchFloat(layer.fields,at,n);
        } else {
            const QMap<QString,int> flags{{"visible",8},{"symmetric",9},{"developable",10},
                {"use_for_intersections",11},{"use_in_hydrostatics",12},{"show_in_linesplan",21}};
            const auto field=flags.find(key);if (field==flags.end()) bad("Unsupported layer field");
            const bool flag=boolean(value);
            // Apply explicit symmetry after hydrostatics, regardless of JSON key order.
            if (key!="symmetric") {
                if (key=="use_in_hydrostatics" && flag && !layer.flag(12)) layer.flag(9,true);
                layer.flag(field.value(),flag);
            }
        }
    }
    if (changes.contains("symmetric")) layer.flag(9,boolean(changes["symmetric"]));
    return layer.name!=oldName || layer.fields!=oldFields;
}
int layerVersion(const Layer &layer) {
    int version=0;
    if (!layer.flag(11) || !layer.flag(12)) version=9;
    if (getFloat(layer.fields,13)!=0 || getFloat(layer.fields,17)!=0) version=11;
    if (!layer.flag(21)) version=15;
    if (getInt(layer.fields,22)!=255) version=21;
    return version;
}
LayerPlan planLayers(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    LayerPlan plan;plan.bindings=bindings;plan.minimum=source.version();
    Reader r{source.blockBytes("layers")};const int count=r.int32();
    const auto ids=bindings["layers"].toArray(),orders=bindings["layer_faces"].toArray();
    if (count<1 || count!=ids.size() || count!=orders.size()) bad("Layer bindings do not match the document");
    for (int i=0;i<count;++i) {
        Layer l;l.name=r.string();l.fields=defaultFields();l.fields.replace(0,11,r.take(11));
        if (source.version()>=9) l.fields.replace(11,2,r.take(2));
        if (source.version()>=11) l.fields.replace(13,8,r.take(8));
        if (source.version()>=15) l.fields.replace(21,1,r.take(1));
        if (source.version()>=21) l.fields.replace(22,4,r.take(4));
        l.stable=integer(ids[i],1,2000000);l.original=i;l.faces=orders[i].toArray();
        plan.high=std::max(plan.high,l.stable);plan.last=std::max(plan.last,getInt(l.fields,0));plan.layers.push_back(l);
    }
    plan.active=r.int32();r.end();
    if (bindings.contains("last_used_layer_id")) plan.last=std::max(plan.last,integer(bindings["last_used_layer_id"],-1,std::numeric_limits<int>::max()));
    plan.high=std::max(plan.high,integer(bindings["high_water"].toObject()["layers"],0,2000000));
    const auto findLayer=[&]() {
        const int id=integer(request["layer_id"],1,2000000);
        for (size_t i=0;i<plan.layers.size();++i) if (plan.layers[i].stable==id) return int(i);
        bad("Unknown layer ID");
    };
    const auto changes=[&]() {
        if (!request["changes"].isObject()) bad("Expected layer changes object");
        return request["changes"].toObject();
    };
    const auto erase=[&](int i) {
        if (!plan.layers[size_t(i)].faces.isEmpty()) bad("Only empty layers can be deleted; move their faces first");
        if (plan.layers.size()<=1) bad("Keep at least one layer");
        if (plan.active==i) plan.active=-1;else if (plan.active>i) --plan.active;
        plan.layers.erase(plan.layers.begin()+i);plan.changed=true;
    };
    if (request["method"]=="update_layer") {
        auto &layer=plan.layers[size_t(findLayer())];plan.changed=patchLayer(layer,changes(),bindings);
        if (plan.changed) plan.minimum=std::max(plan.minimum,layerVersion(layer));
    } else if (request["operation"]=="create") {
        if (plan.high>=2000000 || plan.last==std::numeric_limits<int>::max()) bad("Layer ID budget exhausted");
        Layer l;l.fields=defaultFields();l.stable=++plan.high;
        l.name=encodeFbmText({},bindings["fbm_encoding"].toString());
        patchInt(l.fields,0,++plan.last);
        patchInt(l.fields,4,integer(bindings["default_layer_color"],std::numeric_limits<int>::min(),std::numeric_limits<int>::max()));
        patchLayer(l,changes(),bindings);plan.minimum=std::max(plan.minimum,layerVersion(l));
        plan.active=int(plan.layers.size());plan.layers.push_back(l);plan.changed=true;
    } else if (request["operation"]=="move") {
        const int from=findLayer(),direction=integer(request["direction"],-1,1);
        if(direction==0)bad("Layer move requires a direction");
        const int to=from+direction;
        if(to>=0 && to<int(plan.layers.size())){
            std::swap(plan.layers[size_t(from)],plan.layers[size_t(to)]);
            if(plan.active==from)plan.active=to;else if(plan.active==to)plan.active=from;
            plan.changed=true;
        }
    } else if (request["operation"]=="delete") erase(findLayer());
    else if (request["operation"]=="delete_empty") {
        for (int i=int(plan.layers.size())-1;i>=0;--i)
            if (plan.layers[size_t(i)].faces.isEmpty() && plan.layers.size()>1) erase(i);
    } else bad("Unsupported layer operation");
    if (plan.active<0) plan.active=0;
    return plan;
}
TopologyEdit writeLayers(const Fbm &source,LayerPlan plan) {
    if (!plan.changed) return {source,plan.bindings,false};
    if (source.version()<plan.minimum) bad("Layer fields require a newer file version");
    QByteArray bytes;putInt(bytes,int(plan.layers.size()));QJsonArray ids,orders;
    std::vector<int> remap(size_t(plan.bindings["layers"].toArray().size()),-1);
    for (size_t i=0;i<plan.layers.size();++i) {
        const auto &l=plan.layers[i];putInt(bytes,int(l.name.size()));bytes+=l.name;
        int size=11;if (source.version()>=9) size=13;if (source.version()>=11) size=21;
        if (source.version()>=15) size=22;
        if (source.version()>=21) size=26;
        bytes+=l.fields.first(size);ids.append(l.stable);orders.append(l.faces);
        if (l.original>=0) remap[size_t(l.original)]=int(i);
    }
    putInt(bytes,plan.active);
    Reader faces{source.blockBytes("control_faces")};const int count=faces.int32();
    for (int i=0;i<count;++i) {
        const int n=faces.int32();faces.take(qsizetype(n)*4);const auto at=faces.at;const int old=faces.int32();
        const int index=std::max(0,old);if (size_t(index)>=remap.size() || remap[size_t(index)]<0) bad("Cannot remove a referenced layer");
        if (old>=0 && old!=remap[size_t(index)]) patchInt(faces.data,at,remap[size_t(index)]);
        faces.take(1);
    }
    faces.end();plan.bindings["layers"]=ids;plan.bindings["layer_faces"]=orders;
    plan.bindings["last_used_layer_id"]=plan.last;auto high=plan.bindings["high_water"].toObject();
    high["layers"]=plan.high;plan.bindings["high_water"]=high;
    return {source.withBlocks({{"layers",bytes},{"control_faces",faces.data}},true),plan.bindings,true};
}

constexpr std::array<const char *,4> SectionBlocks{"stations","buttocks","waterlines","diagonals"};
struct Section {
    int kind=0;char curvature=0;QByteArray plane;
    float distance() const {
        const float d=getFloat(plane,12);
        if (kind==4) {
            const float c=getFloat(plane,8);if (c==0) bad("Invalid diagonal plane");
            const float result=-d/c;if (!std::isfinite(result)) bad("Invalid diagonal distance");return result;
        }
        return -d;
    }
    void distance(float value) { patchFloat(plane,12,kind==4 ? -value*getFloat(plane,8) : -value); }
};
struct SectionPlan {
    std::array<std::vector<Section>,4> lists;
    int minimum=0;bool changed=false;
};
// Preserve Plus's exchange order for equal plane constants as well as its key.
void sortSections(std::vector<Section> &list,int left,int right) {
    int a=left,b=right;const float pivot=-getFloat(list[size_t((left+right)/2)].plane,12);
    do {
        while (-getFloat(list[size_t(a)].plane,12)<pivot) ++a;
        while (-getFloat(list[size_t(b)].plane,12)>pivot) --b;
        if (a<=b) {std::swap(list[size_t(a)],list[size_t(b)]);++a;--b;}
    } while (a<=b);
    if (left<b) sortSections(list,left,b);
    if (a<right) sortSections(list,a,right);
}
SectionPlan planSections(const Fbm &source,const QJsonObject &request) {
    SectionPlan plan;plan.minimum=source.version();int total=0;
    for (int i=0;i<4;++i) {
        if (i==3 && source.version()<9) continue;
        Reader r{source.blockBytes(SectionBlocks[size_t(i)])};const int count=r.int32();total+=count;
        for (int j=0;j<count;++j) {
            Section s;s.kind=r.int32();if (source.version()>=11) s.curvature=r.take(1)[0];s.plane=r.take(16);
            r.take(1);const int splines=r.int32();const int dimensions=source.version()>=6 && s.kind>=1 && s.kind<=3 ? 2 : 3;
            for (int k=0;k<splines;++k) {const int n=r.int32();r.take(qsizetype(n)*(dimensions*4+1));}
            plan.lists[size_t(i)].push_back(s);
        }
        r.end();
    }
    const int kind=integer(request["kind"],1,4);auto &list=plan.lists[size_t(kind-1)];
    const auto exists=[&](float value,int except=-1) {
        for (size_t i=0;i<list.size();++i) if (int(i)!=except && std::abs(double(list[i].distance())-value)<1e-7) return true;
        return false;
    };
    const auto operation=request["operation"].toString();
    if (operation=="add") {
        const auto values=request["positions"].toArray();
        if (values.isEmpty() || values.size()>512 || total+values.size()>4096) bad("Invalid section count");
        std::vector<float> positions;for (const auto v:values) positions.push_back(number(v));
        for (float p:positions) if (!exists(p)) {
            Section s;s.kind=kind;s.plane=QByteArray(16,'\0');
            if (kind==4) {const float c=float(1/std::sqrt(2.0));patchFloat(s.plane,4,c);patchFloat(s.plane,8,c);}
            else patchFloat(s.plane,(kind-1)*4,1);
            s.distance(p);list.push_back(s);plan.changed=true;
        }
        if (plan.changed && kind==4) plan.minimum=std::max(plan.minimum,9);
    } else if (operation=="update") {
        const int i=integer(request["index"],0,int(list.size())-1);auto &s=list[size_t(i)];
        if (request.contains("distance")) {
            const float p=number(request["distance"]);if (exists(p,i)) bad("A section already exists at that distance");
            if (s.distance()!=p) {s.distance(p);plan.changed=true;}
        }
        if (request.contains("show_curvature")) {
            const bool value=boolean(request["show_curvature"]);
            if ((s.curvature!=0)!=value) {s.curvature=char(value);plan.changed=true;if (value) plan.minimum=std::max(plan.minimum,11);}
        }
    } else if (operation=="delete" || operation=="clear") {
        std::vector<bool> selected(list.size(),operation=="clear");
        if (operation=="delete") {
            const auto values=request["indices"].toArray();
            if (values.isEmpty() || size_t(values.size())>list.size()) bad("Invalid section selection");
            for (auto v:values) {const int i=integer(v,0,int(list.size())-1);if (selected[size_t(i)]) bad("Duplicate section index");selected[size_t(i)]=true;}
        }
        for (int i=int(list.size())-1;i>=0;--i) if (selected[size_t(i)]) {list.erase(list.begin()+i);plan.changed=true;}
    } else bad("Unsupported section operation");
    if (plan.changed && list.size()>1) sortSections(list,0,int(list.size())-1);
    return plan;
}
TopologyEdit writeSections(const Fbm &source,const QJsonObject &bindings,const SectionPlan &plan) {
    if (!plan.changed) return {source,bindings,false};
    if (source.version()<plan.minimum) bad("Section fields require a newer file version");
    std::map<QString,QByteArray> blocks;
    for (int i=0;i<4;++i) {
        if (i==3 && source.version()<9) continue;
        QByteArray data;putInt(data,int(plan.lists[size_t(i)].size()));
        for (const auto &s:plan.lists[size_t(i)]) {
            putInt(data,s.kind);if (source.version()>=11) data+=s.curvature;
            data+=s.plane;data+=QByteArray(5,'\0'); // All section caches are invalidated, as in Plus.
        }
        blocks[SectionBlocks[size_t(i)]]=data;
    }
    return {source.withBlocks(blocks),bindings,true};
}
}
bool nativeDefinitionCommand(const QJsonObject &request) {
    const auto method=request["method"].toString();return method=="update_layer" || method=="manage_layers" || method=="sections";
}
int definitionMinimumVersion(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if (!nativeDefinitionCommand(request)) bad("Unsupported native definition edit");
    return request["method"]=="sections" ? planSections(source,request).minimum : planLayers(source,bindings,request).minimum;
}
TopologyEdit editDefinitions(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if (!nativeDefinitionCommand(request)) bad("Unsupported native definition edit");
    if (request["method"]=="sections") return writeSections(source,bindings,planSections(source,request));
    return writeLayers(source,planLayers(source,bindings,request));
}
}
