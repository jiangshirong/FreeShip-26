// FreeShip_26 native calculation input. GPL-2.0-or-later; see COPYING.
#pragma once
#include "subdivision.h"
#include <QJsonObject>
#include <QString>

namespace fs::native_geometry {
struct SectionInput {
    int kind = 0, order = 0;
    Plane plane;
};
struct FlowInput {
    int id = 0, view = 0;
    float x = 0, y = 0;
};
struct DocumentInput {
    GeometryInput geometry;
    std::vector<SectionInput> sections;
    QString documentId;
    quint64 revision = 0;
    float draft = 0, split = 0, creaseSplit = 0;
    bool imperial = false, simplify = false, shadeUnderwater = true;
    QJsonObject colors, texts;
    // Stable document IDs in control-net order. Absent for the legacy oracle.
    bool surfaceDisplay = false, showNormals = false;
    std::vector<int> faceIds, layerIds, curveIds;
    std::vector<std::vector<std::size_t>> layerFaces;
    std::vector<FlowInput> flows;
    static DocumentInput read(const QJsonObject &root);
};
} // namespace fs::native_geometry
