#include "dusk/mods/install_paths.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>
#include <system_error>
namespace stdfs=std::filesystem;
namespace fs {
using namespace stdfs;
inline path weakly_canonical(const path&,std::error_code& ec) {
    ec=std::make_error_code(std::errc::permission_denied);return {};
}
}
namespace fmt { template<class... Args> std::string format(const char* s,Args&&...) { return s; } }
struct LogFake {template<class...Args>void error(const char*,Args&&...) {}} Log;
namespace dusk::data {std::string abbreviated_path_string(const fs::path& p){return p.string();}}
struct UpdatePrecondition {};
namespace dusk::mods {
using dusk::data::abbreviated_path_string;
struct ModLoader {
    struct OperationResult {bool success=true;std::string message;};
    struct SearchDir {fs::path path;};
    std::vector<SearchDir> m_searchDirs;
    OperationResult install_staged(const fs::path&,const std::optional<UpdatePrecondition>&);
};
#include "production-install-paths.inc"
}
void require(bool p,const char* s){if(!p)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    require(argc>1,"workdir missing");
    const auto root=stdfs::absolute(argv[1]);const auto mods=root/"mods";
    stdfs::create_directories(mods/".staging");
    const auto staged=mods/".staging/test.dusk.part";
    {std::ofstream out(staged);out<<"PACKAGE";}
    dusk::mods::ModLoader loader;loader.m_searchDirs.push_back({mods});
    const auto result=loader.install_staged(staged,{});
    if(argc>2)return !result.success&&result.message.find("Could not resolve")!=std::string::npos?42:1;
    require(result.success,"Xbox canonicalization still blocks private staging");
    require(loader.install_staged(mods/".staging/sub/../test.dusk.part",{}).success,"lexical normalization failed");
    const auto outside=root/"other.dusk.part";{std::ofstream out(outside);out<<"KEEP";}
    require(!loader.install_staged(outside,{}).success,"outside path accepted");
    require(!loader.install_staged(staged.filename(),{}).success,"relative staged path accepted");
    require(!loader.install_staged(mods/".staging/missing.dusk.part",{}).success,"missing file accepted");
    const auto bad=mods/".staging/wrong.bin";{std::ofstream out(bad);out<<"KEEP";}
    require(!loader.install_staged(bad,{}).success,"wrong staged extension accepted");
    std::error_code ec;const auto link=mods/".staging/link.dusk.part";
    stdfs::create_symlink(outside,link,ec);
    if(!ec)require(!loader.install_staged(link,{}).success,"staged symbolic link accepted");
    const auto actual=mods/".staging";stdfs::rename(actual,mods/"kept-staging");
    ec.clear();stdfs::create_directory_symlink(root,actual,ec);
    if(!ec){const auto escape=root/"escape.dusk.part";{std::ofstream out(escape);out<<"KEEP";}
        require(!loader.install_staged(actual/"escape.dusk.part",{}).success,"linked staging directory accepted");}
    puts("Mod installer private paths and escape rejection passed");return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
