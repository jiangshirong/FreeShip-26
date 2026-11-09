// FreeShip_26 native FBM document codec. GPL-2.0-or-later; see COPYING.
// Format traversal adapted from FreeShip-Plus-Lazarus/Units/FreeShip.inc,
// FreeFileBuffer.pas, FreeVersionUnit.pas and the corresponding *LoadBinary /
// *SaveBinary routines. Original FREE!ship authors: Martijn van Engeland
// (2005), V. F. Timoshenko (2007-2010), Mark Malakanov (2015 onward).
// This deliberately retains the original serialization instead of rebuilding
// it: Pascal records contain padding and historic, sometimes unset parameters.
#include "fbm_codec.h"

#include <QByteArrayView>
#include <QJsonArray>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace fs::document {
namespace {

// The file stores this enum's one-byte ordinal, not the displayed version.
enum Version {
    fv100, fv110, fv120, fv130, fv140, fv150, fv160, fv165, fv170, fv180,
    fv190, fv191, fv195, fv198, fv200, fv201, fv210, fv220, fv230, fv240,
    fv250, fv260, fv270, fv280, fv290, fv295, fv296, fv297, fv298, fv300,
    fv302, fv303, fv305, fv309, fv310, fv313, fv314, fv317, fv327, fv332,
    fv335, fv421, fv430, fv462, fv500
};
constexpr const char *VersionNames[] = {
    "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.65", "1.7", "1.8",
    "1.9", "1.91", "1.95", "1.98", "2.0", "2.01", "2.1", "2.2", "2.3", "2.4",
    "2.5", "2.6", "2.7+", "2.8+", "2.94+", "2.95+", "2.96+", "2.97+", "2.98+", "3.0+",
    "3.02+", "3.03+", "3.08+", "3.09+", "3.12+", "3.13+", "3.16+", "3.27+", "3.3+", "3.34+",
    "3.4", "4.2", "4.3", "4.6.2", "5.0"
};

class Reader {
public:
    explicit Reader(const QByteArray &data, qsizetype position = 0) : data_(data), position_(position) {
        if (position < 0 || position > data.size()) fail("block", "invalid start offset");
    }
    qsizetype position() const noexcept { return position_; }
    qsizetype remaining() const noexcept { return data_.size() - position_; }

