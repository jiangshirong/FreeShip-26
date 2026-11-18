// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>

namespace fs {
inline int legacyResistanceSeries(bool multihull,int choice,int fallback){
    constexpr int roundBilge[]={0,5,8,9,10,11,12,13,14};
    constexpr int multipleHulls[]={0,14,15,19,20,21,22,23,24};
    if(choice<0||choice>=int(sizeof(roundBilge)/sizeof(roundBilge[0])))return fallback;
    return multihull?multipleHulls[choice]:roundBilge[choice];
}
inline float legacyWaterplaneCoefficient(float area,float length,float beam){
    return area/(length*beam);
}
inline bool legacyUnitCoefficientInvalid(float coefficient,bool strictUpperBound){
    return strictUpperBound?coefficient>=1:coefficient==1;
}
struct ResistanceDomainValue {
    double value, minimum, maximum;
    bool valid() const {
        return std::isfinite(value)&&value>=minimum-1e-12&&value<=maximum+1e-12;
    }
};
}
