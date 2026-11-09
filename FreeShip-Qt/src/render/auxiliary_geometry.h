// Cubic presentation of the existing Single spline coefficients.
// FreeShip_26, GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include <QPainterPath>
#include <functional>
namespace fs::presentation {
using Cubic=std::array<Vec3,4>;
QVector<Cubic> splineCubics(const SectionSpline&);
Vec3 cubicValue(const Cubic&,double t);
QVector<Cubic> splitBodyCubics(const QVector<Cubic>&,double split);
QPainterPath projectCubics(const QVector<Cubic>&,const std::function<QPointF(Vec3)>&,bool perspective);
}
