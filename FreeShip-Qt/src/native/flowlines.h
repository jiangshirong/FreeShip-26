// GPL-2.0-or-later. Derived surface traces, not a CFD flow solver.
#pragma once
#include "document.h"
#include <QJsonArray>
namespace fs::native_geometry {
QJsonArray buildFlowlines(const DocumentInput &document, const SubdivisionLevel &mesh,
                         const std::vector<Vec3> &normals);
}
