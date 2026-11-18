// Read-only view annotations matching FreeShip.DrawToViewport. GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include "label_layout.h"
#include "overlay_animation.h"
#include <QFont>
#include <functional>
namespace fs::presentation {
struct Annotation {QString key,text;QPointF anchor,preferred;QColor color;LabelKind kind=LabelKind::Point;bool symbol=false;};
struct ViewOverlay {QVector<Stroke> grid,wave;QVector<Annotation> labels;};
// view: 0 profile, 1 plan, 2 body, 3 perspective (HullProjection order).
ViewOverlay makeOverlay(const ModelSnapshot& model,int view,bool grid,bool hydro,const QFont& font,const std::function<QPointF(Vec3)>& project,const std::array<bool,4>& kinds={true,true,true,true});
QString dimension(double value,bool imperial);
}