    [[noreturn]] void fail(const char *field, const char *why) const {
        throw std::invalid_argument(QString("Invalid FBM %1 at byte %2: %3")
                                        .arg(QString::fromLatin1(field)).arg(position_)
                                        .arg(QString::fromLatin1(why)).toStdString());
    }
    void require(qsizetype size, const char *field) const {
        if (size < 0 || size > remaining())
            fail(field, "truncated or negative length");
    }
    void skip(qsizetype size, const char *field) {
        require(size, field);
        position_ += size;
    }
    unsigned byte(const char *field) {
        require(1, field);
        return static_cast<unsigned char>(data_[position_++]);
    }
    quint32 bits(const char *field) {
        require(4, field);
        const auto result = qFromLittleEndian<quint32>(data_.constData() + position_);
        position_ += 4;
        return result;
    }
    int integer(const char *field) {
        const quint32 raw = bits(field);
        qint32 value;
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
    int ranged(const char *field, int low, int high) {
        const int value = integer(field);
        if (value < low || value > high)
            fail(field, "value outside allowed range");
        return value;
    }
    int count(const char *field, qsizetype minimumItemBytes) {
        const int n = integer(field);
        if (n < 0 || n > Fbm::MaximumCollectionItems || minimumItemBytes <= 0 ||
            qsizetype(n) > remaining() / minimumItemBytes)
            fail(field, "invalid, oversized or truncated collection");
        return n;
    }
    int index(const char *field, int total, bool nullable = false) {
        const int i = integer(field);
        if ((i == -1 && nullable) || (i >= 0 && i < total))
            return i;
        fail(field, "reference outside its collection");
    }
    bool flag(const char *field) {
        const auto value = byte(field);
        if (value > 1)
            fail(field, "invalid Boolean");
        return value != 0;
    }
    void flags(int count, const char *field) {
        require(count, field);
        for (int i = 0; i < count; ++i)
            flag(field);
    }
    float rawReal(const char *field) {
        const auto raw = bits(field);
        float value;
        static_assert(sizeof(value) == 4 && std::numeric_limits<float>::is_iec559);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
    float real(const char *field) {
        const float value = rawReal(field);
        if (!std::isfinite(value))
            fail(field, "non-finite coordinate or setting");
        return value;
    }
    void reals(int n, const char *field) {
        require(qsizetype(n) * 4, field);
        for (int i = 0; i < n; ++i)
            real(field);
    }
    void realArray(const char *field) {
        reals(count(field, 4), field);
    }
    QByteArrayView string(const char *field) {
        const int n = integer(field);
        require(n, field);
        const QByteArrayView result(data_.constData() + position_, n);
        position_ += n;
        return result;
    }
    QJsonObject jpeg(const char *field) {
        // Only the envelope is parsed. The encoded data is preserved, never
        // decompressed/recompressed by this persistence layer.
        const auto offset = position_;
        const int width = ranged("JPEG width", 0, std::numeric_limits<int>::max());
        const int height = ranged("JPEG height", 0, std::numeric_limits<int>::max());
        const int size = integer(field);
        skip(size, field);
        return {{"offset", qint64(offset)}, {"bytes", size}, {"width", width}, {"height", height}};
    }
    // Fixed Pascal records are floats, Boolean flags, then alignment padding.
    // Some distributed historical models contain uninitialised calculator data.
    // Do not rewrite those bytes or treat them as verified numeric results.
    bool calculationRecord(int floatCount, int flagCount, const char *field) {
        require(qsizetype(floatCount) * 4 + 4, field);
        bool valid = true;
        for (int i = 0; i < floatCount; ++i)
            valid &= (bits(field) & 0x7f800000U) != 0x7f800000U;
        for (int i = 0; i < flagCount; ++i)
            valid &= byte(field) <= 1;
        skip(4 - flagCount, "Pascal record padding");
        return valid;
    }

private:
    const QByteArray &data_;
    qsizetype position_ = 0;
};

struct Parsed {
    int version = 0, precision = 0;
    std::vector<FbmControlPoint> points;
    QJsonObject info;
    struct Cache { qsizetype offset, size; QByteArray empty; };
    std::vector<Cache> caches;
};

class Parser {
public:
    explicit Parser(const QByteArray &bytes) : r_(bytes), bytes_(bytes) {}

    Parsed parse() {
        if (r_.string("signature") != QByteArrayView("FREE!ship", 9))
            r_.fail("signature", "expected FREE!ship binary header");
        v_ = int(r_.byte("version"));
        if (v_ > fv500)
            r_.fail("version", "unsupported file version");
        result_.version = v_;
        result_.precision = r_.ranged("precision", 0, 3);
        block("header", 0);

        parseVisibility();
        parseSettings();
        parseSurface();
        sections("stations");
        sections("buttocks");
        sections("waterlines");
        if (v_ >= fv180)
            sections("diagonals");
        if (v_ >= fv191)
            markers();
        if (v_ >= fv210) {
            calculation("delft", 17, 2);
            calculation("kaper", 9, 1);
        }
        if (v_ >= fv250) {
            backgrounds();
            flowlines();
        }
        if (v_ >= fv270)
            calculations();
        if (v_ >= fv421) {
            names("point_names", pointCount());
            names("edge_names", edges_);
            names("face_names", faces_);
            names("curve_names", curves_);
            constraints();
        }
        if (v_ >= fv430)
            groups();
        if (v_ >= fv462)
            anchors();

        const auto parsed = r_.position(), opaque = r_.remaining();
        if (opaque) {
            r_.skip(opaque, "opaque trailing extension");
            block("opaque_tail", parsed);
            warnings_.append("Opaque trailing bytes are preserved; their semantics are not validated.");
        }
        result_.info = {{"format", "FREE!ship binary"}, {"version", v_},
                        {"version_name", QString::fromLatin1(VersionNames[v_])},
                        {"precision", result_.precision}, {"file_bytes", qint64(bytes_.size())},
                        {"parsed_bytes", qint64(parsed)}, {"opaque_tail_bytes", qint64(opaque)},
                        {"counts", counts_}, {"blocks", blocks_}, {"warnings", warnings_}, {"preview", preview_},
                        {"legacy_calculation_layout", legacyLayout_},
                        {"byte_preserving", true}, {"geometry_rebuilt", false},
                        {"image_payloads_decoded", false}};
        return std::move(result_);
    }

private:
    int pointCount() const { return int(result_.points.size()); }
    void block(const char *name, qsizetype begin, int count = -1) {
        QJsonObject b{{"name", QString::fromLatin1(name)}, {"offset", qint64(begin)},
                      {"size", qint64(r_.position() - begin)}};
        if (count >= 0) {
            b["count"] = count;
            counts_[QString::fromLatin1(name)] = count;
        }
        blocks_.append(b);
    }
    void parseVisibility() {
        const auto begin = r_.position();
        r_.ranged("model view", 0, 1);
        r_.flags(10, "visibility");
        r_.real("curvature scale");
        if (v_ >= fv195) r_.flag("show control curves");
        if (v_ >= fv210) r_.real("cursor increment");
        if (v_ >= fv220) r_.flags(6, "hydrostatic visibility");
        if (v_ >= fv250) r_.flag("show flowlines");
        block("visibility", begin);
    }
    void parseSettings() {
        const auto begin = r_.position();
        // SaveBinary gates the early project fields, although the modern
        // Pascal LoadBinary no longer contains all the pre-1.65 gates.
        if (v_ >= fv120) {
            r_.string("project name");
            r_.string("project designer");
            r_.reals(3, "project dimensions");
            r_.flag("main particulars set");
            r_.reals(2, "water density and appendage coefficient");
            r_.flag("shade underwater");
            r_.integer("underwater color");
            r_.ranged("project units", 0, 1);
            if (v_ >= fv160) {
                r_.flag("default split section");
                r_.real("split section position");
            }
            if (v_ >= fv165) r_.flag("disable model check");
            r_.string("project comment");
            r_.string("file created by");
        }
        if (v_ >= fv210) {
            r_.ranged("hydrostatic coefficients", 0, 1);
            const auto flagOffset=r_.position();
            const bool enabled=r_.flag("save preview");
            if (enabled) preview_=r_.jpeg("preview JPEG");
            preview_["enabled"]=enabled;preview_["flag_offset"]=qint64(flagOffset);
        }
        if (v_ >= fv230) r_.flag("simplify intersections");
        if (v_ >= fv250) {
            r_.reals(4, "hydrostatic draft settings");
            r_.realArray("crosscurve displacements");
            r_.reals(3, "displacement range");
            r_.flag("use displacement increments");
            r_.realArray("heel angles");
            r_.realArray("stability trims");
            r_.flag("free trim");
            r_.real("vertical center of gravity");
        }
        if (v_ >= fv317) r_.flag("enable model auto move");
        if (v_ >= fv332) r_.flag("enable Bonjean SAC");
        if (v_ >= fv500) r_.ranged("underwater color alpha", 0, 255);
        block("project_settings", begin);
    }
    void parseSurface() {
        auto begin = r_.position();
        const int storedLayers = r_.count("layer count", 15);
        layers_ = std::max(1, storedLayers); // Zero keeps Pascal's default layer.
        for (int i = 0; i < storedLayers; ++i) {
            r_.string("layer description");
            r_.integer("layer ID");
            r_.integer("layer color");
            r_.flags(3, "layer visibility and symmetry");
            if (v_ >= fv180) r_.flags(2, "layer intersection and hydrostatic flags");
            if (v_ >= fv191) r_.reals(2, "layer density and thickness");
            if (v_ >= fv201) r_.flag("layer in linesplan");
            if (v_ >= fv260) r_.ranged("layer alpha", 0, 255);
        }
        r_.index("active layer", layers_);
        block("layers", begin, storedLayers);

        begin = r_.position();
        const int n = r_.count("control point count", v_ >= fv198 ? 18 : 17);
        result_.points.reserve(std::size_t(n));
        for (int i = 0; i < n; ++i) {
            FbmControlPoint p;
            p.coordinateOffset = r_.position();
            p.x = r_.real("point X");
            p.y = r_.real("point Y");
            p.z = r_.real("point Z");
            p.vertexType = r_.ranged("vertex type", 0, 3);
            p.selected = r_.flag("point selected");
            if (v_ >= fv198) p.locked = r_.flag("point locked");
            result_.points.push_back(p);
        }
        block("control_points", begin, n);

        begin = r_.position();
        edges_ = r_.count("control edge count", 10);
        for (int i = 0; i < edges_; ++i) {
            r_.index("edge start point", n);
            r_.index("edge end point", n);
            r_.flags(2, "edge crease and selected");
        }
        block("control_edges", begin, edges_);

        if (v_ >= fv195) {
            begin = r_.position();
            curves_ = r_.count("control curve count", 5);
            for (int i = 0; i < curves_; ++i) {
                const int m = r_.count("curve point count", 4);
                for (int j = 0; j < m; ++j) r_.index("curve point", n);
                r_.flag("curve selected");
            }
            block("control_curves", begin, curves_);
        }
        begin = r_.position();
        faces_ = r_.count("control face count", 21);
        for (int i = 0; i < faces_; ++i) {
            const int m = r_.count("face point count", 4);
            if (m < 3 || m > n) r_.fail("face point count", "invalid face size");
            for (int j = 0; j < m; ++j) r_.index("face point", n);
            r_.index("face layer", layers_, true); // -1 denotes unassigned.
            r_.flag("face selected");
        }
        block("control_faces", begin, faces_);
    }
    void sections(const char *name) {
        const auto begin = r_.position();
        const int n = r_.count(name, v_ >= fv191 ? 26 : 25);
        for (int i = 0; i < n; ++i) {
            const int kind = r_.ranged("intersection kind", 0, 4);
            if (v_ >= fv191) r_.flag("section curvature visible");
            r_.reals(4, "intersection plane");
            const auto cacheBegin = r_.position();
            r_.flag("section cache built");
            const int splines = r_.count("intersection splines", 4);
            const int dimensions = v_ >= fv160 && kind >= 1 && kind <= 3 ? 2 : 3;
            for (int j = 0; j < splines; ++j) {
                const int m = r_.count("intersection spline points", dimensions * 4 + 1);
                for (int k = 0; k < m; ++k) {
                    r_.reals(dimensions, "intersection cached coordinate");
                    r_.flag("intersection knuckle");
                }
            }
            result_.caches.push_back({cacheBegin, r_.position() - cacheBegin, QByteArray(5, '\0')});
        }
        block(name, begin, n);
    }
    void splinePoints(const char *field) {
        const int n = r_.count(field, 13);
        for (int i = 0; i < n; ++i) {
            r_.reals(3, field);
            r_.flag("spline knuckle");
        }
    }
    void markers() {
        const auto begin = r_.position();
        const int n = r_.count("marker count", v_ >= fv260 ? 11 : 10);
        for (int i = 0; i < n; ++i) {
            r_.flag("marker visible");
            if (v_ >= fv260) r_.flag("marker selected");
            r_.flag("marker curvature visible");
            r_.real("marker curvature scale");
            splinePoints("marker points");
        }
        block("markers", begin, n);
    }
    void backgrounds() {
        const auto begin = r_.position();
        const int n = r_.count("background image count", 46);
        for (int i = 0; i < n; ++i) {
            r_.ranged("background view", 0, 3);
            r_.flag("background visible");
            r_.integer("background JPEG quality");
            r_.integer("background origin X");
            r_.integer("background origin Y");
            r_.real("background scale");
            r_.integer("background blending");
            r_.flag("background transparent");
            r_.integer("background transparent color");
            r_.integer("background tolerance");
            r_.jpeg("background JPEG");
        }
        block("background_images", begin, n);
    }
    void flowlines() {
        const auto begin = r_.position();
        const int n = r_.count("flowline count", 18);
        for (int i = 0; i < n; ++i) {
            r_.reals(2, "flowline projection point");
            r_.ranged("flowline projection view", 0, 3);
            const auto cacheBegin = r_.position();
            r_.flag("flowline built");
            const bool selected = r_.flag("flowline selected");
            splinePoints("flowline points");
            QByteArray empty(6, '\0'); empty[1] = selected ? 1 : 0;
            result_.caches.push_back({cacheBegin, r_.position() - cacheBegin, empty});
        }
        block("flowlines", begin, n);
    }
    void calculation(const char *name, int floats, int flags, bool strict = false,
                     bool tolerateInvalid = false) {
        const auto begin = r_.position();
        if (!r_.calculationRecord(floats, flags, name)) {
            if (strict && !tolerateInvalid)
                r_.fail(name, "invalid historical calculation record");
            warnings_.append(QString("%1 has unset or invalid calculator fields; original bytes are preserved.")
                                 .arg(QString::fromLatin1(name)));
        }
        block(name, begin);
    }
    void calculations() {
        int task2 = 25, task3 = 16, planing = 13;
        bool legacy = false;
        // FreeFileBuffer.PrepareLegacyCalculations documents the distributed
        // Delphi variants. EOF alone cannot prove a short file was intentional;
        // preserve and report that ambiguity. Do not guess arbitrary layouts.
        if (v_ <= fv305) {
            int expected = 176 + 212 + 64 + 104 + 68;
            if (v_ >= fv280) expected += 56;
            if (v_ >= fv290) expected += 68;
            if (v_ >= fv295) expected += 176;
            if (v_ >= fv296) expected += 64;
            if (v_ >= fv302) expected += 24;
            const auto missing = expected - r_.remaining();
            if (v_ == fv270 && (missing == 4 || missing == 44)) {
                task3 = 15;
                if (missing == 44) task2 = 15;
                legacyLayout_ = missing == 44 ? "Task2: 15 fields; Task3: 15 fields" : "Task3: 15 fields";
                legacy = true;
            } else if (v_ >= fv280 && (missing == 8 || missing == 16)) {
                planing -= int(missing / 4);
                legacyLayout_ = QString("Planing: %1 fields").arg(planing);
                legacy = true;
            } else if (missing > 0) {
                r_.fail("legacy calculation tail", "unrecognised or incomplete layout");
            }
            if (legacy)
                warnings_.append("Historical short calculator layout accepted; without a checksum some truncations are indistinguishable.");
        }
        calculation("holtr", 43, 2, legacy);
        calculation("ost", 52, 2, legacy);
        calculation("propeller_task1", 15, 1, legacy);
        calculation("propeller_task2", task2, 1, legacy);
        calculation("propeller_task3", task3, 1, legacy);
        if (v_ >= fv280) calculation("planing", planing, 1, legacy);
        if (v_ >= fv290) calculation("reverse_propeller", 16, 1, legacy);
        if (v_ >= fv295) calculation("hollenbach", 43, 2, legacy);
        if (v_ >= fv296) calculation("propeller_task4", 15, 1, legacy);
        if (v_ >= fv302) calculation("propeller_task5", 5, 1, legacy, true);
        if (v_ >= fv309) calculation("oortmerssen", 43, 2);
        if (v_ >= fv313) calculation("fung", 43, 2);
        if (v_ >= fv327) {
            calculation("maneuvering", 19, 1);
            calculation("hydrodynamic_task1", 11, 1);
        }
        if (v_ >= fv335) {
            calculation("rbhs", 52, 2);
            calculation("mh", 52, 2);
        }
    }
    void names(const char *name, int total) {
        const auto begin = r_.position();
        const int n = r_.count(name, 8);
        for (int i = 0; i < n; ++i) {
            r_.index(name, total);
            r_.string(name);
        }
        block(name, begin, n);
    }
    void markConstrained(int i) {
        if (i >= 0) result_.points[std::size_t(i)].constrained = true;
    }
    void constraints() {
        const auto begin = r_.position();
        const int n = r_.count("linear constraint count", 12);
        for (int i = 0; i < n; ++i) {
            const int point = r_.index("constrained point", pointCount());
            auto &p = result_.points[point];
            p.lineA = r_.index("constraint reference A", pointCount(), true);
            p.lineB = r_.index("constraint reference B", pointCount(), true);
            markConstrained(point); markConstrained(p.lineA); markConstrained(p.lineB);
        }
        block("linear_constraints", begin, n);
    }
    void groups() {
        const auto begin = r_.position();
        const int n = r_.count("point group count", 8);
        for (int i = 0; i < n; ++i) {
            r_.string("point group name");
            const int m = r_.count("group point count", 4);
            for (int j = 0; j < m; ++j) {
                const auto text = r_.string("group point index");
                // SaveBinary writes IntToStr, not binary integers, in TStrings.
                // A bounded spelling avoids allocating a malicious huge string.
                if (text.size() > 32) r_.fail("group point index", "oversized integer spelling");
                bool ok = false;
                const int id = text.toByteArray().toInt(&ok, 10);
                if (!ok || id < 0 || id >= pointCount())
                    r_.fail("group point index", "reference outside control points");
            }
        }
        block("point_groups", begin, n);
    }
    void anchors() {
        const auto begin = r_.position();
        const int n = r_.count("anchor count", 9);
        for (int i = 0; i < n; ++i) {
            const int point = r_.index("anchored point", pointCount());
            auto &p = result_.points[point];
            p.anchor = r_.index("anchor reference", pointCount(), true);
            p.hardAnchor = r_.flag("hard anchor");
            markConstrained(point); markConstrained(p.anchor);
        }
        block("anchors", begin, n);
    }

    Reader r_;
    const QByteArray &bytes_;
    int v_ = 0, layers_ = 0, edges_ = 0, curves_ = 0, faces_ = 0;
    Parsed result_;
    QJsonObject counts_, preview_;
    QJsonArray blocks_, warnings_;
    QString legacyLayout_;
};

struct BlockSpan { qsizetype offset = 0, size = 0; int count = 0; };
using BlockMap = std::map<QString, BlockSpan>;
BlockMap blockMap(const QJsonObject &inspection) {
    BlockMap result;
    for (const auto &item : inspection["blocks"].toArray()) {
        const auto block = item.toObject();
        result.emplace(block["name"].toString(), BlockSpan{block["offset"].toInteger(),
                       block["size"].toInteger(), block["count"].toInt()});
    }
    return result;
}

[[noreturn]] void badNet(const char *message) {
    throw std::invalid_argument(QString("Invalid FBM control net: %1")
                                    .arg(QString::fromLatin1(message)).toStdString());
}

// Every generated block and the final assembled file are bounded independently.
// Preserve strings as raw bytes: FBM text has no universal encoding declaration.
class Writer {
public:
    QByteArray data;
    void append(QByteArrayView bytes) {
        if (bytes.size() > Fbm::MaximumBytes - data.size()) badNet("serialized data exceeds 512 MiB");
        data.append(bytes.data(), bytes.size());
    }
    void integer(int value) {
        char out[4]; qToLittleEndian<qint32>(value, out); append(QByteArrayView(out, 4));
    }
    void real(float value) {
        quint32 bits; std::memcpy(&bits, &value, 4);
        char out[4]; qToLittleEndian<quint32>(bits, out); append(QByteArrayView(out, 4));
    }
    void flag(bool value) { const char byte = value ? 1 : 0; append(QByteArrayView(&byte, 1)); }
    void string(QByteArrayView value) { integer(int(value.size())); append(value); }
    void countAtStart(int count) { qToLittleEndian<qint32>(count, data.data()); }
};

template<class T>
std::vector<int> sourceMap(const std::vector<T> &items, int oldCount) {
    if (items.size() > std::size_t(Fbm::MaximumCollectionItems)) badNet("too many entities");
    std::vector<int> result(std::size_t(oldCount), -1);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int source = items[i].sourceIndex;
        if (source < -1 || source >= oldCount) badNet("sourceIndex outside original collection");
        if (source >= 0) {
            if (result[std::size_t(source)] != -1) badNet("duplicate sourceIndex");
            result[std::size_t(source)] = int(i);
        }
    }
    return result;
}

bool identityMap(const std::vector<int> &mapping, std::size_t newSize) {
    if (mapping.size() != newSize) return false;
    for (std::size_t i = 0; i < mapping.size(); ++i)
        if (mapping[i] != int(i)) return false;
    return true;
}

void netIndex(int index, std::size_t total, bool nullable = false) {
    if (index == -1 && nullable) return;
    if (index < 0 || std::size_t(index) >= total) badNet("reference outside candidate collection");
}

QByteArray mappedNames(const QByteArray &bytes, BlockSpan block, const std::vector<int> &mapping,
                      const std::vector<FbmNetPoint> *points = nullptr) {
    Reader r(bytes, block.offset);
    const int count = r.count("name count", 8);
    Writer w; w.integer(0); int kept = 0;
    for (int i = 0; i < count; ++i) {
        const int old = r.index("named entity", int(mapping.size()));
        const auto name = r.string("entity name");
        if (mapping[std::size_t(old)] >= 0 && (!points || !(*points)[mapping[std::size_t(old)]].name)) {
            w.integer(mapping[std::size_t(old)]); w.string(name); ++kept;
        }
    }
    if (points) for (std::size_t i = 0; i < points->size(); ++i) {
        const auto &name = (*points)[i].name;
        if (name && !name->isEmpty()) { w.integer(int(i)); w.string(*name); ++kept; }
    }
    w.countAtStart(kept);
    return std::move(w.data);
}

QByteArray mappedGroups(const QByteArray &bytes, BlockSpan block, const std::vector<int> &mapping) {
    Reader r(bytes, block.offset);
    const int count = r.count("point group count", 8);
    Writer w; w.integer(count);
    for (int i = 0; i < count; ++i) {
        w.string(r.string("point group name"));
        const int members = r.count("point group members", 4);
        Writer list; list.integer(0); int kept = 0;
        for (int j = 0; j < members; ++j) {
            const auto text = r.string("point group member");
            bool ok = false;
            const int old = text.toByteArray().toInt(&ok, 10);
            if (!ok || old < 0 || std::size_t(old) >= mapping.size()) badNet("invalid stored group reference");
            const int mapped = mapping[std::size_t(old)];
            if (mapped < 0) continue;
            // Retain the original integer spelling when the index is unchanged.
            if (mapped == old) list.string(text);
            else list.string(QByteArray::number(mapped));
            ++kept;
        }
        list.countAtStart(kept); w.append(list.data);
        // TFreeSubdivisionControlPoint.Delete calls RemoveControlPoint only;
        // an empty group and its name remain in the original application.
    }
    return std::move(w.data);
}

struct TransformLayout {
    FbmTransformFields fields;
    qsizetype dimensions = -1, particulars = -1, split = -1;
    std::vector<qsizetype> sections;
    std::vector<std::vector<qsizetype>> markers;
    std::map<QString, qsizetype> calculations;
};
TransformLayout transformLayout(const QByteArray &bytes, int version, const QJsonObject &inspection) {
    const auto blocks = blockMap(inspection);
    TransformLayout out;
    if (version >= fv120) {
        Reader r(bytes, blocks.at("project_settings").offset);
        r.string("project name"); r.string("project designer");
        out.dimensions = r.position(); out.fields.hasDimensions = true;
        out.fields.dimensions = {r.real("length"), r.real("beam"), r.real("draft")};
        out.particulars = r.position(); out.fields.particularsSet = r.flag("particulars set");
        r.skip(8 + 1 + 4 + 4, "unchanged project fields");
        if (version >= fv160) {
            out.fields.hasSplit = true; out.fields.defaultSplit = r.flag("default split section");
            out.split = r.position(); out.fields.split = r.real("split section");
        }
    }
    for (const auto *name : {"stations", "buttocks", "waterlines", "diagonals"}) {
        const auto found = blocks.find(name); if (found == blocks.end()) continue;
        Reader r(bytes, found->second.offset); const int count = r.count(name, version >= fv191 ? 26 : 25);
        for (int i = 0; i < count; ++i) {
            const int kind = r.ranged("intersection kind", 0, 4);
            if (version >= fv191) r.flag("intersection curvature");
            out.sections.push_back(r.position());
            out.fields.sections.push_back({r.real("plane a"), r.real("plane b"), r.real("plane c"), r.real("plane d")});
            r.flag("intersection built"); const int splines = r.count("intersection splines", 4);
            const int width = version >= fv160 && kind >= 1 && kind <= 3 ? 9 : 13;
            for (int j = 0; j < splines; ++j) {
                const int points = r.count("intersection points", width);
                r.skip(qsizetype(points) * width, "intersection cached coordinates");
            }
        }
    }
    if (version >= fv191) {
        Reader r(bytes, blocks.at("markers").offset);
        const int count = r.count("markers", version >= fv260 ? 11 : 10);
        for (int i = 0; i < count; ++i) {
            r.flag("marker visible"); if (version >= fv260) r.flag("marker selected");
            r.flag("marker curvature"); r.real("marker curvature scale");
            const int points = r.count("marker points", 13);
            std::vector<FbmPosition> values; std::vector<qsizetype> offsets;
            values.reserve(std::size_t(points)); offsets.reserve(std::size_t(points));
            for (int j = 0; j < points; ++j) {
                offsets.push_back(r.position());
                values.push_back({r.real("marker x"), r.real("marker y"), r.real("marker z")});
                r.flag("marker knuckle");
            }
            out.fields.markers.push_back(std::move(values)); out.markers.push_back(std::move(offsets));
        }
    }
    for (const auto *name : {"delft", "kaper", "holtr", "ost", "planing", "hollenbach", "oortmerssen", "fung", "rbhs", "mh"}) {
        const auto found = blocks.find(name); if (found == blocks.end()) continue;
        out.calculations[name] = found->second.offset;
        Reader r(bytes, found->second.offset);
        auto &values = out.fields.calculations[name];
        const int count = int(found->second.size / 4) - 1; values.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i) values.push_back(r.rawReal("stored calculation parameter"));
    }
    return out;
}

} // namespace

