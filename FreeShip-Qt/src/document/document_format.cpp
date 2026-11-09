// GPL-2.0-or-later. FBM layout/defaults from current Plus FreeShip.inc,
// FreeProjectSettings, FreeVisibility and entity LoadBinary/SaveBinary routines.
// Original FREE!ship: Martijn van Engeland; Plus: Timoshenko and Malakanov.
#include "document_create.h"
#include "point_edit.h"
#include "geometry_input.h"
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *s) { throw std::invalid_argument(s); }
struct Bytes {
    QByteArray data;
    void integer(int n) { char b[4];qToLittleEndian<qint32>(n,b);data.append(b,4); }
    void real(float x) { quint32 n;std::memcpy(&n,&x,4);integer(qint32(n)); }
    void flag(bool b) { data.append(char(b)); }
    void string(QByteArray s) { integer(s.size());data+=s; }
};
struct Read {
    QByteArray data;qsizetype at=0;
    QByteArray take(qsizetype n) { if(n<0||n>data.size()-at)bad("Truncated document field");auto v=data.mid(at,n);at+=n;return v; }
    int integer() { auto s=take(4);return qFromLittleEndian<qint32>(s.constData()); }
    QByteArray string() { return take(integer()); }
    void end() { if(at!=data.size())bad("Unexpected document field layout"); }
};
using Parts=std::map<int,QByteArray>;
Parts visibility(Read r,int v) {
    Parts p;p[0]=r.take(18);
    if(v>=12)p[12]=r.take(1);
    if(v>=16)p[16]=r.take(4);
    if(v>=17)p[17]=r.take(6);
    if(v>=20)p[20]=r.take(1);
    r.end();return p;
}
Parts settings(Read r,int v) {
    Parts p;
    auto piece=[&](int key,auto action){auto start=r.at;action();p[key]=r.data.mid(start,r.at-start);};
    if(v>=2) {
        piece(2,[&]{r.string();r.string();r.take(30);});
        if(v>=6)p[6]=r.take(5);
        if(v>=7)p[7]=r.take(1);
        piece(3,[&]{r.string();r.string();});
    }
    if(v>=16)piece(16,[&]{r.take(4);if(r.take(1)[0]){r.take(8);r.take(r.integer());}});
    if(v>=18)p[18]=r.take(1);
    if(v>=20)piece(20,[&]{r.take(16);r.take(qsizetype(r.integer())*4);r.take(13);r.take(qsizetype(r.integer())*4);r.take(qsizetype(r.integer())*4);r.take(5);});
    if(v>=37)p[37]=r.take(1);
    if(v>=39)p[39]=r.take(1);
    if(v>=44)p[44]=r.take(4);
    r.end();return p;
}
QByteArray combine(const Parts &old,const Parts &defaults,int version,bool project=false) {
    QByteArray data;
    const std::vector<int> order=project?std::vector<int>{2,6,7,3,16,18,20,37,39,44}:std::vector<int>{0,12,16,17,20};
    for(int key:order)if((key==3?2:key)<=version){auto found=old.find(key);data+=found!=old.end()?found->second:defaults.at(key);}
    return data;
}
std::map<QString,QByteArray> blocks(const Fbm &source) {
    std::map<QString,QByteArray> out;for(auto value:source.inspect()["blocks"].toArray()){
        auto row=value.toObject();out[row["name"].toString()]=source.bytes().mid(row["offset"].toInteger(),row["size"].toInteger());
    }return out;
}
const std::vector<std::tuple<const char*,int,int,int>> calculations{
    {"delft",16,17,2},{"kaper",16,9,1},{"holtr",22,43,2},{"ost",22,52,2},
    {"propeller_task1",22,15,1},{"propeller_task2",22,25,1},{"propeller_task3",22,16,1},
    {"planing",23,13,1},{"reverse_propeller",24,16,1},{"hollenbach",25,43,2},
    {"propeller_task4",26,15,1},{"propeller_task5",30,5,1},{"oortmerssen",33,43,2},
    {"fung",35,43,2},{"maneuvering",38,19,1},{"hydrodynamic_task1",38,11,1},{"rbhs",40,52,2},{"mh",40,52,2}};
}

