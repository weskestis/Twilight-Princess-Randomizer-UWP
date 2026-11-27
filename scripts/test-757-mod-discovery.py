"""Compile the production duplicate selector and manual install guard for Xbox and desktop."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
source = parser.parse_args().source.resolve()
packages = (source / "src/dusk/mods/loader/packages.cpp").read_text(encoding="utf-8")
loader = (source / "src/dusk/mods/loader/loader.cpp").read_text(encoding="utf-8")
selector = packages[packages.index("const PackageCandidate* select_package("):
                    packages.index("void record_package_sources(")]
install = loader[loader.index("ModLoader::OperationResult ModLoader::install_staged("):]
end = install.index("    if (!borealis::update::parse_version(metadata.version))")
guard = install[install.rindex("#if defined(_UWP)", 0, end):end]
assert install.index("inspect_mod_bundle(path, metadata") < end < install.index("install_package(path,")
assert "uwp_ports::native_id_known" in guard
code = '''#include "dusk/mods/uwp_ports.hpp"
#include <algorithm>
#include <cassert>
#include <span>
#include <string_view>
#include <vector>
using namespace std::string_view_literals;
namespace dusk::mods {
struct Metadata { std::string_view id, version; };
struct PackageCandidate { std::string_view path; Metadata metadata; unsigned searchDirIndex; };
// All fixtures use normalized versions with identical digit widths per component.
int compare_package_versions(std::string_view lhs, std::string_view rhs) { return lhs.compare(rhs); }
''' + selector + '''
struct Result { bool success; std::string_view message; };
Result install(Metadata metadata) {
''' + guard + '''
    return {true, {}};
}
}
int main() {
    using namespace dusk::mods;
#if defined(_UWP)
    constexpr bool xbox = true;
#else
    constexpr bool xbox = false;
#endif
    const auto ess = "com.dusklight.twilit_essentials"sv;
    const auto coop = "dev.remiafterdark.coop_mod"sv;
    const auto data = "example.texture"sv;
    std::vector<PackageCandidate> candidates{
        {"user-ess", {ess, "3.1.3"}, 0}, {"signed-ess", {ess, "2.5.0"}, 1},
        {"user-coop", {coop, "1.10.0"}, 0}, {"signed-coop", {coop, "1.10.1"}, 1},
        {"user-data", {data, "2.0.0"}, 0}, {"signed-data", {data, "1.0.0"}, 1}
    };
    for (int order = 0; order < 2; ++order) {
        assert(select_package(candidates, ess)->path == (xbox ? "signed-ess" : "user-ess"));
        assert(select_package(candidates, coop)->path == "signed-coop");
        assert(select_package(candidates, data)->path == "user-data");
        assert(select_package(candidates, "missing") == nullptr);
        std::reverse(candidates.begin(), candidates.end());
    }
    candidates.push_back({"user-exact", {ess, "2.5.0"}, 0});
    if (xbox) assert(select_package(candidates, ess)->path == "user-exact");
    assert(install({ess, "2.5.0"}).success && install({coop, "1.10.1"}).success);
    assert(install({data, "2.0.0"}).success);
    assert(install({ess, "3.1.3"}).success == !xbox);
    assert(install({coop, "1.10.0"}).success == !xbox);
    assert(install({uwp_ports::LuauId, "99.0.0"}).success == !xbox);
    assert(install({uwp_ports::ControllerId, "99.0.0"}).success == !xbox);
}
'''
with tempfile.TemporaryDirectory(prefix="mod-discovery-757-") as temp:
    root = Path(temp)
    cpp = root / "selector.cpp"
    cpp.write_text(code, encoding="utf-8")
    for xbox in (False, True):
        binary = root / ("checks.exe" if os.name == "nt" else "checks")
        if os.name == "nt":
            cmd = ["cl", "/nologo", "/std:c++20", "/EHsc", "/I" + str(source / "src"),
                   str(cpp), "/Fe:" + str(binary)]
            if xbox:
                cmd.append("/D_UWP=1")
        else:
            cmd = [shutil.which("c++") or "g++", "-std=c++20", "-I" + str(source / "src"),
                   str(cpp), "-o", str(binary)]
            if xbox:
                cmd.append("-D_UWP=1")
        subprocess.run(cmd, cwd=root, check=True)
        subprocess.run([str(binary)], cwd=root, check=True)
print("PASS: Xbox discovery and import use the signed port version, desktop and data selection preserved")
