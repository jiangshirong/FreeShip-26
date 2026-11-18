// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace fs {
enum class PropellerConstraint {Diameter,Rpm};
struct PropellerDesignInput {
    double speedKnots,resistanceKn,density,wake,deduction,etaR,etaS,etaG,area,pitchLow,pitchHigh,maxDiameter;
    int blades,propellers;
};
struct PropellerDesignPoint {
    double speedKnots,resistanceKn,diameter,rpm,pitch,j,kt,kq,eta0,totalThrustKn,totalTorqueKnM,
           deliveredPowerKw,shaftInputPowerKw,enginePowerKw,thrustResidualKn;
    bool pitchBoundary=false,advanceBoundary=false,diameterBoundary=false;
};
struct PropellerSearch {
    std::optional<PropellerDesignPoint> best;
    int evaluations=0,maxRootCount=0;
    double pitchStep=0;
};
using DesignCheck=std::function<void()>;
PropellerSearch designPropeller(const PropellerDesignInput&,PropellerConstraint,double fixed,const DesignCheck& check={});
struct PropellerMatch {
    std::array<PropellerDesignPoint,5> table;
    std::vector<PropellerDesignPoint> search;
    PropellerDesignPoint target,matched;
    bool tableLimited=false;
    double powerResidualKw=0;
};
PropellerMatch matchPropeller(const PropellerDesignInput&,double rpm,double totalPowerKw,
    const std::array<double,5>& speeds,const std::array<double,5>& resistance,const DesignCheck& check={});
}