Fbm Fbm::read(const QByteArray &bytes) {
    if (bytes.size() > MaximumBytes)
        throw std::invalid_argument("FBM exceeds the 512 MiB document limit");
    Fbm result;
    // Own the storage even when the caller supplies QByteArray::fromRawData.
    // Validation and all saved offsets refer to this immutable owned snapshot.
    result.bytes_ = QByteArray(bytes.constData(), bytes.size());
    auto parsed = Parser(result.bytes_).parse();
    result.version_ = parsed.version;
    result.precision_ = parsed.precision;
    result.points_ = std::move(parsed.points);
    result.inspection_ = std::move(parsed.info);
    for (auto &c : parsed.caches) result.caches_.push_back({c.offset, c.size, std::move(c.empty)});
    return result;
}

QString Fbm::versionName() const {
    return QString::fromLatin1(VersionNames[version_]);
}

QJsonObject Fbm::inspect() const { return inspection_; }

FbmTransformFields Fbm::transformFields() const {
    return transformLayout(bytes_, version_, inspection_).fields;
}

Fbm Fbm::withCalculationRecordsFrom(const Fbm &original) const {
    const auto target = blockMap(inspection_), source = blockMap(original.inspection_);
    QByteArray patched = bytes_;
    for (const auto *name : {"delft", "kaper", "holtr", "ost", "propeller_task1", "propeller_task2", "propeller_task3",
                             "planing", "reverse_propeller", "hollenbach", "propeller_task4", "propeller_task5",
                             "oortmerssen", "fung", "maneuvering", "hydrodynamic_task1", "rbhs", "mh"}) {
        const auto to = target.find(name), from = source.find(name);
        if (to == target.end() || from == source.end()) continue;
        const auto &a = to->second, &b = from->second;
        if (a.size < 4 || b.size < 4 || a.size % 4 || b.size % 4) badNet("invalid calculation record span");
        const auto common = std::min(a.size, b.size) - 4;
        // Prefix fields retain their historical positions. Added fields in an
        // expanded record retain the computation adapter's initialized defaults.
        std::memcpy(patched.data() + a.offset, original.bytes_.constData() + b.offset, std::size_t(common));
        std::memcpy(patched.data() + a.offset + a.size - 4, original.bytes_.constData() + b.offset + b.size - 4, 4);
    }
    return patched == bytes_ ? *this : read(patched);
}

