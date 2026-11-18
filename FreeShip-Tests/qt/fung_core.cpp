// Core-only numerical boundaries and optimized polynomial timing.
#include "core/fung.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
int main(){
    fs::FungInput p{100,14,4.4,3138.034, .633,1700,12.9,6.545,9,1025,1.189e-6,0,-2.5,.76,.2};
    auto m=fs::fungModel(p);auto zero=fs::fungPoint(m,0);if(zero.totalKn!=0||zero.powerKw!=0)return 1;
    const double scale=std::sqrt(9.80665*p.lwl)*3600/1852;
    for(double fn:{.15,.9})if(!std::isfinite(fs::fungPoint(m,fn*scale).totalKn))return 2;
    int rejected=0;
    for(double speed:{-1.,std::numeric_limits<double>::quiet_NaN(),1e-12,.149999*scale,.900001*scale})
        try{fs::fungPoint(m,speed);}catch(const std::invalid_argument&){++rejected;}
    for(int i=0;i<16;++i){auto bad=p;
        if(i==0)bad.lwl=0;if(i==1)bad.beam=-1;if(i==2)bad.draft=0;if(i==3)bad.volume=0;if(i==4)bad.cp=1;
        if(i==5)bad.wettedArea=1;if(i==6)bad.entrance=90;if(i==7)bad.transomArea=100;if(i==8)bad.transomBeam=20;
        if(i==9)bad.density=0;if(i==10)bad.viscosity=1;if(i==11)bad.correlation=std::numeric_limits<double>::infinity();
        if(i==12)bad.lcbPercent=20;if(i==13)bad.cwp=.5;if(i==14)bad.transomDraftRatio=.9;if(i==15)bad.transomBeam=0;
        try{fs::fungPoint(fs::fungModel(bad),20);}catch(const std::invalid_argument&){++rejected;}}
    if(rejected!=21){std::cerr<<"rejected="<<rejected;return 3;}
    auto dry=p;dry.transomArea=dry.transomBeam=0;if(fs::fungPoint(fs::fungModel(dry),20).cr<=0)return 4;
    double total=0;auto start=std::chrono::steady_clock::now();
    for(int i=0;i<50000;++i)total+=fs::fungPoint(m,(.15+.75*(i%201)/200)*scale).totalKn;
    std::cout<<"zero/domain checks passed; 50000 complete points ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" checksum="<<total<<'\n';
}
