// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "bseries.h"
#include <array>

namespace fs {
// Steady ahead-running characteristics. Constant, explicitly supplied hull
// factors; not CALCPROP's undocumented slip-dependent empirical corrections.
struct PassportInput {
    double diameter,area,pitch,referenceJ;
    int blades,propellers;
    double rpm,wake,deduction,relativeRotativeEfficiency,legacyTorqueFactor,shaftEfficiency,gearEfficiency,density,enginePower;
};
struct PassportPoint {
    double j,rpm,advanceSpeed,shipSpeedKnots,kt,kq,thrust,torque,deliveredPower,enginePower,effectiveThrust;
    std::optional<double> efficiency;
};
struct PassportBranch {
    double rpm,targetPower;
    std::vector<PassportPoint> intersections;
};
struct PropulsionPassport {
    BSeriesResult polynomial;
    std::array<std::array<PassportPoint,5>,5> points; // RPM, then J
    std::array<PassportBranch,4> engine;
};
PropulsionPassport propulsionPassport(const PassportInput& input);
}
