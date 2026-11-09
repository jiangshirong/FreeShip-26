// Test protocol producer only. Not a hydrodynamic solver, never shipped with
// FreeShip. Separate from the actual pinned solver tests in qt_native_propulsion.
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
int main(){
    std::ifstream config("CONF.PRO");std::string mode,marker;std::getline(config,mode);std::getline(config,marker);
    if(!marker.empty())std::ofstream(marker)<<GetCurrentProcessId();
    if(mode=="wait"){Sleep(60000);return 0;}
    if(mode=="exit")return 23;
    int task=0;for(int n=1;n<=3;++n)if(std::ifstream("TMP"+std::to_string(n)+".tsk").good())task=n;
    if(mode=="missing")return 0;
    if(task){
        std::string output="RES"+std::to_string(task)+".tsk";std::ofstream f(output);
        if(mode=="truncated"){f<<"1 2";return 0;}if(mode=="nonfinite"){f<<"nan";return 0;}
        for(int i=1;i<=5;++i){
            if(task==1)f<<4-i*.1<<" 1.1 0.6 "<<100+i*10<<" .2 .03 .6 "<<800+i*20<<" "<<900+i*25<<'\n';
            if(task==2)f<<8+i*2<<" .1 .2 "<<500+i*100<<' '<<(mode=="weak"?2000:100)<<" .1 .2 .6 3 .2 .65 1.1 4 "<<(i<4?500+i*100:0)<<'\n';
            if(task==3){for(int j=1;j<=8;++j)f<<1+i*.1+j*.01<<' ';for(int g=0;g<5;++g)f<<.4+i*.03<<' '<<150+25*g<<' '<<6+i*2+g<<' '<<100+i*10+g*5<<' '<<100+i*200+g*150<<' ';f<<'\n';}
        }
        f.close();DeleteFileA(("TMP"+std::to_string(task)+".tsk").c_str());return 0;
    }
    // Bounded malformed FPP reports exercise numeric, summary and ordering guards.
    std::ofstream f("OUT.");for(int i=0;i<25;++i)f<<"fixture header\n";
    if(mode=="nonfinite")f<<"0 .2 0 nan .03 0 .03 .5\n";
    else {f<<"0 .2 0 .2 .03 0 .03 .5\n.2 .1 0 .1 .02 0 .02 .6\n";if(mode!="truncated")f<<".2 .6 .8 .9 .3 .1 .02\n";if(mode=="after-summary")f<<"1 2 3 4 5 6 7 8\n";}
    return 0;
}