Fbm Fbm::withTransformFields(const std::vector<FbmPosition> &positions, const FbmTransformFields &fields) const {
    const auto layout = transformLayout(bytes_, version_, inspection_);
    const auto &old = layout.fields;
    if (positions.size() != points_.size() || fields.hasDimensions != old.hasDimensions ||
        fields.hasSplit != old.hasSplit || fields.defaultSplit != old.defaultSplit ||
        fields.sections.size() != old.sections.size() || fields.markers.size() != old.markers.size() ||
        fields.calculations.size() != old.calculations.size()) badNet("affine field schema mismatch");
    QByteArray patched = bytes_; bool changed = false;
    const auto patch = [&](qsizetype offset, float value, bool bounded = true) {
        quint32 raw; std::memcpy(&raw, &value, 4);
        const auto was = qFromLittleEndian<quint32>(bytes_.constData() + offset);
        if (raw == was) return; // Includes untouched legacy NaN payloads.
        if (!std::isfinite(value) || (bounded && std::abs(value) > 1e9f)) badNet("invalid transformed scalar");
        qToLittleEndian<quint32>(raw, patched.data() + offset); changed = true;
    };
    const auto point = [&](qsizetype offset, FbmPosition p) {
        patch(offset, p.x); patch(offset + 4, p.y); patch(offset + 8, p.z);
    };
    for (std::size_t i = 0; i < positions.size(); ++i) point(points_[i].coordinateOffset, positions[i]);
    if (fields.hasDimensions) {
        patch(layout.dimensions, fields.dimensions.x, false);
        patch(layout.dimensions + 4, fields.dimensions.y, false);
        patch(layout.dimensions + 8, fields.dimensions.z, false);
        if (fields.particularsSet != old.particularsSet) {
            patched[layout.particulars] = fields.particularsSet ? 1 : 0; changed = true;
        }
    } else if (fields.dimensions.x != old.dimensions.x || fields.dimensions.y != old.dimensions.y ||
               fields.dimensions.z != old.dimensions.z || fields.particularsSet != old.particularsSet)
        badNet("project dimensions require file version 1.2");
    if (fields.hasSplit) patch(layout.split, fields.split, false);
    else if (fields.split != old.split) badNet("explicit split section requires file version 1.6");
    for (std::size_t i = 0; i < fields.sections.size(); ++i)
        for (int j = 0; j < 4; ++j) patch(layout.sections[i] + j * 4, fields.sections[i][j]);
    for (std::size_t i = 0; i < fields.markers.size(); ++i) {
        if (fields.markers[i].size() != old.markers[i].size()) badNet("marker point cardinality changed");
        for (std::size_t j = 0; j < fields.markers[i].size(); ++j) point(layout.markers[i][j], fields.markers[i][j]);
    }
    for (const auto &[name, values] : fields.calculations) {
        const auto original = old.calculations.find(name);
        if (original == old.calculations.end() || values.size() != original->second.size())
            badNet("calculation record schema changed");
        for (std::size_t i = 0; i < values.size(); ++i) patch(layout.calculations.at(name) + qsizetype(i) * 4, values[i], false);
    }
    if (!changed) return *this;
    if (inspection_["opaque_tail_bytes"].toInteger()) badNet("cannot transform unknown FBM extensions");
    Writer writer; writer.data.reserve(patched.size()); qsizetype at = 0;
    for (const auto &cache : caches_) {
        writer.append(QByteArrayView(patched.constData() + at, cache.offset - at));
        writer.append(cache.empty); at = cache.offset + cache.size;
    }
    writer.append(QByteArrayView(patched.constData() + at, patched.size() - at));
    return read(writer.data);
}

