// Native FBM structural/persistence regression. GPL-2.0-or-later.
// Run: fbm_codec_tests --output C:/FreeShip26Tests/fbm/report.json [model.fbm | corpus-dir ...]
// The synthetic writer below is test-only: explicit historical field widths
// from FreeShip's SaveBinary routines, with independent coordinate/reference
// expectations. Real supplied files provide separate compatibility evidence.
#include "document/fbm_codec.h"
#include "document/preview_image.h"
#include <QBuffer>
#include <QImage>
#include "document/point_edit.h"
#include "document/transform_edit.h"
#include "document/attribute_edit.h"
#include "document/fbm_text.h"
#include "document/face_tools.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <limits>
#include <stdexcept>
#include <tuple>

using fs::document::Fbm;
namespace {
struct Fixture {
    QByteArray data;
    std::map<QString, qsizetype> offset;
    void mark(const char *name) { offset[QString::fromLatin1(name)] = data.size(); }
    void i(int value) {
        char bytes[4]; qToLittleEndian<qint32>(value, bytes); data.append(bytes, 4);
    }
    void f(float value) {
        quint32 raw; std::memcpy(&raw, &value, 4);
        char bytes[4]; qToLittleEndian<quint32>(raw, bytes); data.append(bytes, 4);
    }
    void b(bool value = false) { data.append(value ? char(1) : char(0)); }
    void s(const QByteArray &text = {}) { i(int(text.size())); data.append(text); }
    void zero(int n) { for (int j = 0; j < n; ++j) f(0); }
    void record(int fields, int flags = 1) {
        zero(fields);
        for (int j = 0; j < flags; ++j) b();
        data.append(QByteArray(4 - flags, char(0xa5))); // Deliberately nonzero padding.
    }
    void jpeg(const char *lengthField) {
        i(1); i(1); mark(lengthField); i(4); data.append(QByteArray::fromHex("ffd8ffd9"));
    }
};

Fixture fixture(int version, bool rich = false, int task2 = 25, int task3 = 16, int planing = 13) {
    Fixture w;
    w.s("FREE!ship"); w.data.append(char(version)); w.i(2);
    w.i(1); for (int j = 0; j < 10; ++j) w.b(); w.f(1);
    if (version >= 12) w.b();
    if (version >= 16) w.f(0.1f);
    if (version >= 17) for (int j = 0; j < 6; ++j) w.b();
    if (version >= 20) w.b();
    if (version >= 2) {
        w.s("Binary fixture"); w.s(QByteArray::fromHex("fffe81"));
        w.mark("dimensions"); w.f(10); w.f(4); w.f(1); w.mark("particulars"); w.b(true); w.f(1.025f); w.f(1);
        w.b(); w.i(0x123456); w.i(0);
        if (version >= 6) { w.mark("default_split"); w.b(true); w.mark("split"); w.f(5); }
        if (version >= 7) w.b();
        w.s("Keep annotation\r\nexactly"); w.s("FreeShip codec regression");
    }
    if (version >= 16) {
        w.i(0); w.b(rich);
        if (rich) w.jpeg("preview_length");
    }
    if (version >= 18) w.b();
    if (version >= 20) {
        w.zero(4); w.i(2); w.f(1); w.f(2); w.zero(3); w.b();
        w.i(1); w.f(0); w.i(1); w.f(0); w.b(); w.f(0);
    }
    if (version >= 37) w.b();
    if (version >= 39) w.b();
    if (version >= 44) w.i(123);
    w.mark("layers"); w.i(1); w.s("Hull"); w.i(7); w.i(0x234567);
    w.b(true); w.b(true); w.b();
    if (version >= 9) { w.b(true); w.b(true); }
    if (version >= 11) { w.f(7.85f); w.f(0.004f); }
    if (version >= 15) w.b(true);
    if (version >= 21) w.i(255);
    w.mark("active_layer"); w.i(0);
    w.mark("point_count"); w.i(3);
    for (int j = 0; j < 3; ++j) {
        if (j == 0) w.mark("first_point");
        w.f(j * 2.5f); w.f(j + 0.25f); w.f(3.5f - j);
        if (j == 0) w.mark("first_vertex_type");
        w.i(j); w.b(j == 1);
        if (version >= 13) w.b(j == 2);
    }
    w.i(3);
    for (int j = 0; j < 3; ++j) {
        if (j == 0) w.mark("first_edge_point");
        w.i(j); w.i((j + 1) % 3); w.b(true); w.b();
    }
    if (version >= 12) { w.i(1); w.i(2); w.i(0); w.i(1); w.b(true); }
    w.i(1); w.i(3); w.i(0); w.i(1); w.i(2);
    w.mark("face_layer"); w.i(0); w.b();
    for (int kind = 1; kind <= (version >= 9 ? 4 : 3); ++kind) {
        w.i(rich ? 1 : 0);
        if (rich) {
            w.i(kind); if (version >= 11) w.b();
            w.f(kind == 1); w.f(kind == 2); w.f(kind >= 3); w.f(-1);
            w.b(true); w.i(1); w.i(2);
            for (int j = 0; j < 2; ++j) {
                w.f(1); w.f(2); if (version < 6 || kind == 4) w.f(3);
                w.b(j == 1);
            }
        }
    }
    if (version >= 11) {
        w.i(rich ? 1 : 0);
        if (rich) {
            w.b(true); if (version >= 21) w.b(); w.b(); w.f(1); w.i(1);
            w.f(1); w.f(2); w.f(3); w.b();
        }
    }
    if (version >= 16) { w.mark("delft"); w.record(17, 2); w.mark("kaper"); w.record(9); }
    if (version >= 20) {
        w.i(rich ? 1 : 0);
        if (rich) {
            w.i(1); w.b(true); w.i(100); w.i(-2); w.i(3); w.f(0.1f);
            w.i(255); w.b(); w.i(0); w.i(3); w.jpeg("background_length");
        }
        w.i(rich ? 1 : 0);
        if (rich) {
            w.f(1); w.f(2); w.i(1); w.b(true); w.b(); w.i(1);
            w.f(3); w.f(4); w.f(5); w.b(true);
        }
    }
    if (version >= 22) {
        w.mark("holtr"); w.record(43, 2); w.mark("ost"); w.record(52, 2); w.record(15); w.record(task2); w.record(task3);
    }
    if (version >= 23) { w.mark("planing"); w.record(planing); }
    if (version >= 24) w.record(16);
    if (version >= 25) { w.mark("hollenbach"); w.record(43, 2); }
    if (version >= 26) w.record(15);
    if (version >= 30) { w.mark("task5"); w.record(5); }
    if (version >= 33) { w.mark("oortmerssen"); w.record(43, 2); }
    if (version >= 35) { w.mark("fung"); w.record(43, 2); }
    if (version >= 38) { w.record(19); w.record(11); }
    if (version >= 40) { w.mark("rbhs"); w.record(52, 2); w.mark("mh"); w.record(52, 2); }
    if (version >= 41) {
        for (int j = 0; j < 4; ++j) {
            w.i(1); if (j == 0) w.mark("named_point"); w.i(0); w.s("persistent name");
        }
        w.i(rich ? 1 : 0);
        if (rich) { w.i(0); w.mark("constraint_reference"); w.i(1); w.i(2); }
    }
    if (version >= 42) {
        w.i(1); w.s("point group"); w.i(2);
        w.mark("group_reference"); w.s("0"); w.s("2");
    }
    if (version >= 43) {
        w.i(rich ? 1 : 0);
        if (rich) { w.i(2); w.mark("anchor_reference"); w.i(1); w.b(true); }
    }
    return w;
}

QByteArray readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > Fbm::MaximumBytes)
        throw std::runtime_error("Cannot read bounded test input");
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError) throw std::runtime_error("Test input read failed");
    return bytes;
}
void putInt(QByteArray &bytes, qsizetype pos, int value) {
    qToLittleEndian<qint32>(value, bytes.data() + pos);
}
void putFloat(QByteArray &bytes, qsizetype pos, float value) {
    quint32 raw; std::memcpy(&raw, &value, 4); qToLittleEndian<quint32>(raw, bytes.data() + pos);
}
void ensure(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
bool rejected(const QByteArray &bytes) {
    try { Fbm::read(bytes); } catch (const std::invalid_argument &) { return true; }
    return false;
}
QString sha256(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
std::map<QString, QByteArray> serializedBlocks(const Fbm &document) {
    std::map<QString, QByteArray> blocks;
    for (const auto &item : document.inspect()["blocks"].toArray()) {
        const auto b = item.toObject();
        blocks.emplace(b["name"].toString(), document.bytes().mid(b["offset"].toInteger(), b["size"].toInteger()));
    }
    return blocks;
}
void reversePoints(fs::document::FbmControlNet &net) {
    const auto remap = [&](int p) { return p < 0 ? p : int(net.points.size()) - 1 - p; };
    std::reverse(net.points.begin(), net.points.end());
    for (auto &p : net.points) {
        p.value.anchor = remap(p.value.anchor);
        p.value.lineA = remap(p.value.lineA); p.value.lineB = remap(p.value.lineB);
    }
    for (auto &e : net.edges) { e.a = remap(e.a); e.b = remap(e.b); }
    for (auto &c : net.curves) for (auto &p : c.points) p = remap(p);
    for (auto &f : net.faces) for (auto &p : f.points) p = remap(p);
}
void preservedUnrelated(const Fbm &before, const Fbm &after, bool keepCaches) {
    const auto old = serializedBlocks(before), now = serializedBlocks(after);
    for (const auto &[name, bytes] : old) {
        if (name.startsWith("control_") || name.endsWith("_names") || name == "linear_constraints" ||
            name == "anchors" || name == "point_groups") continue;
        if (!keepCaches && (name == "stations" || name == "buttocks" || name == "waterlines" ||
                            name == "diagonals" || name == "flowlines")) continue;
        ensure(now.at(name) == bytes, "Unrelated FBM block changed");
    }
}
QJsonObject transformBindings(const Fbm &doc) {
    auto n = doc.controlNet(); QJsonObject bindings, high;
    const std::array<std::pair<const char *, size_t>, 4> counts{{{"points",n.points.size()}, {"edges",n.edges.size()}, {"faces",n.faces.size()}, {"curves",n.curves.size()}}};
    for (const auto &[key, count] : counts) {
        QJsonArray values; for (size_t i = 0; i < count; ++i) values.append(int(i+1));
        bindings[key] = values; high[key] = int(count);
    }
    bindings["high_water"] = high; return bindings;
}
Fbm unrestricted(const Fbm &doc) {
    auto net = doc.controlNet();
    for (auto &p : net.points) { p.value.locked = false; p.value.hardAnchor = false; p.value.anchor = p.value.lineA = p.value.lineB = -1; }
    return doc.withControlNet(net, false);
}
QJsonObject transformRequest(const char *op, QJsonArray vector, QJsonArray ids = {1,2,3}, const char *kind = "point") {
    return {{"method","advanced"},{"operation",op},{"kind",kind},{"ids",ids},{"vector",vector}};
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QString output;
    QStringList models;
    const auto args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--output" && i + 1 < args.size()) output = args[++i];
        else if (args[i] == "--model" && i + 1 < args.size()) models.append(args[++i]);
        else models.append(args[i]);
    }
    const QString testRoot = output.isEmpty() ? QStringLiteral("C:/FreeShip26Tests")
                                              : QFileInfo(output).absolutePath();
    if (!QDir().mkpath(testRoot)) return 3;
    QTemporaryDir scratch(testRoot + "/fbm-codec-XXXXXX");
    if (!scratch.isValid()) return 3;
    QJsonArray cases;
    bool failed = false;
    auto test = [&](const QString &id, const std::function<QJsonObject()> &action) {
        QJsonObject row{{"id", id}, {"status", "pass"}};
        try {
            const auto detail = action();
            for (auto it = detail.begin(); it != detail.end(); ++it) row[it.key()] = it.value();
        } catch (const std::exception &e) {
            row["status"] = "fail"; row["error"] = QString::fromUtf8(e.what()); failed = true;
        }
        cases.append(row);
    };
    test("fbm-text-exact-encodings-and-rejection", [&] {
        using namespace fs::document;
        for (const auto &item : std::vector<std::tuple<QString,QString,QByteArray>>{
            {"Ansi","Plain text",QByteArray("Plain text")},
            {"cp1252",QString::fromUtf8("Café €"),QByteArray::fromHex("436166e92080")},
            {"CP-1251",QString::fromUtf8("Женя"),QByteArray::fromHex("c6e5edff")},
            {"utf8",QString::fromUtf8("船肋"),QByteArray::fromHex("e888b9e8828b")},
            {"utf8bom","A",QByteArray::fromHex("efbbbf41")},
            {"ucs2le",QString::fromUtf8("船A"),QByteArray::fromHex("39824100")},
            {"ucs2be",QString::fromUtf8("船A"),QByteArray::fromHex("82390041")},
            {"cp936",QString::fromUtf8("船"),QByteArray::fromHex("b4ac")}}) {
            const auto &[encoding,text,bytes]=item;
            ensure(encodeFbmText(text,encoding)==bytes,"Stored text bytes changed");
            ensure(decodeFbmText(bytes,encoding)==text,"Stored text decoding changed");
        }
        for (const auto &item : std::vector<std::pair<QString,QString>>{
            {"cp1252",QString::fromUtf8("船")},{"cp1252",QString::fromUtf8("Ж")},
            {"utf8",QString(QChar(0xd800))},{"utf8",QString("A")+QChar(0)},
            {"ucs2le",QString::fromUtf8("😀")},{"unknown","A"}}) {
            bool refused=false;try { encodeFbmText(item.second,item.first); } catch (const std::invalid_argument &) { refused=true; }
            ensure(refused,"Unrepresentable or malformed text was accepted");
        }
        return QJsonObject{};
    });
    test("attribute-blocks-preserve-unrelated-payloads", [&] {
        using namespace fs::document;
        const auto source=Fbm::read(fixture(44,true).data);
        QJsonObject bindings{{"points",QJsonArray{11,22,33}},{"fbm_encoding","cp1252"}};
        auto request=QJsonObject{{"method","modeling"},{"operation","point_properties"},{"ids",QJsonArray{11}},{"vertex_type",0},{"name","Renamed"}};
        const auto renamed=editAttributes(source,bindings,request);
        ensure(renamed.changed,"Point rename was ignored");
        for (const auto &b:source.inspect()["blocks"].toArray()) {
            auto name=b.toObject()["name"].toString();
            if (name!="point_names") ensure(renamed.document.blockBytes(name)==source.blockBytes(name),"Rename changed unrelated block");
        }
        ensure(!editAttributes(renamed.document,bindings,request).changed,"Repeated rename creates history");
        request=QJsonObject{{"method","modeling"},{"operation","add_markers"},
            {"paths",QJsonArray{QJsonArray{QJsonArray{0.1,2,3},QJsonArray{4,5,6}}}}};
        const auto added=editAttributes(source,bindings,request).document;
        const auto oldMarkers=source.blockBytes("markers"),markers=added.blockBytes("markers");
        ensure(qFromLittleEndian<qint32>(markers.constData())==2,"New marker count mismatch");
        ensure(markers.mid(4,oldMarkers.size()-4)==oldMarkers.mid(4),"Existing marker attributes changed");
        ensure(added.blockBytes("visibility")[12]==char(1),"Added markers remain hidden");
        for (const auto &b:source.inspect()["blocks"].toArray()) {
            auto name=b.toObject()["name"].toString();
            if (name!="markers" && name!="visibility") ensure(added.blockBytes(name)==source.blockBytes(name),"Marker edit changed unrelated block");
        }
        auto bad=request;auto paths=bad["paths"].toArray();paths.append(QJsonArray{QJsonArray{0,0,0},QJsonArray{1,2,"bad"}});bad["paths"]=paths;
        bool refused=false;try { editAttributes(source,bindings,bad); } catch (const std::invalid_argument &) { refused=true; }
        ensure(refused && source.bytes()==fixture(44,true).data,"Late marker error changed source");
        return QJsonObject{};
    });
    test("atomic-named-block-validation", [&] {
        const auto source=Fbm::read(fixture(44,true).data);
        for (const auto &changes:std::vector<std::map<QString,QByteArray>>{
            {{"unknown","x"}},{{"control_faces",QByteArray(4,'\0')}},{{"layers",QByteArray(4,'\0')}}}) {
            // Removing all faces alone is structurally legal only if no face
            // names remain; this rich fixture retains index-bearing names.
            bool refused=false;try { source.withBlocks(changes); } catch (const std::invalid_argument &) { refused=true; }
            ensure(refused,"Inconsistent block replacement accepted");
        }
        ensure(source.withBlocks({{"markers",source.blockBytes("markers")}}).bytes()==source.bytes(),"Identical blocks are not a no-op");
        return QJsonObject{};
    });
    test("new-layer-names-use-current-fbm-encoding", [&] {
        using namespace fs::document;
        for (const auto &[encoding,name]:std::vector<std::pair<QString,QString>>{
            {"utf8",QString::fromUtf8("船体")},{"ucs2le",QString::fromUtf8("船体")},
            {"ucs2be",QString::fromUtf8("船体")},{"cp1251",QString::fromUtf8("Корпус")},{"utf8bom",""}}) {
            auto source=Fbm::read(fixture(44,false).data);const auto original=source.blockBytes("layers");
            Fixture layers;layers.i(1);layers.s(encodeFbmText(name,encoding));layers.data.append(original.mid(12));
            source=source.withBlocks({{"layers",layers.data}});
            auto net=source.controlNet();auto face=net.faces[0];face.sourceIndex=-1;net.faces.push_back(face);source=source.withControlNet(net);
            QJsonObject bindings{{"points",QJsonArray{11,22,33}},{"edges",QJsonArray{1,2,3}},{"faces",QJsonArray{101,102}},
                {"layers",QJsonArray{1}},{"curves",QJsonArray{1}},{"fbm_encoding",encoding},
                {"high_water",QJsonObject{{"points",33},{"edges",3},{"faces",102},{"layers",1},{"curves",1}}}};
            auto readNames=[](const Fbm &document,const QString &enc){
                const auto block=document.blockBytes("layers");qsizetype offset=4;QStringList names;
                for (int i=0;i<qFromLittleEndian<qint32>(block.constData());++i) {
                    int size=qFromLittleEndian<qint32>(block.constData()+offset);offset+=4;
                    names.append(decodeFbmText(block.mid(offset,size),enc));offset+=size+26;
                }
                return names;
            };
            const auto grouped=editFaceTools(source,bindings,QJsonObject{{"method","modeling"},{"operation","auto_group_layers"},{"ids",QJsonArray{101}}});
            const auto expected=(name.isEmpty()?QString("Layer"):name)+" 101";
            ensure(readNames(grouped.document,encoding).contains(expected),"Auto-group suffix used raw ASCII in a different encoding");
            const auto assembled=editFaceTools(source,bindings,QJsonObject{{"method","modeling"},{"operation","assemble_faces"}});
            ensure(readNames(assembled.document,encoding)==QStringList{"Patch 1-1","Patch 1-2"},"Patch name encoding changed");
        }
        return QJsonObject{};
    });
    for (int v = 0; v <= 44; ++v) {
        test(QString("synthetic-version-%1").arg(v), [&, v] {
            const auto src = fixture(v, true);
            const auto doc = Fbm::read(src.data);
            ensure(doc.version() == v && doc.precision() == 2, "Version or precision mismatch");
            ensure(doc.bytes() == src.data, "Snapshot bytes differ");
            ensure(doc.controlPoints().size() == 3, "Point count mismatch");
            const auto &p = doc.controlPoints()[1];
            ensure(p.x == 2.5f && p.y == 1.25f && p.z == 2.5f && p.selected,
                   "Point values were not decoded as little-endian Single");
            ensure(doc.controlPoints()[0].coordinateOffset == src.offset.at("first_point"),
                   "Point offset mismatch");
            ensure(doc.controlPoints()[2].locked == (v >= 13), "Lock version gate mismatch");
            ensure(doc.controlPoints()[0].constrained == (v >= 41), "Constraint version gate mismatch");
            ensure(doc.inspect()["parsed_bytes"].toInteger() == src.data.size() &&
                   doc.inspect()["opaque_tail_bytes"].toInteger() == 0, "Incomplete parse");
            return QJsonObject{{"bytes", qint64(src.data.size())}, {"version_name", doc.versionName()}};
        });
    }
    // Thumbnail replacement has one independent allowed byte span. The test
    // writer's offsets, not the production parser's inspection, define it.
    QImage previewPixels(400,300,QImage::Format_RGB32);previewPixels.fill(QColor(40,120,160));
    QByteArray previewJpeg;QBuffer previewBuffer(&previewJpeg);previewBuffer.open(QIODevice::WriteOnly);previewPixels.save(&previewBuffer,"JPEG",90);
    for(int v=0;v<=44;++v)for(bool enabled:{false,true})test(QString("preview-version-%1-enabled-%2").arg(v).arg(enabled),[&,v,enabled]{
        const auto src=fixture(v,enabled);const auto doc=Fbm::read(src.data);
        auto expected=src.data;
        if(v>=16&&enabled){
            const auto begin=src.offset.at("preview_length")-8;Fixture envelope;envelope.i(400);envelope.i(300);envelope.i(previewJpeg.size());envelope.data+=previewJpeg;
            expected.replace(begin,16,envelope.data);
        }
        const auto updated=fs::document::withPreviewImage(doc,previewJpeg);
        ensure(updated.bytes()==expected,"Preview changed bytes outside its JPEG envelope or enabled a disabled/unsupported preview");
        ensure(doc.bytes()==src.data,"Preview mutated source");
        ensure(fs::document::withPreviewImage(updated,previewJpeg).bytes()==expected,"Repeated preview replacement changed bytes");
        return QJsonObject{};
    });
    for(int v=16;v<=44;++v)test(QString("preview-toggle-version-%1").arg(v),[&,v]{
        const auto src=fixture(v,true);const auto doc=Fbm::read(src.data);
        const auto at=src.offset.at("preview_length")-9;
        auto expected=src.data;expected.replace(at,17,QByteArray(1,0));
        const auto disabled=fs::document::withPreviewEnabled(doc,false);
        ensure(disabled.bytes()==expected,"Disabling preview changed non-image bytes");
        ensure(fs::document::withPreviewEnabled(disabled,false).bytes()==expected,"Disabling preview is not idempotent");
        const auto enabled=fs::document::withPreviewEnabled(disabled,true);
        const auto inspection=enabled.inspect()["preview"].toObject();
        auto pixels=QImage::fromData(enabled.bytes().mid(inspection["offset"].toInteger()+12,inspection["bytes"].toInteger()),"JPEG");
        ensure(pixels.size()==QSize(400,300),"Enabled preview has no valid initial image");
        ensure(fs::document::withPreviewEnabled(enabled,false).bytes()==expected,"Re-enabling preview changed other data");
        ensure(fs::document::withPreviewEnabled(doc,true).bytes()==src.data,"Enabling existing preview changed its bytes");
        return QJsonObject{};
    });
    test("preview-rejects-invalid-image-and-preserves-opaque",[&]{
        const auto src=fixture(44,true);const auto doc=Fbm::read(src.data);
        QImage wrong(40,30,QImage::Format_RGB32);wrong.fill(Qt::red);QByteArray small;QBuffer buffer(&small);buffer.open(QIODevice::WriteOnly);wrong.save(&buffer,"JPEG");
        for(const auto& bad:{QByteArray{},QByteArray::fromHex("ffd8ffd9"),previewJpeg.left(previewJpeg.size()/2),QByteArray(600000,'x'),small}){
            bool refused=false;try{fs::document::withPreviewImage(doc,bad);}catch(const std::exception&){refused=true;}ensure(refused&&doc.bytes()==src.data,"Invalid preview accepted or changed source");
        }
        const auto opaque=Fbm::read(src.data+QByteArray("unknown-extension"));
        ensure(fs::document::withPreviewImage(opaque,previewJpeg).bytes()==opaque.bytes(),"Preview rewrote unknown extension offsets");return QJsonObject{};
    });
    const auto latest = fixture(44, true);
    for (int v = 0; v <= 44; ++v) test(QString("control-net-version-%1").arg(v), [&, v] {
        const auto doc = Fbm::read(fixture(v, true).data);
        auto net = doc.controlNet();
        ensure(net.points.size() == 3 && net.edges.size() == 3 && net.faces.size() == 1 &&
               net.curves.size() == (v >= 12 ? 1U : 0U), "Control net extraction count mismatch");
        ensure(net.edges[2].a == 2 && net.edges[2].b == 0 && net.edges[2].crease &&
               net.edges[2].sourceIndex == 2 && net.faces[0].points == std::vector<int>{0, 1, 2} &&
               net.faces[0].layer == 0, "Control net extraction values mismatch");
        ensure(doc.withControlNet(net).bytes() == doc.bytes(), "No-op topology write changed bytes");
        reversePoints(net);
        std::reverse(net.edges.begin(), net.edges.end());
        const auto changed = doc.withControlNet(net, false);
        preservedUnrelated(doc, changed, true);
        const auto decoded = changed.controlNet();
        ensure(decoded.points[0].value.x == 5 && decoded.points[0].value.z == 1.5f &&
               decoded.edges[0].a == 0 && decoded.edges[0].b == 2 &&
               decoded.faces[0].points == std::vector<int>{2, 1, 0}, "Reindexed net mismatch");
        ensure(decoded.points[0].sourceIndex == 0 && decoded.edges[0].sourceIndex == 0,
               "Reopened source indices do not describe new serialized order");
        auto undo = changed.controlNet(); reversePoints(undo);
        std::reverse(undo.edges.begin(), undo.edges.end());
        ensure(changed.withControlNet(undo, false).bytes() == doc.bytes(), "Inverse renumbering changed original bytes");
        auto flags = doc.controlNet(); flags.edges[0].crease = false;
        const auto invalidated = doc.withControlNet(flags);
        preservedUnrelated(doc, invalidated, false);
        const auto before = serializedBlocks(doc), after = serializedBlocks(invalidated);
        for (const auto &name : {"stations", "buttocks", "waterlines", "diagonals"}) {
            if (!before.count(name)) continue;
            const int header = 4 + 4 + (v >= 11 ? 1 : 0) + 16;
            ensure(after.at(name) == before.at(name).first(header) + QByteArray(5, '\0'),
                   "Control net rewrite did not invalidate section cache only");
        }
        if (v >= 20) {
            auto expected = before.at("flowlines").first(16); expected.append(char(0));
            expected.append(before.at("flowlines")[17]); expected.append(QByteArray(4, '\0'));
            ensure(after.at("flowlines") == expected, "Flow cache invalidation changed source or selection");
        }
        ensure(doc.bytes() == fixture(v, true).data, "Control net writer mutated original");
        return QJsonObject{};
    });
    test("control-net-remaps-names-groups-constraints-anchors", [&] {
        const auto doc = Fbm::read(latest.data); auto net = doc.controlNet();
        reversePoints(net); std::reverse(net.edges.begin(), net.edges.end());
        const auto changed = doc.withControlNet(net, false); const auto blocks = serializedBlocks(changed);
        Fixture name; name.i(1); name.i(2); name.s("persistent name");
        ensure(blocks.at("point_names") == name.data && blocks.at("edge_names") == name.data,
               "Names did not follow original entity provenance");
        Fixture group; group.i(1); group.s("point group"); group.i(2); group.s("2"); group.s("0");
        ensure(blocks.at("point_groups") == group.data, "Group members did not follow point mapping");
        Fixture line; line.i(1); line.i(2); line.i(1); line.i(0);
        ensure(blocks.at("linear_constraints") == line.data, "Linear constraint candidate references changed");
        Fixture anchor; anchor.i(1); anchor.i(0); anchor.i(1); anchor.b(true);
        ensure(blocks.at("anchors") == anchor.data, "Anchor candidate references changed");
        return QJsonObject{};
    });
    test("control-net-deletion-clears-references-keeps-empty-group", [&] {
        const auto doc = Fbm::read(latest.data); const auto original = doc.controlNet();
        fs::document::FbmControlNet net;
        net.points.push_back(original.points[1]);
        fs::document::FbmNetPoint added; added.value.x = -7; net.points.push_back(added);
        const auto changed = doc.withControlNet(net, false); const auto blocks = serializedBlocks(changed);
        ensure(changed.controlPoints().size() == 2 && changed.controlPoints()[1].x == -7,
               "New unnamed point not persisted");
        for (const auto &name : {"point_names", "edge_names", "face_names", "curve_names", "linear_constraints", "anchors"})
            ensure(blocks.at(name) == QByteArray(4, '\0'), "Deleted entity reference survived");
        Fixture group; group.i(1); group.s("point group"); group.i(0);
        ensure(blocks.at("point_groups") == group.data, "Empty group/name was removed");
        preservedUnrelated(doc, changed, true);
        return QJsonObject{};
    });
    test("control-net-partial-constraint-and-curve-removal", [&] {
        const auto doc = Fbm::read(latest.data); auto net = doc.controlNet();
        net.points.pop_back(); net.points[0].value.lineB = -1;
        net.edges.resize(1); net.faces.clear();
        // A new curve with the same geometry has no inherited name.
        net.curves[0].sourceIndex = -1;
        const auto changed = doc.withControlNet(net, false); const auto blocks = serializedBlocks(changed);
        ensure(changed.controlPoints()[0].lineA == 1 && changed.controlPoints()[0].lineB == -1,
               "Partial constraint not preserved");
        ensure(blocks.at("curve_names") == QByteArray(4, '\0'), "New curve inherited deleted curve name");
        Fixture group; group.i(1); group.s("point group"); group.i(1); group.s("0");
        ensure(blocks.at("point_groups") == group.data, "Deleted member survived in partial group");
        auto withoutCurve = changed.controlNet(); withoutCurve.curves.clear();
        const auto result = changed.withControlNet(withoutCurve, false);
        ensure(result.controlNet().edges.size() == 1 && result.controlNet().curves.empty(),
               "Curve deletion altered edge collection");
        return QJsonObject{};
    });
    test("control-net-validation-rejects-invalid-candidates-atomically", [&] {
        const auto doc = Fbm::read(latest.data); const auto net = doc.controlNet();
        using Net = fs::document::FbmControlNet;
        const std::vector<std::function<void(Net &)>> faults{
            [](Net &n) { n.points[0].sourceIndex = -2; },
            [](Net &n) { n.points[0].sourceIndex = 3; },
            [](Net &n) { n.points[0].sourceIndex = 1; },
            [](Net &n) { n.edges[0].sourceIndex = 1; },
            [](Net &n) { n.curves.push_back(n.curves[0]); },
            [](Net &n) { n.faces.push_back(n.faces[0]); },
            [](Net &n) { n.edges[0].a = -1; },
            [](Net &n) { n.edges[0].b = 3; },
            [](Net &n) { n.curves[0].points[0] = 3; },
            [](Net &n) { n.faces[0].points[0] = -1; },
            [](Net &n) { n.faces[0].points.resize(2); },
            [](Net &n) { n.faces[0].layer = 1; },
            [](Net &n) { n.points[0].value.lineA = 3; },
            [](Net &n) { n.points[0].value.anchor = -2; },
            [](Net &n) { n.points[0].value.vertexType = 4; },
            [](Net &n) { n.points[0].value.x = std::numeric_limits<float>::infinity(); }};
        for (const auto &fault : faults) {
            auto invalid = net; fault(invalid); bool threw = false;
            try { doc.withControlNet(invalid); } catch (const std::invalid_argument &) { threw = true; }
            ensure(threw, "Invalid topology candidate accepted");
        }
        ensure(doc.bytes() == latest.data, "Rejected topology candidate mutated source");
        return QJsonObject{{"invalid_candidates", int(faults.size())}};
    });
    test("control-net-old-version-fields-and-opaque-tail-rejected", [&] {
        using Net = fs::document::FbmControlNet;
        const std::vector<std::pair<int, std::function<void(Net &)>>> faults{
            {11, [](Net &n) { fs::document::FbmNetCurve c; c.points = {0, 1}; n.curves.push_back(c); }},
            {12, [](Net &n) { n.points[0].value.locked = true; }},
            {40, [](Net &n) { n.points[0].value.lineA = 1; }},
            {42, [](Net &n) { n.points[0].value.anchor = 1; }},
            {42, [](Net &n) { n.points[0].value.hardAnchor = true; }}};
        for (const auto &[version, fault] : faults) {
            const auto doc = Fbm::read(fixture(version).data); auto net = doc.controlNet(); fault(net);
            bool threw = false; try { doc.withControlNet(net); } catch (const std::invalid_argument &) { threw = true; }
            ensure(threw, "Unrepresentable field silently dropped or version upgraded");
        }
        const auto opaque = Fbm::read(latest.data + "unknown extension");
        bool threw = false; try { opaque.withControlNet(opaque.controlNet()); } catch (const std::invalid_argument &) { threw = true; }
        ensure(threw, "Unknown extension accepted for topology rewrite");
        return QJsonObject{};
    });
    for (int v=0;v<=44;++v) test(QString("coordinate-write-version-%1").arg(v),[&,v]{
        const auto src=fixture(v,true);const auto doc=Fbm::read(src.data);
        std::vector<fs::document::FbmPosition> positions;
        for(const auto& p:doc.controlPoints())positions.push_back({p.x,p.y,p.z});
        ensure(doc.withPointPositions(positions).bytes()==src.data,"No-op changed caches or padding");
        positions[0]={-12.25f,3.125f,8.5f};const auto changed=doc.withPointPositions(positions);
        ensure(doc.bytes()==src.data,"Coordinate writer mutated source");
        ensure(changed.controlPoints()[0].x==-12.25f&&changed.controlPoints()[0].y==3.125f&&changed.controlPoints()[0].z==8.5f,"Coordinates not persisted");
        const auto before=doc.inspect()["blocks"].toArray(),after=changed.inspect()["blocks"].toArray();
        ensure(before.size()==after.size(),"Writer changed block inventory");
        for(qsizetype i=0;i<before.size();++i){
            auto a=before[i].toObject(),b=after[i].toObject();auto name=a["name"].toString();
            auto old=doc.bytes().mid(a["offset"].toInteger(),a["size"].toInteger());
            auto now=changed.bytes().mid(b["offset"].toInteger(),b["size"].toInteger());
            ensure(name==b["name"].toString(),"Writer reordered blocks");
            if(name=="control_points"){
                auto expected=old;auto at=doc.controlPoints()[0].coordinateOffset-a["offset"].toInteger();
                Fixture w;w.f(-12.25f);w.f(3.125f);w.f(8.5f);expected.replace(at,12,w.data);
                ensure(now==expected,"Unrelated point bytes changed");
            }else if(name=="stations"||name=="buttocks"||name=="waterlines"||name=="diagonals"){
                const auto header=4+4+(v>=11?1:0)+16;
                ensure(now==old.first(header)+QByteArray(5,'\0'),"Section definition changed or cache retained");
            }else if(name=="flowlines"){
                QByteArray expected=old.first(16);expected.append(char(0));expected.append(old[17]);expected.append(QByteArray(4,'\0'));
                ensure(now==expected,"Flow source changed or cache retained");
            }else ensure(now==old,"Unrelated serialized field changed");
        }
        return QJsonObject{{"bytes_before",doc.bytes().size()},{"bytes_after",changed.bytes().size()}};
    });
    test("native-point-validation-and-atomicity",[&]{
        const auto doc=Fbm::read(fixture(40,true).data);
        const QJsonObject binding{{"points",QJsonArray{10,20,30}}};
        const auto move=[](int id,QJsonArray p){return QJsonObject{{"point_id",id},{"position",p}};};
        const QJsonObject valid{{"method","move_points"},{"moves",QJsonArray{move(10,{1,2,3}),move(20,{4,5,6})}}};
        auto edited=fs::document::editPoints(doc,binding,valid);
        ensure(edited.controlPoints()[0].x==1&&edited.controlPoints()[1].z==6,"Native batch failed");
        for(auto moves:{QJsonArray{move(10,{1,2,3}),move(10,{4,5,6})},QJsonArray{move(10,{1,2,3}),move(30,{4,5,6})},QJsonArray{move(10,{1,2,3}),move(999,{4,5,6})},QJsonArray{move(10,{1e20,2,3})}}){
            bool threw=false;try{fs::document::editPoints(doc,binding,QJsonObject{{"method","move_points"},{"moves",moves}});}catch(const std::invalid_argument&){threw=true;}
            ensure(threw,"Invalid batch accepted");
        }
        ensure(doc.bytes()==fixture(40,true).data,"Failed batch mutated input");
        return QJsonObject{};
    });
    test("native-anchor-chain-and-hard-anchor",[&]{
        auto src=fixture(44,false);auto p0=src.offset.at("first_point");src.data[p0+2*18+17]=0;
        src.data.chop(4);src.i(2);src.i(1);src.i(0);src.b();src.i(2);src.i(1);src.b(true);
        const auto doc=Fbm::read(src.data);const QJsonObject binding{{"points",QJsonArray{10,20,30}}};
        auto changed=fs::document::editPoints(doc,binding,{{"method","move_point"},{"point_id",10},{"position",QJsonArray{3,4.25,8.5}}});
        for(size_t i=0;i<3;++i){const auto& old=doc.controlPoints()[i];const auto& now=changed.controlPoints()[i];
            ensure(now.x==old.x+3&&now.y==old.y+4&&now.z==old.z+5,"Anchor offset drift");}
        bool threw=false;try{fs::document::editPoints(doc,binding,{{"method","move_point"},{"point_id",30},{"position",QJsonArray{1,2,3}}});}catch(const std::invalid_argument&){threw=true;}
        ensure(threw,"Hard anchored direct move accepted");return QJsonObject{};
    });
    test("every-truncated-prefix-rejected", [&] {
        for (qsizetype n = 0; n < latest.data.size(); ++n)
            ensure(rejected(latest.data.first(n)), "A truncated current-version prefix was accepted");
        return QJsonObject{{"prefixes", qint64(latest.data.size())}};
    });
    for (const auto &item : std::initializer_list<std::pair<const char *, int>>{
             {"active_layer", 1}, {"point_count", -1}, {"point_count", 2147483647},
             {"first_vertex_type", 4}, {"first_edge_point", 3}, {"face_layer", 1},
             {"named_point", 3}, {"constraint_reference", -2}, {"anchor_reference", 3},
             {"preview_length", -1}, {"background_length", 2147483647}}) {
        test(QString("reject-%1-%2").arg(item.first).arg(item.second), [&, item] {
            auto bad = latest.data; putInt(bad, latest.offset.at(item.first), item.second);
            ensure(rejected(bad), "Malformed field accepted"); return QJsonObject{};
        });
    }
    test("reject-invalid-header-version-boolean-and-nan", [&] {
        auto bad = latest.data; bad[4] = 'X'; ensure(rejected(bad), "Wrong signature accepted");
        bad = latest.data; bad[13] = char(255); ensure(rejected(bad), "Future version accepted");
        bad = latest.data; bad[22] = char(2); ensure(rejected(bad), "Invalid Boolean accepted");
        bad = latest.data; putInt(bad, latest.offset.at("first_point"), 0x7fc00001);
        ensure(rejected(bad), "NaN control point accepted");
        bad = latest.data; bad[latest.offset.at("group_reference") + 4] = '9';
        ensure(rejected(bad), "Out-of-range textual group index accepted");
        return QJsonObject{};
    });
    test("nullable-unassigned-face-layer-preserved", [&] {
        auto data = latest.data; putInt(data, latest.offset.at("face_layer"), -1);
        ensure(Fbm::read(data).bytes() == data, "Documented -1 face layer not preserved");
        return QJsonObject{};
    });
    for (const auto &layout : std::initializer_list<std::tuple<int, int, int, int>>{
             {22, 25, 15, 13}, {22, 15, 15, 13}, {32, 25, 16, 11}, {32, 25, 16, 9}}) {
        test(QString("legacy-layout-%1-%2-%3-%4").arg(std::get<0>(layout)).arg(std::get<1>(layout))
                 .arg(std::get<2>(layout)).arg(std::get<3>(layout)), [&, layout] {
            const auto src = fixture(std::get<0>(layout), false, std::get<1>(layout),
                                     std::get<2>(layout), std::get<3>(layout));
            const auto doc = Fbm::read(src.data);
            ensure(doc.bytes() == src.data && !doc.inspect()["legacy_calculation_layout"].toString().isEmpty(),
                   "Historical calculator layout not preserved/reported");
            return QJsonObject{{"inspection", doc.inspect()}};
        });
    }
    test("legacy-invalid-task5-preserved-with-warning", [&] {
        auto src = fixture(32, false, 25, 16, 9);
        putInt(src.data, src.offset.at("task5"), 0x7fc00000);
        const auto doc = Fbm::read(src.data);
        ensure(doc.bytes() == src.data && doc.inspect()["warnings"].toArray().size() >= 2,
               "Invalid historical calculator payload was rewritten or not reported");
        return QJsonObject{};
    });
    test("opaque-extension-and-atomic-unchanged-save", [&] {
        QByteArray data = latest.data + QByteArray::fromHex("004e4557455854ff00010203");
        const auto doc = Fbm::read(data);
        ensure(doc.inspect()["opaque_tail_bytes"].toInteger() == 12, "Opaque suffix count mismatch");
        const QString dest = scratch.path() + QString::fromUtf8("/原样-Женя.fbm");
        Fbm::read(fixture(43).data).writeAtomic(dest);
        doc.writeAtomic(dest);
        ensure(readFile(dest) == data, "Atomic save changed file bytes");
        const QByteArray before = readFile(dest);
        bool failedSave = false;
        try { doc.writeAtomic(dest + "/child.fbm"); } catch (const std::exception &) { failedSave = true; }
        ensure(failedSave && readFile(dest) == before, "Failed save damaged existing target");
        return QJsonObject{{"sha256", sha256(data)}};
    });
    test("raw-data-input-is-owned-after-read", [&] {
        QByteArray storage = latest.data;
        const auto doc = Fbm::read(QByteArray::fromRawData(storage.constData(), storage.size()));
        storage[4] = 'X';
        ensure(doc.bytes() == latest.data, "Snapshot aliases externally mutable raw storage");
        return QJsonObject{};
    });

    for (int version = 0; version <= 44; ++version) {
        test(QString("native-transform-version-%1").arg(version), [&, version] {
            auto seed = fixture(version, true);
            if (version >= 6) seed.data[seed.offset.at("default_split")] = 0;
            if (version >= 2) seed.data[seed.offset.at("particulars")] = 0;
            const std::map<QString,int> widths{{"delft",17},{"kaper",9},{"holtr",43},{"ost",52},{"planing",13},
                {"hollenbach",43},{"oortmerssen",43},{"fung",43},{"rbhs",52},{"mh",52}};
            for (const auto &[name,width] : widths) if (seed.offset.count(name))
                for (int i = 0; i < width; ++i) putFloat(seed.data, seed.offset.at(name) + 4*i, .375f + i*.125f);
            const auto source = unrestricted(Fbm::read(seed.data)); const auto bindings = transformBindings(source);
            std::vector<fs::document::FbmPosition> points;
            for (const auto &p : source.controlPoints()) points.push_back({p.x,p.y,p.z});
            const auto oldFields = source.transformFields();
            ensure(source.withTransformFields(points,oldFields).bytes() == source.bytes(), "Untouched affine metadata rewrote file");
            auto request = transformRequest("translate",{.125,.25,-.5}); request["adjust_markers"] = true;
            const auto moved = fs::document::editTransforms(source,bindings,request);
            const auto fields = moved.transformFields();
            for (size_t i = 0; i < points.size(); ++i) {
                const auto &p = moved.controlPoints()[i];
                ensure(p.x==points[i].x+.125f && p.y==points[i].y+.25f && p.z==points[i].z-.5f, "Translation coordinate formula differs");
                ensure(p.vertexType==source.controlPoints()[i].vertexType && p.selected==source.controlPoints()[i].selected, "Translation changed point attributes");
            }
            ensure(fields.dimensions.x==oldFields.dimensions.x && fields.particularsSet==oldFields.particularsSet, "Translation changed project dimensions");
            if (version >= 6) ensure(fields.split==5.125f, "Explicit split did not translate");
            for (size_t i = 0; i < fields.sections.size(); ++i) {
                auto expected = oldFields.sections[i]; expected[3] -= expected[0]*.125f+expected[1]*.25f+expected[2]*-.5f;
                ensure(fields.sections[i]==expected, "Whole-model section translation differs");
            }
            if (!fields.markers.empty()) ensure(fields.markers[0][0].x==1.125f && fields.markers[0][0].y==2.25f && fields.markers[0][0].z==2.5f, "Markers did not translate");
            ensure(fields.calculations==oldFields.calculations, "Translation changed resistance parameters");
            const auto noOp = fs::document::editTransforms(source,bindings,transformRequest("scale",{1,1,1}));
            ensure(noOp.bytes()==source.bytes(), "Unit scale changed raw bytes/caches");
            const auto partial = fs::document::editTransforms(source,bindings,transformRequest("scale",{2,3,4},{1}));
            const auto originalBlocks = serializedBlocks(source), partialBlocks = serializedBlocks(partial);
            for (const auto &[name,bytes] : originalBlocks) {
                if (name=="control_points" || name=="stations" || name=="buttocks" || name=="waterlines" || name=="diagonals" || name=="flowlines") continue;
                ensure(partialBlocks.at(name)==bytes, "Partial transform modified unrelated fields");
            }
            request = transformRequest("scale",{1.125,.75,1.5}); request["adjust_markers"] = true;
            if (version < 2) {
                bool rejected = false; try { fs::document::editTransforms(source,bindings,request); } catch (const std::invalid_argument &) { rejected=true; }
                ensure(rejected, "Unrepresentable pre-1.2 dimension update accepted");
            } else {
                const auto scaled = fs::document::editTransforms(source,bindings,request); const auto now = scaled.transformFields();
                ensure(now.dimensions.x==11.25f && now.dimensions.y==3 && now.dimensions.z==1.5f && now.particularsSet, "Whole-scale dimensions/setter flag differ");
                ensure(now.sections==oldFields.sections, "Scale changed section planes (legacy scaling is disabled)");
                if (version >= 6) ensure(now.split==5.625f, "Explicit split did not scale");
                if (!now.markers.empty()) ensure(now.markers[0][0].x==1.125f && now.markers[0][0].y==1.5f && now.markers[0][0].z==4.5f, "Marker scale differs");
                const auto blocks = serializedBlocks(scaled);
                for (const auto &[name,values] : oldFields.calculations) {
                    auto expected = values; const bool shortRecord = name=="kaper" || name=="planing";
                    std::map<int,std::vector<float>> factors;
                    if (shortRecord) factors={{0,{1.5f}},{1,{1.125f}},{2,{.75f}},{4,{1.125f,.75f,1.5f}},{6,{1.125f,1.5f}}};
                    else factors={{3,{.75f}},{5,{1.125f,.75f,1.5f}},{6,{1.5f}},{7,{1.5f}},{8,{1.125f}},
                        {9,{1.125f,1.5f}},{11,{1.125f}},{12,{1.125f}},{13,{1.125f,1.5f}},{15,{1.125f,1.5f}},
                        {16,(name=="hollenbach" || name=="oortmerssen" || name=="fung") ? std::vector<float>{1.125f} : std::vector<float>{1.125f,.75f}}};
                    for (const auto &[index,sequence] : factors) for (float factor : sequence) expected[index] *= factor;
                    ensure(now.calculations.at(name)==expected, "One or more resistance parameters scaled incorrectly");
                    ensure(blocks.at(name).right(4)==originalBlocks.at(name).right(4), "Calculator flags/padding changed");
                }
                for (const auto &[name,bytes] : originalBlocks) {
                    if (name=="control_points" || name=="project_settings" || name=="markers" || name=="stations" || name=="buttocks" ||
                        name=="waterlines" || name=="diagonals" || name=="flowlines" || oldFields.calculations.count(name)) continue;
                    ensure(blocks.at(name)==bytes, "Whole scale modified unrelated annotations/JPEG/propeller settings");
                }
            }
            ensure(source.bytes()==unrestricted(Fbm::read(seed.data)).bytes(), "Transform mutated its source");
            return QJsonObject{};
        });
    }
    test("native-transform-validation-and-selection", [&] {
        const auto source = unrestricted(Fbm::read(fixture(44,true).data)); const auto bindings = transformBindings(source);
        const auto reject = [&](QJsonObject request, const Fbm &document) {
            bool failed = false; try { fs::document::editTransforms(document,bindings,request); } catch (const std::invalid_argument &) { failed=true; }
            ensure(failed, "Invalid transform accepted");
        };
        for (const auto &request : std::vector<QJsonObject>{transformRequest("translate",{1,2,3},{}), transformRequest("translate",{1,2,3},{1,1}),
                transformRequest("translate",{1,2,3},{999}),transformRequest("scale",{1,0,1}), transformRequest("rotate",{360001,0,0}),
                transformRequest("translate",{1,2,"x"}),transformRequest("translate",{1,2,3},{1},"marker")}) reject(request,source);
        auto request = transformRequest("translate",{1,2,3},{1}); request["adjust_markers"] = true; reject(request,source);
        request = transformRequest("translate",{1,2,3}); request["adjust_markers"] = 1; reject(request,source);
        request = transformRequest("scale",{1,1,1}); request["pivot"] = QJsonArray{0,1}; reject(request,source);
        reject(transformRequest("translate",{1,0,0}), Fbm::read(source.bytes()+"future extension"));
        reject(transformRequest("translate",{1,0,0}), Fbm::read(fixture(44,true).data));
        for (const auto *kind : {"edge","face","curve"}) {
            const auto moved = fs::document::editTransforms(source,bindings,transformRequest("translate",{1,0,0},{1},kind));
            ensure(moved.controlPoints()[0].x==source.controlPoints()[0].x+1 && moved.controlPoints()[1].x==source.controlPoints()[1].x+1, "Entity selection did not include endpoints");
            ensure(moved.controlPoints()[2].x==source.controlPoints()[2].x+(QString(kind)=="face" ? 1 : 0), "Entity selection included wrong points");
        }
        // Independent right-angle orientation: the original row matrix rotates
        // +Z clockwise. Signed negative Y must remain; pivot translation is signed.
        auto rotate = transformRequest("rotate",{0,0,90},{1}); rotate["pivot"] = QJsonArray{0,0,0};
        const auto turned = fs::document::editTransforms(source,bindings,rotate);
        ensure(turned.controlPoints()[0].x==.25f && std::abs(turned.controlPoints()[0].y)<1e-15f && turned.controlPoints()[0].z==3.5f, "Rotation orientation differs");
        rotate["ids"] = QJsonArray{2}; const auto negative = fs::document::editTransforms(source,bindings,rotate);
        ensure(negative.controlPoints()[1].y==-2.5f, "Rotation incorrectly clamped negative Y");
        std::vector<fs::document::FbmPosition> positions;
        for (const auto &p : source.controlPoints()) positions.push_back({p.x,p.y,p.z});
        positions[0]={10,0,0}; const auto axial=source.withPointPositions(positions);
        const auto quarter=fs::document::editTransforms(axial,bindings,transformRequest("rotate",{0,0,90},{1}));
        // Captured from the shipped original Windows FPC generic RTL. The
        // separate Python dual-service tests verify the entire exact model.
        ensure(quarter.controlPoints()[0].x==6.123233998228043e-16f && quarter.controlPoints()[0].y==-10,
               "Quarter-turn FPC Double argument reduction differs");
        const auto extreme=fs::document::editTransforms(axial,bindings,transformRequest("rotate",{360000,-360000,270},{1}));
        ensure(extreme.controlPoints()[0].x==-1.8369700935892946e-15f && extreme.controlPoints()[0].y==10 &&
               extreme.controlPoints()[0].z==-2.666614325186334e-12f,"Large-angle FPC argument reduction differs");
        return QJsonObject{};
    });
    test("native-transform-dimension-epsilon-and-opaque-calculator-nan", [&] {
        auto seed = fixture(44,false); seed.data[seed.offset.at("particulars")] = 0;
        putFloat(seed.data,seed.offset.at("dimensions"),1e-7f);
        putInt(seed.data,seed.offset.at("delft")+4*4,0x7fc12345); // Cp is not scaled.
        const auto source = unrestricted(Fbm::read(seed.data)); const auto bindings = transformBindings(source);
        const auto changed = fs::document::editTransforms(source,bindings,transformRequest("scale",{1.25,1,1}));
        ensure(changed.transformFields().dimensions.x==1e-7f && !changed.transformFields().particularsSet, "Project dimension setter epsilon changed");
        const auto old = serializedBlocks(source), now = serializedBlocks(changed);
        ensure(now.at("delft").mid(16,4)==old.at("delft").mid(16,4), "Untouched calculator NaN payload changed");
        return QJsonObject{};
    });

    test("native-whole-scale-keeps-semantic-history-with-identical-bytes", [&] {
        auto source = unrestricted(Fbm::read(fixture(44,false).data));
        auto net = source.controlNet(); net.points.resize(1); net.edges.clear(); net.curves.clear(); net.faces.clear();
        net.points[0].value.x = net.points[0].value.y = net.points[0].value.z = 0;
        source = source.withControlNet(net); auto fields = source.transformFields();
        fields.dimensions = {1e-7f,1e-7f,1e-7f}; fields.particularsSet = false;
        source = source.withTransformFields({{0,0,0}},fields); const auto bindings=transformBindings(source);
        bool changed = false;
        const auto scaled = fs::document::editTransforms(source,bindings,transformRequest("scale",{1.25,1.25,1.25},{1}),&changed);
        ensure(changed && scaled.bytes()==source.bytes(), "Non-unit whole scale lost its semantic history step");
        fs::document::editTransforms(source,bindings,transformRequest("translate",{0,0,0},{1}),&changed);
        ensure(!changed, "Zero translation gained a semantic history step");
        return QJsonObject{};
    });
    test("restore-calculator-payload-without-geometry-transfer", [&] {
        auto seed = fixture(44,true); putFloat(seed.data,seed.offset.at("delft")+4*4,1.375f);
        const auto original = Fbm::read(seed.data);
        auto derivedBytes = seed.data; putFloat(derivedBytes,seed.offset.at("delft")+4*4,0);
        putFloat(derivedBytes,seed.offset.at("first_point"),17.25f);
        for (const auto &v : original.inspect()["blocks"].toArray()) {
            const auto block = v.toObject(); const auto name = block["name"].toString();
            if (name=="delft" || name=="kaper" || name.startsWith("propeller_")) {
                const auto offset=block["offset"].toInteger(),size=block["size"].toInteger();
                derivedBytes[offset+size-1] = char(0xff);
            }
        }
        const auto derived = Fbm::read(derivedBytes), restored = derived.withCalculationRecordsFrom(original);
        ensure(restored.controlPoints()[0].x==17.25f, "Calculation restoration transferred original geometry");
        const auto before=serializedBlocks(original), after=serializedBlocks(restored);
        for (const auto &[name,bytes] : before) if (name!="control_points") ensure(after.at(name)==bytes,"Original calculator bytes were lost");
        const auto shortOriginal = Fbm::read(fixture(22,false,15,15).data);
        const auto expanded = derived.withCalculationRecordsFrom(shortOriginal); const auto shortBlocks=serializedBlocks(shortOriginal), expandedBlocks=serializedBlocks(expanded);
        for (const auto *name : {"propeller_task2","propeller_task3"}) {
            ensure(expandedBlocks.at(name).left(shortBlocks.at(name).size()-4)==shortBlocks.at(name).chopped(4),"Short calculator prefix not restored");
            ensure(expandedBlocks.at(name).right(4)==shortBlocks.at(name).right(4),"Short calculator flags/padding not restored");
        }
        return QJsonObject{};
    });

    QStringList paths;
    for (const auto &arg : models) {
        if (QFileInfo(arg).isDir()) {
            QDirIterator it(arg, {"*.fbm", "*.FBM"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) paths.append(it.next());
        } else paths.append(arg);
    }
    paths.removeDuplicates();
    std::sort(paths.begin(), paths.end());
    for (const auto &path : paths) {
        test("supplied-file:" + path, [&] {
            const auto bytes = readFile(path);
            const auto doc = Fbm::read(bytes);
            const QString dest = scratch.path() + "/corpus-roundtrip.fbm";
            doc.writeAtomic(dest);
            ensure(readFile(dest) == bytes, "Supplied-file roundtrip differs");
            ensure(readFile(path) == bytes, "Source file changed");
            bool coordinateWrite=false, topologyWrite=false;
            if(!doc.controlPoints().empty()&&doc.inspect()["opaque_tail_bytes"].toInteger()==0){
                std::vector<fs::document::FbmPosition> positions;
                for(const auto& p:doc.controlPoints())positions.push_back({p.x,p.y,p.z});
                positions[0].z+=0.125f;
                const auto edited=doc.withPointPositions(positions);edited.writeAtomic(dest);
                const auto reopened=Fbm::read(readFile(dest));
                ensure(reopened.controlPoints()[0].z==positions[0].z,"Real-file coordinate write failed");
                ensure(doc.bytes()==bytes&&readFile(path)==bytes,"Real-file edit changed source");
                coordinateWrite=true;
                auto net=doc.controlNet();
                ensure(doc.withControlNet(net,false).bytes()==bytes,"Real-file unchanged net rewrote bytes");
                reversePoints(net);
                const auto reordered=doc.withControlNet(net,false);
                const auto reopenedNet=Fbm::read(reordered.bytes()).controlNet();
                ensure(reopenedNet.points.size()==net.points.size()&&reopenedNet.edges.size()==net.edges.size()&&
                       reopenedNet.faces.size()==net.faces.size()&&reopenedNet.curves.size()==net.curves.size(),
                       "Real-file reordered net count mismatch");
                for(std::size_t i=0;i<net.points.size();++i)
                    ensure(reopenedNet.points[i].value.x==net.points[i].value.x&&
                           reopenedNet.points[i].value.y==net.points[i].value.y&&
                           reopenedNet.points[i].value.z==net.points[i].value.z,"Real-file point reindex changed position");
                for(std::size_t i=0;i<net.edges.size();++i)
                    ensure(reopenedNet.edges[i].a==net.edges[i].a&&reopenedNet.edges[i].b==net.edges[i].b,
                           "Real-file edge reindex failed");
                preservedUnrelated(doc,reordered,true);
                ensure(doc.bytes()==bytes&&readFile(path)==bytes,"Real-file net rewrite changed source");
                topologyWrite=true;
            }
            return QJsonObject{{"fixture", QFileInfo(path).absoluteFilePath()}, {"sha256", sha256(bytes)},
                               {"inspection", doc.inspect()},{"coordinate_write",coordinateWrite},{"control_net_write",topologyWrite}};
        });
    }
    const QJsonObject report{{"status", failed ? "fail" : "pass"}, {"cases", cases},
        {"supplied_files", paths.size()},
        {"limitations", QJsonArray{"Structural parsing and bounded replacement-image validation; original image payloads stay opaque. No whole-format re-encoder, topology algorithm or geometry rebuild.",
                                    "Historical short calculation layouts can be indistinguishable from some truncations.",
                                    "No power-loss, desktop-dialog or arbitrary historical-writer acceptance."}}};
    const auto json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (!output.isEmpty()) {
        QSaveFile file(output);
        if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit()) return 3;
        std::cout << QJsonDocument(QJsonObject{{"status", failed ? "fail" : "pass"},
            {"cases", cases.size()}, {"supplied_files", paths.size()}, {"report", output}})
                         .toJson(QJsonDocument::Compact).constData() << '\n';
    } else std::cout << json.constData();
    return failed ? 1 : 0;
}