Fbm emptyDocument(const QJsonObject &c,int v) {
    if(v<0||v>44)bad("Invalid initial FBM version");
    Bytes w;w.string("FREE!ship");w.data+=char(v);w.integer(0);
    w.integer(0);for(bool b:{true,false,true,true,true,true,true,true,true,true})w.flag(b);w.real(1);
    if(v>=12)w.flag(true);
    if(v>=16)w.real(.1f);
    if(v>=17)w.data+=QByteArray(6,1);
    if(v>=20)w.flag(true);
    if(v>=2) {
        w.string({});w.string({});w.real(1);w.real(1);w.real(1);w.flag(false);
        w.real(1.025f);w.real(1);w.flag(true);w.integer(c["initial_underwater_color"].toInt(0x800000));w.integer(0);
        if(v>=6){w.flag(true);w.real(0);}if(v>=7)w.flag(false);w.string({});w.string({});
    }
    if(v>=16){w.integer(1);w.flag(false);} // No image/viewport is fabricated.
    if(v>=18)w.flag(true);
    if(v>=20) {
        for(float n:{0.f,1.f,.1f,0.f})w.real(n);
        w.integer(0);
        w.real(0);w.real(1);w.real(.1f);w.flag(true);
        const std::vector<float> angles{0,2,5,10,15,20,30,40,50,60,70,80,90,100,110,120,130,140,150};
        w.integer(angles.size());for(float n:angles)w.real(n);w.integer(1);w.real(0);w.flag(true);w.real(1);
    }
    if(v>=37)w.flag(false);
    if(v>=39)w.flag(false);
    if(v>=44)w.integer(c["initial_underwater_alpha"].toInt(255));
    w.integer(1);w.string({});w.integer(c["initial_layer_id"].toInt(0));w.integer(c["initial_layer_color"].toInt(0xffff00));
    w.flag(true);w.flag(true);w.flag(false);if(v>=9){w.flag(true);w.flag(true);}if(v>=11){w.real(0);w.real(0);}if(v>=15)w.flag(true);if(v>=21)w.integer(255);w.integer(0);
    w.integer(0);w.integer(0);if(v>=12)w.integer(0);w.integer(0);
    for(int i=0;i<3+(v>=9);++i)w.integer(0);
    if(v>=11)w.integer(0);
    for(auto [name,min,floats,flags]:calculations)if(v>=min) {
        if(QString(name)=="holtr"&&v>=20){w.integer(0);w.integer(0);}
        // FileBuffer pads the stored record to four bytes with $FF. These are
        // padding bytes, not extra Boolean flags or calculation parameters.
        w.data+=QByteArray(floats*4+flags,0)+QByteArray(4-flags,char(0xff));
    }
    // Versions 2.5/2.6 have backgrounds/flows, but no expanded calculators.
    if(v>=20&&v<22){w.integer(0);w.integer(0);}
    if(v>=41)for(int i=0;i<5;++i)w.integer(0);
    if(v>=42)w.integer(0);
    if(v>=43)w.integer(0);
    return Fbm::read(w.data);
}

Fbm promoteDocument(const Fbm &source,int v,const QJsonObject &context) {
    const int old=source.version();if(v<old||v>44)bad("FBM conversion must not discard newer fields");
    if(source.inspect()["opaque_tail_bytes"].toInteger())bad("Cannot convert unknown FBM extensions");
    auto c=context;c["initial_underwater_color"]=context["default_underwater_color"];
    c["initial_underwater_alpha"]=context["default_underwater_alpha"].toInt(context["initial_underwater_alpha"].toInt(255));
    const auto defaults=blocks(emptyDocument(c,44));auto src=blocks(source);
    auto result=blocks(emptyDocument(c,v));result["header"]=src.at("header");result["header"][13]=char(v);
    result["visibility"]=combine(visibility({src.at("visibility")},old),visibility({defaults.at("visibility")},44),v);
    result["project_settings"]=combine(settings({src.at("project_settings")},old),settings({defaults.at("project_settings")},44),v,true);
    Read layers{src.at("layers")};int n=layers.integer();Bytes ls;ls.integer(n);
    for(int i=0;i<n;++i) {
        ls.string(layers.string());ls.data+=layers.take(11);
        auto fields=[&](int minimum,int size,QByteArray def){auto data=old>=minimum?layers.take(size):def;if(v>=minimum)ls.data+=data;};
        fields(9,2,QByteArray(2,1));fields(11,8,QByteArray(8,0));fields(15,1,QByteArray(1,1));
        Bytes alpha;alpha.integer(255);fields(21,4,alpha.data);
    }
    ls.integer(layers.integer());layers.end();result["layers"]=n?ls.data:emptyDocument(context,v).blockBytes("layers");
    Read points{src.at("control_points")};n=points.integer();Bytes ps;ps.integer(n);
    for(int i=0;i<n;++i){ps.data+=points.take(17);auto locked=old>=13?points.take(1):QByteArray(1,0);if(v>=13)ps.data+=locked;}points.end();result["control_points"]=ps.data;
    for(const char *name:{"stations","buttocks","waterlines","diagonals"})if(result.count(name)&&src.count(name)) {
        Read r{src[name]};Bytes w;const int count=r.integer();w.integer(count);
        for(int i=0;i<count;++i) {
            int kind=r.integer();w.integer(kind);auto flag=old>=11?r.take(1):QByteArray(1,0);if(v>=11)w.data+=flag;w.data+=r.take(16);
            const auto begin=r.at;r.take(1);int splines=r.integer();
            for(int j=0;j<splines;++j){int points=r.integer();r.take(qsizetype(points)*(old>=6&&kind>=1&&kind<=3?9:13));}
            w.data+=(old<6&&v>=6)?QByteArray(5,0):r.data.mid(begin,r.at-begin);
        }r.end();result[name]=w.data;src.erase(name);
    }
    if(old>=11&&v>=11) {
        Read r{src.at("markers")};Bytes w;n=r.integer();w.integer(n);
        for(int i=0;i<n;++i){w.data+=r.take(1);auto flag=old>=21?r.take(1):QByteArray(1,0);if(v>=21)w.data+=flag;w.data+=r.take(5);int count=r.integer();w.integer(count);w.data+=r.take(qsizetype(count)*13);}r.end();result["markers"]=w.data;src.erase("markers");
    }
    for(const auto &[name,data]:src)if(result.count(name)&&name!="header"&&name!="visibility"&&name!="project_settings"&&name!="layers"&&name!="control_points")result[name]=data;
    for(auto [name,min,floats,flags]:calculations)if(v>=min&&src.count(name)&&src[name].size()!=(floats+1)*4) {
        const auto data=src[name];if(data.size()<4||data.size()>(floats+1)*4)bad("Invalid short calculator record");
        auto expanded=defaults.at(name);expanded.replace(0,data.size()-4,data.left(data.size()-4));expanded.replace(floats*4,4,data.right(4));result[name]=expanded;
    }
    // A file can explicitly store -1 for an unassigned face. The old loader
    // resolves this to layer zero; retain that effective editable state.
    Read faces{result.at("control_faces")};Bytes fs;n=faces.integer();fs.integer(n);
    for(int i=0;i<n;++i){int count=faces.integer();fs.integer(count);fs.data+=faces.take(qsizetype(count)*4);int layer=faces.integer();fs.integer(std::max(0,layer));fs.data+=faces.take(1);}faces.end();result["control_faces"]=fs.data;
    QByteArray bytes;for(auto row:emptyDocument(c,v).inspect()["blocks"].toArray())bytes+=result.at(row.toObject()["name"].toString());
    return Fbm::read(bytes);
}

