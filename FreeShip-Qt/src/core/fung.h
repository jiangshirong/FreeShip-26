// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include "resistance_domain.h"
namespace fs {
// SI; half entrance angle in degrees. The empirical DLR uses standard seawater
// volume (35 ft^3/long ton), independently of the force density. See NATIVE-FUNG.
struct FungInput {
    double lwl,beam,draft,volume,cp,wettedArea,entrance,transomArea,transomBeam;
    double density,viscosity,correlation,lcbPercent,cwp,transomDraftRatio;
};
struct FungModel {
    FungInput input;
    double cm,dlr,cws,areaRatio,beamRatio,lambda;
    std::array<std::array<double,3>,3> polynomial{};
};
struct FungPoint {double knots,speed,fn,reynolds,cf,x1,x2,logCr,cr,frictionKn,allowanceKn,residuaryKn,totalKn,powerKw;};
FungModel fungModel(const FungInput&);
// LWL/B, B/T, the location of the center of buoyancy, Cm, DLR, Cws,
// submerged transom area ratio, and transom breadth ratio.
inline std::array<ResistanceDomainValue,8> fungDomain(const FungInput& p){
    const double cm=p.volume/(p.lwl*p.beam*p.draft*p.cp);
    return {{{p.lwl/p.beam,2.520,17.935},{p.beam/p.draft,1.696,10.204},{.5-p.lcbPercent/100,.481,.591},
        {cm,.556,.994},{p.volume/std::pow(p.lwl,3)*1e6/35,16.239,359.180},
        {p.wettedArea/std::sqrt(p.volume*p.lwl/35),14.324,23.673},
        {p.transomArea/(p.beam*p.draft*cm),0,.740},{p.transomBeam/p.beam,0,1}}};
}
FungPoint fungPoint(const FungModel&,double knots);
}