FbmControlNet Fbm::controlNet() const {
    const auto blocks = blockMap(inspection_);
    FbmControlNet net;
    net.points.reserve(points_.size());
    for (std::size_t i = 0; i < points_.size(); ++i) net.points.push_back({points_[i], int(i)});
    {
        Reader r(bytes_, blocks.at("control_edges").offset);
        const int count = r.count("control edges", 10); net.edges.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i) {
            FbmNetEdge edge;
            edge.a = r.index("edge start", int(points_.size()));
            edge.b = r.index("edge end", int(points_.size()));
            edge.crease = r.flag("edge crease"); edge.selected = r.flag("edge selected");
            edge.sourceIndex = i; net.edges.push_back(edge);
        }
    }
    if (version_ >= fv195) {
        Reader r(bytes_, blocks.at("control_curves").offset);
        const int count = r.count("control curves", 5); net.curves.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i) {
            FbmNetCurve curve; curve.sourceIndex = i;
            const int n = r.count("curve points", 4); curve.points.reserve(std::size_t(n));
            for (int j = 0; j < n; ++j) curve.points.push_back(r.index("curve point", int(points_.size())));
            curve.selected = r.flag("curve selected"); net.curves.push_back(std::move(curve));
        }
    }
    {
        Reader r(bytes_, blocks.at("control_faces").offset);
        const int count = r.count("control faces", 21); net.faces.reserve(std::size_t(count));
        const int layers = std::max(1, blocks.at("layers").count);
        for (int i = 0; i < count; ++i) {
            FbmNetFace face; face.sourceIndex = i;
            const int n = r.count("face points", 4); face.points.reserve(std::size_t(n));
            for (int j = 0; j < n; ++j) face.points.push_back(r.index("face point", int(points_.size())));
            face.layer = r.index("face layer", layers, true);
            face.selected = r.flag("face selected"); net.faces.push_back(std::move(face));
        }
    }
    return net;
}

