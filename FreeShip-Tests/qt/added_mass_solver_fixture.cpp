// Isolated process fault fixture; no engineering computation or fabricated oracle.
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
int main(int argc,char**argv){
    auto dir=std::filesystem::absolute(argv[0]).parent_path();std::ifstream f(dir/"mode.txt");std::string mode,marker;std::getline(f,mode);std::getline(f,marker);
    if(mode=="wait"){if(!marker.empty()){std::ofstream m(marker);m<<GetCurrentProcessId();}std::this_thread::sleep_for(std::chrono::seconds(40));return 0;}
    if(mode=="exit")return 17;if(mode=="missing")return 0;
    std::ifstream input(dir/"result.txt",std::ios::binary);std::string data((std::istreambuf_iterator<char>(input)),{});
    if(mode=="truncated")data.resize(data.size()/2);
    if(mode=="nonfinite"){auto at=data.find("4318.714");if(at!=std::string::npos)data.replace(at,8,"     NaN");}
    std::ofstream out("OUTA",std::ios::binary);out<<data;return 0;
}
