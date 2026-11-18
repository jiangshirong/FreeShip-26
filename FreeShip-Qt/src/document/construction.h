// GPL-2.0-or-later. Current Plus ModelTools / Geometry_AddCylinder semantics.
#pragma once
#include "fbm_codec.h"
namespace fs::document {
struct ConstructionGrid {
    int columns=0, rows=0;
    bool closedStart=false, closedEnd=false, cylinder=false;
    // Column-major creation order, matching the original model and stable IDs.
    std::vector<FbmPosition> points;
};
ConstructionGrid constructionGrid(const QJsonObject& request);
}
