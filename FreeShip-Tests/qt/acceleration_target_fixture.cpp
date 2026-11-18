// Deterministic streaming protocol fixture, not a hydrodynamic model.
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <string>
int main(int argc,char**argv){
    std::ifstream config(std::filesystem::absolute(argv[0]).parent_path()/"mode.txt");
    std::string mode;config>>mode;
    if(mode=="exit")return 17;
    std::ofstream out("RVRSRES.dat");
    out<<"0 0 0 0 0 0\n"<<std::flush;
    if(mode=="unreachable")out<<"10 2 0.1 20 3 4\n"<<std::flush;
    else{
        out<<"10 5 1 100 20 30\n"<<std::flush;
        if(mode=="stream")std::this_thread::sleep_for(std::chrono::milliseconds(80));
        out<<"20 20 3 200 40 50\n"<<std::flush;
        if(mode=="stream")std::this_thread::sleep_for(std::chrono::seconds(15));
        if(mode!="brake")out<<"30 50 5 300 60 70\n"<<std::flush;
    }
    out.close();std::filesystem::remove("TMP4.tsk");return 0;
}
