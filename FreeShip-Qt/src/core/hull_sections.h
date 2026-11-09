// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_snapshot.h"
#include "native/document.h"
#include "native/sections.h"
namespace fs::hull {
struct Point { double y,z; };
struct Station { double x; std::vector<Point> points; };
struct Sections {
    std::vector<Station> stations; // Source X ascending, source units, positive half breadth.
    double metersPerUnit,density; // Density in t/m3.
    QJsonObject diagnostics;
};
std::vector<std::array<double,2>> sampleSection(const native_geometry::SectionCurve&,double tolerance);
Sections build(const native_geometry::DocumentInput&,const ModelSnapshot&,bool closeDeck);
}