QByteArray Fbm::blockBytes(const QString &name) const {
    const auto blocks = blockMap(inspection_);
    const auto it = blocks.find(name);
    if (it == blocks.end()) badNet("requested block is absent from this file version");
    return bytes_.mid(it->second.offset, it->second.size);
}

Fbm Fbm::withBlocks(const std::map<QString, QByteArray> &blocksToReplace,
                    bool invalidateGeometry) const {
    if (inspection_["opaque_tail_bytes"].toInteger()) badNet("cannot edit unknown FBM extensions");
    const auto blocks = blockMap(inspection_);
    struct Replacement { BlockSpan span; QByteArray data; };
    std::vector<Replacement> replacements;
    for (const auto &[name, data] : blocksToReplace) {
        const auto it = blocks.find(name);
        if (it == blocks.end()) badNet("replacement block is absent from this file version");
        if (bytes_.mid(it->second.offset, it->second.size) != data)
            replacements.push_back({it->second, data});
    }
    if (replacements.empty()) return *this;
    if (invalidateGeometry)
        for (const auto &cache : caches_) replacements.push_back({{cache.offset, cache.size, 0}, cache.empty});
    std::sort(replacements.begin(), replacements.end(), [](const auto &a, const auto &b) {
        return a.span.offset < b.span.offset;
    });
    qint64 finalSize = bytes_.size();
    qsizetype previousEnd = 0;
    for (const auto &item : replacements) {
        if (item.span.offset < previousEnd || item.span.size < 0 ||
            item.span.offset > bytes_.size() - item.span.size) badNet("overlapping replacement ranges");
        previousEnd = item.span.offset + item.span.size;
        finalSize += item.data.size() - item.span.size;
    }
    if (finalSize < 0 || finalSize > MaximumBytes) badNet("result exceeds 512 MiB");
    Writer w; w.data.reserve(qsizetype(finalSize)); qsizetype at = 0;
    for (const auto &item : replacements) {
        w.append(QByteArrayView(bytes_.constData() + at, item.span.offset - at));
        w.append(item.data); at = item.span.offset + item.span.size;
    }
    w.append(QByteArrayView(bytes_.constData() + at, bytes_.size() - at));
    return read(w.data);
}

