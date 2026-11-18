// GPL-2.0-or-later. Operation semantics: bridge/ModelingEdits.inc and
// ModelTools.inc, Plus ControlPointGroup and Marker/FreeSpline persistence.
// Every mutation is built privately; a late validation error commits nothing.
#include "attribute_edit.h"
#include "fbm_text.h"
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) { throw std::invalid_argument(message); }
int integer(QJsonValue value, int low, int high) {
    double n=value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(n) || n<low || n>high || std::floor(n)!=n) bad("Invalid bounded integer");
    return int(n);
}
bool flag(const QJsonObject &r, const char *key) {
    if (!r[key].isBool()) bad("Invalid Boolean field");
    return r[key].toBool();
}
struct Reader {
    QByteArray bytes; qsizetype at=0;
    void require(qsizetype size) { if (size<0 || size>bytes.size()-at) bad("Truncated attribute block"); }
    int integer() { require(4); int n=qFromLittleEndian<qint32>(bytes.constData()+at);at+=4;return n; }
    QByteArray string() { int size=integer();require(size);auto v=bytes.mid(at,size);at+=size;return v; }
};
struct Writer {
    QByteArray bytes;
    void append(const QByteArray &data) { if (data.size()>Fbm::MaximumBytes-bytes.size()) bad("Attribute block exceeds limit");bytes.append(data); }
    void integer(int n) { char b[4];qToLittleEndian<qint32>(n,b);append(QByteArray(b,4)); }
    void flag(bool v) { append(QByteArray(1,v?char(1):char(0))); }
    void real(float v) { quint32 bits;std::memcpy(&bits,&v,4);char b[4];qToLittleEndian<quint32>(bits,b);append(QByteArray(b,4)); }
    void string(const QByteArray &v) { integer(int(v.size()));append(v); }
};
struct Group { QByteArray name; std::vector<int> points; std::vector<QByteArray> spelling; };
std::vector<Group> groups(const Fbm &source) {
    if (source.version()<42) return {};
    Reader r{source.blockBytes("point_groups")};int count=r.integer();std::vector<Group> result;
    for (int i=0;i<count;++i) {
        Group g;g.name=r.string();int n=r.integer();
        for (int j=0;j<n;++j) { auto s=r.string();g.points.push_back(s.toInt());g.spelling.push_back(s); }
        result.push_back(std::move(g));
    }
    return result;
}
QByteArray groupBytes(const std::vector<Group> &groups) {
    Writer w;w.integer(int(groups.size()));
    for (const auto &g:groups) { w.string(g.name);w.integer(int(g.points.size()));
        for (size_t i=0;i<g.points.size();++i) w.string(i<g.spelling.size()?g.spelling[i]:QByteArray::number(g.points[i])); }
    return w.bytes;
}
QString text(const QJsonObject &request) {
    if (!request["name"].isString()) bad("Invalid name");
    const auto name=request["name"].toString();
    if (name.toUtf8().size()>4096 || name.contains(QChar(0)) || !name.isValidUtf16()) bad("Invalid name");
    return name;
}
std::vector<int> selected(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request) {
    const auto ids=bindings["points"].toArray();QHash<int,int> indices;
    if (size_t(ids.size())!=source.controlPoints().size()) bad("Point binding count mismatch");
    for (int i=0;i<ids.size();++i) {
        const int id=integer(ids[i],1,2000000);
        if (indices.contains(id)) bad("Duplicate point binding");
        indices.insert(id,i);
    }
    if (!request["ids"].isArray()) bad("Missing point selection");
    const auto values=request["ids"].toArray();
    if (values.empty() || values.size()>100000) bad("Invalid selection size");
    QSet<int> seen;std::vector<int> result;
    for (const auto &v:values) {
        const int id=integer(v,1,2000000);
        if (!indices.contains(id) || seen.contains(id)) bad("Unknown or duplicate point ID");
        seen.insert(id);result.push_back(indices[id]);
    }
    return result;
}
QString pointName(const Fbm &source, int point, const QString &encoding) {
    if (source.version()<41) return {};
    Reader r{source.blockBytes("point_names")};int count=r.integer();QString name;
    for (int i=0;i<count;++i) { int index=r.integer();auto value=r.string();if (index==point) name=decodeFbmText(value,encoding); }
    return name;
}
}

