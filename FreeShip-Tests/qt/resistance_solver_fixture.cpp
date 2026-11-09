// Failure-protocol fixture only. Not a resistance solver; never packaged.
#include <windows.h>
#include <fstream>
#include <string>
int main(){
    std::ifstream config("CONF.PRO");std::string mode,marker;
    std::getline(config,mode);std::getline(config,marker);
    if(!marker.empty())std::ofstream(marker)<<GetCurrentProcessId();
    if(mode=="wait"){Sleep(60000);return 0;}
    if(mode=="exit")return 23;
    if(mode=="missing")return 0;
    std::ofstream out("hollenba.res");
    if(mode=="truncated"){out<<"1 2";return 0;}
    for(int i=0;i<(mode=="extra"?91:90);++i)out<<((mode=="nonfinite"&&i==15)?"nan":"1")<<' ';
    return 0;
}
