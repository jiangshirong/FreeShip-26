// FreeShip_26 native FBM document codec. GPL-2.0-or-later; see COPYING.
// Binary format provenance: FreeShip-Plus-Lazarus/Units/FreeShip.inc,
// FreeFileBuffer.pas, FreeVersionUnit.pas and the LoadBinary/SaveBinary units.
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <array>
#include <map>
#include <optional>
#include <vector>

namespace fs::document {

struct FbmControlPoint {
    float x = 0, y = 0, z = 0;
    qsizetype coordinateOffset = 0;
    int vertexType = 0;
    bool selected = false;
    bool locked = false;
    // Includes both constraint/anchor subjects and their referenced points.
    bool constrained = false;
    int anchor = -1, lineA = -1, lineB = -1;
    bool hardAnchor = false;
};

struct FbmPosition { float x = 0, y = 0, z = 0; };

struct FbmNetPoint {
    FbmControlPoint value;
    int sourceIndex = -1;
    // Absent preserves the source name. Present supplies an encoded FBM name.
    std::optional<QByteArray> name = std::nullopt;
};
struct FbmNetEdge {
    int a = 0, b = 0;
    bool crease = false, selected = false;
    int sourceIndex = -1;
};
struct FbmNetCurve {
    std::vector<int> points;
    bool selected = false;
    int sourceIndex = -1;
};
struct FbmNetFace {
    std::vector<int> points;
    int layer = -1;
    bool selected = false;
    int sourceIndex = -1;
};
struct FbmControlNet {
    std::vector<FbmNetPoint> points;
    std::vector<FbmNetEdge> edges;
    std::vector<FbmNetCurve> curves;
    std::vector<FbmNetFace> faces;
};

// Persistent fields touched by whole-model affine editing. Other settings,
// marker attributes, JPEG data and calculator padding remain opaque/unmodified.
struct FbmTransformFields {
    bool hasDimensions = false, hasSplit = false;
    bool particularsSet = false, defaultSplit = true;
    FbmPosition dimensions{1, 1, 1};
    float split = 0;
    std::vector<std::array<float, 4>> sections;
    std::vector<std::vector<FbmPosition>> markers;
    std::map<QString, std::vector<float>> calculations;
};

// An immutable validated binary snapshot. Saving writes the complete original
// byte stream, including caches, record padding and any opaque trailing bytes.
// This is a structural codec, not a geometry/rebuild or image decoder.
class Fbm {
public:
    static constexpr qsizetype MaximumBytes = 512 * 1024 * 1024;
    static constexpr int MaximumCollectionItems = 2000000;

    // Throws std::invalid_argument, including the field/byte offset, on invalid,
    // truncated, unsupported-version or over-limit input. FTM is not accepted.
    static Fbm read(const QByteArray &bytes);
    const QByteArray &bytes() const noexcept { return bytes_; }
    int version() const noexcept { return version_; } // On-disk enum ordinal.
    QString versionName() const;
    int precision() const noexcept { return precision_; }
    const std::vector<FbmControlPoint> &controlPoints() const noexcept { return points_; }
    QJsonObject inspect() const;

    // QSaveFile replacement with direct-write fallback disabled. Failure leaves
    // an existing target intact and throws std::runtime_error.
    void writeAtomic(const QString &filename) const;

    // Replace coordinates in file order, preserving every unrelated field.
    // Derived intersection/flowline caches are removed and marked unbuilt.
    // This does not perform dependency propagation; point_edit owns that rule.
    Fbm withPointPositions(const std::vector<FbmPosition> &positions) const;

    // Connections and anchor/line references use the returned/candidate indices.
    // sourceIndex identifies an original entity for name/group preservation;
    // -1 creates an unnamed entity. Each original index may be retained once.
    FbmControlNet controlNet() const;
    Fbm withControlNet(const FbmControlNet &net, bool invalidateGeometry = true) const;
    // Named blocks come only from the validated format traversal. Related
    // records (for example layers and face-layer indices) are replaced in one
    // transaction and the complete candidate is reparsed before returning.
    QByteArray blockBytes(const QString &name) const;
    Fbm withBlocks(const std::map<QString, QByteArray> &replacements,
                   bool invalidateGeometry = false) const;
    FbmTransformFields transformFields() const;
    // The legacy computation adapter normalizes calculator record padding.
    // Before a native edit, recover original persistent calculator payloads
    // without transferring geometry or index-bearing blocks. Known historical
    // short records copy their common float prefix and final flags/padding.
    Fbm withCalculationRecordsFrom(const Fbm &original) const;
    // Cardinalities and field availability must match; this only patches stored
    // Singles/particulars status, then invalidates derived geometry if changed.
    Fbm withTransformFields(const std::vector<FbmPosition> &positions,
                            const FbmTransformFields &fields) const;

private:
    Fbm() = default;
    QByteArray bytes_;
    int version_ = 0;
    int precision_ = 0;
    std::vector<FbmControlPoint> points_;
    QJsonObject inspection_;
    struct CacheRange { qsizetype offset, size; QByteArray empty; };
    std::vector<CacheRange> caches_;
};

} // namespace fs::document