QJsonArray documentColors(const Fbm &source) {
    QJsonArray result;for(const auto &l:documentGeometry(source).layers)result.append(l.color);
    if(source.version()>=2){Read r{source.blockBytes("project_settings")};r.string();r.string();r.take(22);result.append(r.integer());}
    return result;
}
QJsonObject documentBindings(const Fbm &source,const QJsonObject &context) {
    auto b=context;b.remove("id");b.remove("ok");b.remove("history_limit_bytes");
    b["format"]="FreeShipDocumentBindings";b["version"]=1;QJsonObject high;
    const auto net=source.controlNet();const auto counts=source.inspect()["counts"].toObject();
    for(const char *key:{"points","edges","faces","layers","curves","flows"}) {
        const QString k=key;int n=k=="points"?net.points.size():k=="edges"?net.edges.size():k=="faces"?net.faces.size():k=="layers"?std::max(1,counts["layers"].toInt()):k=="curves"?net.curves.size():counts["flowlines"].toInt();
        QJsonArray ids;for(int i=1;i<=n;++i)ids.append(i);b[k]=ids;high[k]=n;
    }b["high_water"]=high;
    QJsonArray edges,faces;std::vector<QJsonArray> pe(net.points.size()),lf(high["layers"].toInt());
    for(size_t i=0;i<net.edges.size();++i){const auto &e=net.edges[i];pe[e.a].append(int(i+1));pe[e.b].append(int(i+1));}
    for(size_t i=0;i<net.faces.size();++i)lf.at(std::max(0,net.faces[i].layer)).append(int(i+1));
    for(auto row:pe)edges.append(row);
    for(auto row:lf)faces.append(row);
    b["point_edges"]=edges;b["layer_faces"]=faces;
    auto points=source.controlPoints();PointEditor(points).writeOrder(b);
    int last=0;for(const auto &l:documentGeometry(source).layers)last=std::max(last,l.layerId);b["last_used_layer_id"]=last;
    return b;
}
TopologyEdit loadDocument(const Fbm &source,const QJsonObject &context) {
    // Unknown extension bytes remain in State.saved, never passed to a writer.
    auto parsed=Fbm::read(source.bytes().left(source.inspect()["parsed_bytes"].toInteger()));
    auto working=promoteDocument(parsed,parsed.version(),context);
    auto bindings=documentBindings(working,context);
    // Clear creates layer 0. LoadBinary then calls AddNewLayer before reading
    // each stored layer ID, retaining the larger counter. It is not simply the
    // maximum ID in the file (and must not be applied to a newly built hull).
    qint64 last=0;
    if(parsed.inspect()["counts"].toObject()["layers"].toInt()>0)
        for(const auto &layer:documentGeometry(parsed).layers)last=std::max(last+1,qint64(layer.layerId));
    if(last>std::numeric_limits<int>::max())bad("Loaded layer ID budget exhausted");
    bindings["last_used_layer_id"]=last;
    return {working,bindings,false};
}
}
