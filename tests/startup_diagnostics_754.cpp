#include "dusk/startup_guard.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char**argv) {
    assert(argc==2);
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    namespace diag=dusk::xbox_session_diagnostics;
    namespace guard=dusk::startup_guard;
    diag::arm_session(root);
    {std::ofstream previous(root/"xbox-startup-stage-749.txt");previous<<"menu.mods.open.launcher\n";}
    guard::begin_startup(root);
    const auto report=diag::report_for_ui(root);
    assert(report.find("does not establish a crash")!=std::string::npos);
    assert(report.find("menu.mods.open.launcher")!=std::string::npos);
    assert(!std::filesystem::exists(root/diag::kMarker));
    const auto history=diag::read_bounded(root/diag::kHistory);
    for(int i=0;i<100;++i) {
        guard::previous_stage();guard::previous_stage();
        assert(history==diag::read_bounded(root/diag::kHistory));
    }
    diag::arm_session(root);guard::begin_startup(root);
    assert(std::filesystem::exists(root/diag::kMarker)); // current session is never consumed on a menu refresh
    assert(history==diag::read_bounded(root/diag::kHistory));
    diag::clear_session(root);assert(!std::filesystem::exists(root/diag::kMarker));
    for(int i=0;i<100;++i){diag::arm_session(root);diag::clear_session(root);}
    assert(history==diag::read_bounded(root/diag::kHistory));
    {std::ofstream large(root/diag::kHistory);large<<std::string(300*1024,'x');}
    diag::arm_session(root);diag::capture_previous_session(root,"fixture");
    assert(std::filesystem::exists(root/(std::string(diag::kHistory)+".old")));
    assert(std::filesystem::file_size(root/diag::kHistory)<4096);
    std::cout<<"PASS: stale marker captured once, current session preserved, clean exits clear, repeated menu reads do not write, bounded history\n";
}
