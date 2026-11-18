// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <utility>
namespace fs {
enum class HollenbachCondition { SingleDesign, SingleBallast, TwinDesign };
// SI throughout. trim = TF - TA; wettedArea=0 requests Table 2 estimation.
// Explicit measured area includes appendages. No form factor is applied.
struct HollenbachInput {
    HollenbachCondition condition;
    double lpp,lwl,los,beam,draft,trim,volume,diameter,wettedArea,density,viscosity,correlation;
    int rudders,brackets,bossings,thrusters;
    bool bulb;
};
struct HollenbachModel {
    HollenbachInput input;
    double cb,ta,tf,length,wettedArea,fnCritical,fnMin,fnMax,minimumFnMin,minimumFnMax;
    double geometryFactor,minimumGeometryFactor,lengthFactor;
    bool minimumGeometryValid;
};
struct HollenbachPoint {
    double knots,speed,fn,reynolds,cf,crStandard,fnFactor,cr,frictionKn,allowanceKn,residuaryKn,totalKn,powerKw;
    double minimumKn,maximumKn;
    bool minimumValid;
};
enum class HollenbachGeometryLimit { Lpp, LengthVolume, BlockCoefficient, LengthBeam, BeamDraft,
                                     SubmergedWaterline, WaterlineLength, PropellerDraft };
struct HollenbachGeometryCheck {
    HollenbachGeometryLimit limit;
    double value, minimum, maximum;
    bool valid() const;
};
std::array<HollenbachGeometryCheck,8> hollenbachGeometryChecks(const HollenbachInput&);
bool hollenbachEndDraftsValid(const HollenbachInput&);
std::pair<double,double> hollenbachSpeedRange(const HollenbachInput&);
HollenbachModel hollenbachModel(const HollenbachInput&);
HollenbachPoint hollenbachPoint(const HollenbachModel&,double knots);
}
