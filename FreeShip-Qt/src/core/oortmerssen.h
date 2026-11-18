// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include "resistance_domain.h"
namespace fs {
// LCB is forward-positive from LWL midships. LPP and LWL share the forward
// reference, as in Helmore (2008), Appendix 1. Density is kg/m3, viscosity m2/s.
struct OortmerssenInput {
    double lwl,lpp,beam,draft,volume,cpLwl,lcbLwlPercent,entranceDegrees;
    double wettedArea,density,viscosity,correlation;
};
struct OortmerssenModel {
    OortmerssenInput input;
    double length,cp,lcb,cm,wettedArea,m;
    std::array<double,4> coefficients;
};
struct OortmerssenPoint {
    double knots,speed,fn,reynolds,cf,frictionKn,allowanceKn,residuaryKn,totalKn,powerKw;
};
OortmerssenModel oortmerssenModel(const OortmerssenInput&);
inline double oortmerssenLength(const OortmerssenInput& p){return (p.lwl+p.lpp)*.5;}
// LD/B, B/T, Cp(LD), Cm, LCB(LD), half entrance angle.
inline std::array<ResistanceDomainValue,6> oortmerssenDomain(const OortmerssenInput& p){
    const double length=oortmerssenLength(p);
    return {{{length/p.beam,3,6.2},{p.beam/p.draft,1.9,4},{p.cpLwl*p.lwl/length,.5,.725},
        {p.volume/(p.cpLwl*p.lwl*p.beam*p.draft),.73,.97},
        {(p.lcbLwlPercent*p.lwl/100-(p.lwl-length)*.5)/length*100,-8,2.8},{p.entranceDegrees,10,46}}};
}
OortmerssenPoint oortmerssenPoint(const OortmerssenModel&,double knots);
}