Fbm Fbm::withControlNet(const FbmControlNet &net, bool invalidateGeometry) const {
    if (inspection_["opaque_tail_bytes"].toInteger()) badNet("cannot edit unknown FBM extensions");
    const auto blocks = blockMap(inspection_);
    const auto pointMap = sourceMap(net.points, int(points_.size()));
    const auto edgeMap = sourceMap(net.edges, blocks.at("control_edges").count);
    const auto faceMap = sourceMap(net.faces, blocks.at("control_faces").count);
    const auto curveMap = sourceMap(net.curves, version_ >= fv195 ? blocks.at("control_curves").count : 0);
    if (version_ < fv195 && !net.curves.empty()) badNet("control curves require file version 1.95");
    const int layers = std::max(1, blocks.at("layers").count);

    struct Replacement { BlockSpan span; QByteArray data; };
    std::vector<Replacement> replacements;
    bool surfaceChanged = false;
    auto replace = [&](const char *name, QByteArray data, bool surface = false) {
        const auto span = blocks.at(QString::fromLatin1(name));
        if (surface && QByteArrayView(bytes_.constData() + span.offset, span.size) != QByteArrayView(data))
            surfaceChanged = true;
        replacements.push_back({span, std::move(data)});
    };
    bool relationsUnchanged = net.points.size() == points_.size();
    {
        Writer w; w.integer(int(net.points.size()));
        for (std::size_t i = 0; i < net.points.size(); ++i) {
            const auto &p = net.points[i].value;
            for (float value : {p.x, p.y, p.z}) {
                if (!std::isfinite(value) || std::abs(value) > 1e9f) badNet("coordinate outside supported domain");
                w.real(value);
            }
            if (p.vertexType < 0 || p.vertexType > 3) badNet("invalid vertex type");
            netIndex(p.anchor, net.points.size(), true);
            netIndex(p.lineA, net.points.size(), true); netIndex(p.lineB, net.points.size(), true);
            if (version_ < fv198 && p.locked) badNet("locked points require file version 1.98");
            if (version_ < fv421 && (p.lineA != -1 || p.lineB != -1)) badNet("linear constraints require file version 4.2");
            if (version_ < fv462 && (p.anchor != -1 || p.hardAnchor)) badNet("anchors require file version 4.6.2");
            w.integer(p.vertexType); w.flag(p.selected);
            if (version_ >= fv198) w.flag(p.locked);
            if (i < points_.size()) {
                const auto &old = points_[i];
                relationsUnchanged &= p.anchor == old.anchor && p.hardAnchor == old.hardAnchor &&
                                      p.lineA == old.lineA && p.lineB == old.lineB;
            }
        }
        replace("control_points", std::move(w.data), true);
    }
    {
        Writer w; w.integer(int(net.edges.size()));
        for (const auto &edge : net.edges) {
            netIndex(edge.a, net.points.size()); netIndex(edge.b, net.points.size());
            w.integer(edge.a); w.integer(edge.b); w.flag(edge.crease); w.flag(edge.selected);
        }
        replace("control_edges", std::move(w.data), true);
    }
    if (version_ >= fv195) {
        Writer w; w.integer(int(net.curves.size()));
        for (const auto &curve : net.curves) {
            if (curve.points.size() > std::size_t(MaximumCollectionItems)) badNet("too many curve points");
            w.integer(int(curve.points.size()));
            for (int p : curve.points) { netIndex(p, net.points.size()); w.integer(p); }
            w.flag(curve.selected);
        }
        replace("control_curves", std::move(w.data), true);
    }
    {
        Writer w; w.integer(int(net.faces.size()));
        for (const auto &face : net.faces) {
            if (face.points.size() < 3 || face.points.size() > net.points.size()) badNet("invalid face point count");
            netIndex(face.layer, std::size_t(layers), true);
            w.integer(int(face.points.size()));
            for (int p : face.points) { netIndex(p, net.points.size()); w.integer(p); }
            w.integer(face.layer); w.flag(face.selected);
        }
        replace("control_faces", std::move(w.data), true);
    }
    const bool pointIdentity = identityMap(pointMap, net.points.size());
    const bool namedPoints = std::any_of(net.points.begin(), net.points.end(), [](const auto &point) { return point.name.has_value(); });
    if (namedPoints && version_ < fv421) badNet("point names require file version 4.2");
    if (!surfaceChanged && !namedPoints && relationsUnchanged && pointIdentity && identityMap(edgeMap, net.edges.size()) &&
        identityMap(curveMap, net.curves.size()) && identityMap(faceMap, net.faces.size())) return *this;

    if (version_ >= fv421) {
        replace("point_names", mappedNames(bytes_, blocks.at("point_names"), pointMap, &net.points));
        replace("edge_names", mappedNames(bytes_, blocks.at("edge_names"), edgeMap));
        replace("face_names", mappedNames(bytes_, blocks.at("face_names"), faceMap));
        replace("curve_names", mappedNames(bytes_, blocks.at("curve_names"), curveMap));
        if (!pointIdentity || !relationsUnchanged) {
            Writer w; w.integer(0); int count = 0;
            for (std::size_t i = 0; i < net.points.size(); ++i) {
                const auto &p = net.points[i].value;
                if (p.lineA == -1 && p.lineB == -1) continue;
                w.integer(int(i)); w.integer(p.lineA); w.integer(p.lineB); ++count;
            }
            w.countAtStart(count); replace("linear_constraints", std::move(w.data));
        }
    }
    if (version_ >= fv430)
        replace("point_groups", mappedGroups(bytes_, blocks.at("point_groups"), pointMap));
    if (version_ >= fv462 && (!pointIdentity || !relationsUnchanged)) {
        Writer w; w.integer(0); int count = 0;
        for (std::size_t i = 0; i < net.points.size(); ++i) {
            const auto &p = net.points[i].value;
            if (p.anchor == -1 && !p.hardAnchor) continue;
            w.integer(int(i)); w.integer(p.anchor); w.flag(p.hardAnchor); ++count;
        }
        w.countAtStart(count); replace("anchors", std::move(w.data));
    }
    if (invalidateGeometry)
        for (const auto &cache : caches_) replacements.push_back({{cache.offset, cache.size, 0}, cache.empty});
    std::sort(replacements.begin(), replacements.end(), [](const auto &a, const auto &b) {
        return a.span.offset < b.span.offset;
    });
    qint64 finalSize = bytes_.size();
    qsizetype previousEnd = 0;
    for (const auto &item : replacements) {
        if (item.span.offset < previousEnd || item.span.size < 0 ||
            item.span.offset > bytes_.size() - item.span.size) badNet("overlapping replacement ranges");
        previousEnd = item.span.offset + item.span.size;
        finalSize += item.data.size() - item.span.size;
    }
    if (finalSize < 0 || finalSize > MaximumBytes) badNet("result exceeds 512 MiB");
    Writer w; w.data.reserve(qsizetype(finalSize)); qsizetype at = 0;
    for (const auto &item : replacements) {
        w.append(QByteArrayView(bytes_.constData() + at, item.span.offset - at));
        w.append(item.data); at = item.span.offset + item.span.size;
    }
    w.append(QByteArrayView(bytes_.constData() + at, bytes_.size() - at));
    return read(w.data);
}

