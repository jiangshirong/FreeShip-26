// GPL-2.0-or-later; derived display geometry, never editable document state.
#pragma once
#include "document.h"
namespace fs::native_geometry {
std::vector<Vec3> surfacePointNormals(const SubdivisionLevel &mesh);
QByteArray buildSurfaceDisplay(const DocumentInput &input, const SubdivisionLevel &mesh);
}
