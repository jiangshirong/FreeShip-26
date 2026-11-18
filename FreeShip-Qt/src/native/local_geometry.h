// Exact derived geometry cache. GPL-2.0-or-later; see COPYING.
#pragma once
#include "subdivision.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace fs::native_geometry {
struct FillTriangle {
    Vec3 a{}, b{}, c{}, center{};
    std::uint32_t color = 0;
    bool symmetric = false;
    std::array<std::uint32_t, 3> shades{};
};
struct RootFill {
    std::vector<FillTriangle> below, above;
};
// One bounded, atomically replaced entry per application cache. The arithmetic
// program survives worker restarts; the original full generator remains the
// fallback and the independent regression reference.
class LocalGeometry {
public:
    LocalGeometry(const GeometryInput&, const QString& directory, const QByteArray& engine);
    const SubdivisionLevel& mesh() const;
    const std::vector<bool>& changedRoots() const { return program_.changedRoots; }
    bool active() const { return !program_.operations.empty(); }
    bool hit = false;
    QByteArray fillKey;
    std::vector<RootFill> fills;
    void save();
    QJsonObject diagnostics() const;
private:
    SubdivisionProgram program_;
    std::unique_ptr<SubdivisionSurface> full_;
    QString path_;
    QByteArray key_, loadedFillKey_;
    unsigned level_ = 0;
    double loadMs_ = 0, computeMs_ = 0, saveMs_ = 0;
};
} // namespace fs::native_geometry
