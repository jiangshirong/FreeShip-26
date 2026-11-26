// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "subdivision.h"

namespace fs::native_geometry {
// The face order and coordinates are those of the requested subdivision.
// Consumers that only cut the surface do not need canonical edge and curve data.
struct SubdivisionFaces {
    std::vector<SubdivisionPoint> points;
    std::vector<SubdivisionFace> faces;
};
SubdivisionFaces buildSubdivisionFaces(const GeometryInput& input,unsigned level);
}
