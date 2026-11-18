// Core boundaries unavailable through a positive-speed UI range.
#include "core/oortmerssen.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main(){
    fs::OortmerssenInput p{30.5,27.5,29/3.45,(29/3.45)/2.85,376,.609*29/30.5,(-.0255*29+.75)/30.5*100,23,284,1025,1.18831e-6,.00051};
    const auto model=fs::oortmerssenModel(p);const auto zero=fs::oortmerssenPoint(model,0);
    if(zero.totalKn!=0||zero.powerKw!=0||zero.residuaryKn!=0)return 1;
    int rejected=0;
    for(double speed:{-1.,std::numeric_limits<double>::quiet_NaN(),1e-12,30.})try{fs::oortmerssenPoint(model,speed);}catch(const std::invalid_argument&){++rejected;}
    for(int i=0;i<3;++i){auto bad=p;if(i==0)bad.density=-1;if(i==1)bad.lpp=0;if(i==2)bad.correlation=std::numeric_limits<double>::infinity();
        try{fs::oortmerssenModel(bad);}catch(const std::invalid_argument&){++rejected;}}
    const double boundary=.5*std::sqrt(9.80665*model.length)*3600/1852;
    if(fs::oortmerssenPoint(model,boundary).fn>.500000000001||rejected!=7)return 2;
    std::cout<<"zero speed, nonfinite inputs, low Re and Fn limit verified\n";return 0;
}
