// SPDX-License-Identifier: GPL-2.0-or-later
// Angle deficit and color scale follow FreeSubdivisionPoint.FGetCurvature and
// FreeSubdivisionControlFace.Draw. Invalid/boundary vertices are explicitly unknown.
#pragma once
#include "core/model_snapshot.h"
#include <QColor>

enum class HullShade { Flat=0,Gaussian=1,Developable=2,Zebra=3 };
struct SurfaceShadeVertex { fs::Vec3 normal;QColor gaussian,developable; };
using SurfaceShadeTriangle=std::array<SurfaceShadeVertex,3>;
struct SurfaceAnalysis {
    QVector<double> angleDefect;
    QVector<bool> valid;
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
};
