// Native FBM calculation input. GPL-2.0-or-later; see COPYING.
#pragma once
#include "fbm_codec.h"
#include "native/subdivision.h"

namespace fs::document {
struct DocumentGeometrySettings {
    float length=1, beam=1, draft=1, split=.5f;
    float density=1.025f, appendage=1;
    bool imperial=false, simplifyIntersections=true, defaultSplit=true, actualCoefficients=true;
};
// All indices match the FBM serialized control net; no external snapshot or
// geometry adapter is involved. The caller owns stable-ID bindings separately.
native_geometry::GeometryInput documentGeometry(const Fbm &source);
DocumentGeometrySettings documentGeometrySettings(const Fbm &source);
}