bool nativeAttributeCommand(const QJsonObject &request) {
    static const QSet<QString> operations{"point_properties","create_group","update_group","delete_group","add_markers","delete_markers"};
    return request["method"]=="modeling" && operations.contains(request["operation"].toString());
}

TopologyEdit editAttributes(const Fbm &source, const QJsonObject &bindings, const QJsonObject &request) {
    if (!nativeAttributeCommand(request)) bad("Unsupported native attribute operation");
    const auto operation=request["operation"].toString();
    const auto encoding=bindings["fbm_encoding"].toString();
    if (operation=="point_properties") {
        const auto ids=selected(source,bindings,request);
        if (ids.size()!=1) bad("Select one point to edit properties");
        const auto name=text(request);const auto encoded=encodeFbmText(name,encoding);
        const int type=integer(request["vertex_type"],0,3), index=ids[0];
        auto net=source.controlNet();auto &p=net.points[index];
        const bool typeChanged=p.value.vertexType!=type, nameChanged=pointName(source,index,encoding)!=name;
        if (!typeChanged && !nameChanged) return {source,bindings,false};
        if (typeChanged && (p.value.locked || p.value.constrained)) bad("Operation touches a locked or constrained point");
        p.value.vertexType=type;
        if (nameChanged) p.name=encoded;
        return {source.withControlNet(net,typeChanged),bindings,true};
    }
    if (operation=="create_group" || operation=="update_group" || operation=="delete_group") {
        auto list=groups(source);int index=-1;
        if (operation!="create_group") index=integer(request["index"],0,int(list.size())-1);
        auto net=source.controlNet();
        if (operation=="delete_group") {
            for (int p:list[index].points) net.points[p].value.selected=false;
            list.erase(list.begin()+index);
        } else {
            const auto ids=selected(source,bindings,request);if (ids.size()<2) bad("A group needs at least two points");
            const auto name=text(request);if (name.isEmpty()) bad("Group name must not be empty");
            const auto encoded=encodeFbmText(name,encoding);const bool locked=flag(request,"locked");
            if (index>=0) {
                const auto &old=list[index];bool oldLocked=!old.points.empty();
                for (int p:old.points) oldLocked &= net.points[p].value.locked;
                auto a=old.points,b=ids;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
                if (decodeFbmText(old.name,encoding)==name && oldLocked==locked && a==b) return {source,bindings,false};
            } else { index=int(list.size());list.push_back({}); }
            list[index]={encoded,ids,{}};
            for (int p:ids) if (locked || operation=="update_group") net.points[p].value.locked=locked;
        }
        auto candidate=source.withControlNet(net,false);
        return {candidate.withBlocks({{"point_groups",groupBytes(list)}}),bindings,true};
    }
    QByteArray stored;
    if (source.version()>=11) stored=source.blockBytes("markers");
    else { Writer w;w.integer(0);stored=w.bytes; }
    const int existing=qFromLittleEndian<qint32>(stored.constData());
    if (operation=="delete_markers") {
        if (!existing) return {source,bindings,false};
        Writer w;w.integer(0);return {source.withBlocks({{"markers",w.bytes}}),bindings,true};
    }
    if (!request["paths"].isArray()) bad("Invalid marker paths");
    const auto paths=request["paths"].toArray();
    if (paths.empty() || paths.size()>4096 || paths.size()+existing>4096) bad("Invalid marker count");
    Writer w;w.append(stored);qToLittleEndian<qint32>(existing+int(paths.size()),w.bytes.data());
    int pointCount=0;
    for (const auto &value:paths) {
        if (!value.isArray()) bad("Invalid marker path");const auto path=value.toArray();
        pointCount+=int(path.size());if (path.size()<2 || pointCount>100000) bad("Invalid marker point count");
        w.flag(true);if (source.version()>=21) w.flag(false);
        w.flag(false);w.real(0.10f);w.integer(int(path.size()));
        for (const auto &item:path) {
            if (!item.isArray() || item.toArray().size()!=3) bad("Invalid marker coordinate");
            for (const auto &coordinate:item.toArray()) {
                double n=coordinate.toDouble();
                if (!coordinate.isDouble() || !std::isfinite(n) || std::abs(n)>1e9) bad("Invalid marker coordinate");
                w.real(float(n));
            }
            w.flag(false);
        }
    }
    auto visibility=source.blockBytes("visibility");visibility[12]=char(1); // ShowMarkers after model-view + eight flags.
    return {source.withBlocks({{"markers",w.bytes},{"visibility",visibility}}),bindings,true};
}
}
