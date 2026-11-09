// Deterministic process-contract faults, never an engineering solver.
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
int main(int argc,char**argv){
    auto exe=std::filesystem::absolute(argv[0]);std::ifstream in(exe.parent_path()/"mode.txt");std::string mode,marker;std::getline(in,mode);std::getline(in,marker);
    if(mode=="wait"){
        if(!marker.empty()){std::ofstream f(marker);f<<GetCurrentProcessId();}
        std::this_thread::sleep_for(std::chrono::seconds(30));return 0;
    }
    if(mode=="exit")return 17;
    if(mode=="missing"||exe.stem()=="DBFview")return 0;
    std::ofstream out("OUT");
    if(mode=="truncated"){out<<"incomplete\n";return 0;}
    for(int i=0;i<35;++i)out<<"header\n";
    for(int i=0;i<19;++i)out<<i*10<<" "<<(mode=="nonfinite"?"NaN":"0.2")<<" 0 0.3 0 0.04 0\n";
    return 0;
}
