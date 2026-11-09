// FreeShip Plus curvature presentation, GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include "native/geometry.h"
namespace fs::presentation {
struct CurvatureSample {native_geometry::Vec3 base,normal;float value=0;};
CurvatureSample curvature(const native_geometry::Spline&,float parameter);
struct CombPoint {Vec3 base;native_geometry::Vec3 normal;float value=0;bool rib=false;};
QVector<CombPoint> curvatureComb(const SectionSpline&,int precision);
Vec3 combTip(const CombPoint&,float scale);
}