Fbm Fbm::withPointPositions(const std::vector<FbmPosition> &positions) const {
    if (positions.size() != points_.size()) throw std::invalid_argument("Point coordinate count mismatch");
    bool changed = false;
    for (size_t i = 0; i < positions.size(); ++i) {
        const auto &p = positions[i]; const auto &old = points_[i];
        for (float v : {p.x, p.y, p.z})
            if (!std::isfinite(v) || std::abs(v) > 1e9f) throw std::invalid_argument("Coordinate outside supported domain");
        changed |= p.x != old.x || p.y != old.y || p.z != old.z;
    }
    if (!changed) return *this;
    if (inspection_["opaque_tail_bytes"].toInteger()) throw std::invalid_argument("Cannot edit unknown FBM extensions");
    QByteArray patched = bytes_;
    for (size_t i = 0; i < positions.size(); ++i) {
        auto at = points_[i].coordinateOffset;
        for (float v : {positions[i].x, positions[i].y, positions[i].z}) {
            quint32 bits; std::memcpy(&bits, &v, 4);
            qToLittleEndian<quint32>(bits, patched.data() + at); at += 4;
        }
    }
    QByteArray encoded; encoded.reserve(patched.size()); qsizetype at = 0;
    for (const auto &c : caches_) {
        encoded.append(patched.constData() + at, c.offset - at);
        encoded.append(c.empty); at = c.offset + c.size;
    }
    encoded.append(patched.constData() + at, patched.size() - at);
    return read(encoded);
}

void Fbm::writeAtomic(const QString &filename) const {
    if (filename.isEmpty() || filename.contains(QChar(0)))
        throw std::invalid_argument("FBM save requires a nonempty path without NUL");
    QSaveFile file(filename);
    file.setDirectWriteFallback(false);
    const auto error = [&] {
        return std::runtime_error(QString("Cannot save FBM %1: %2")
                                      .arg(filename, file.errorString()).toStdString());
    };
    if (!file.open(QIODevice::WriteOnly)) throw error();
    if (file.write(bytes_) != bytes_.size()) {
        const auto failure = error();
        file.cancelWriting();
        throw failure;
    }
    if (!file.commit()) throw error();
}

} // namespace fs::document
