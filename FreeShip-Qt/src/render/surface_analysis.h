// SPDX-License-Identifier: GPL-2.0-or-later
// Angle deficit follows the display triangles. Invalid/boundary vertices are unknown.
#pragma once
#include "core/model_snapshot.h"
#include <QColor>

enum class HullShade { Flat=0,Gaussian=1,Developable=2,Zebra=3 };
enum class ShadeClass : quint8 { Unknown=1, NearZero=2, Positive=4, Negative=8, Nonzero=16 };
constexpr quint8 shadeBit(ShadeClass value) { return static_cast<quint8>(value); }
struct SurfaceShadeVertex {
    fs::Vec3 normal;
    QColor gaussian,developable;
    quint8 gaussianClasses=0,developableClasses=0;
};
using SurfaceShadeTriangle=std::array<SurfaceShadeVertex,3>;
struct SurfaceAnalysis {
    QVector<double> angleDefect;
    QVector<bool> valid;
    QVector<bool> developableEvaluated;
    QVector<fs::Vec3> normals;
    QVector<SurfaceShadeTriangle> triangles;
    double minimum=0,maximum=0;
    int boundary=0,invalid=0;
};
std::shared_ptr<const SurfaceAnalysis> analyzeSurface(const fs::ModelSnapshot& snapshot);
QColor gaussianColor(double degrees,double minimum,double maximum);
struct HullStyle {
    HullShade mode=HullShade::Flat;
    fs::Vec3 eye{},direction{0,1,0}; // mesh-centered/normalized coordinates
    bool perspective=false;
    QColor zebra=Qt::black;
    QColor zebraLight{238,243,242};
};

inline QColor developableDisplayColor(ShadeClass region) {
    switch(region){
    case ShadeClass::NearZero:return QColor("#1b9d45");
    case ShadeClass::Nonzero:return QColor("#c43f36");
    default:return QColor(150,150,150);
    }
}
