"""Compile the shipped startup guard; verify stale markers stay out of launch UI."""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args()
r=a.source.resolve();c=Path(__file__).resolve().parents[1]
pre=(r/'src/dusk/ui/prelaunch.cpp').read_text()
assert 'Previous Xbox stage: {}' not in pre
assert 'Xbox safe launch: choose' not in pre
assert 'Button>(menuList, "Diagnostics")' in pre
assert 'xbox_session_diagnostics::report_for_ui(ConfigPath)' in pre
main=(r/'src/m_Do/m_Do_main.cpp').read_text()
assert 'xbox_session_diagnostics::clear_session(dusk::ConfigPath)' in main
assert 'xboxStartupDiagnostic' not in main and 'startup.diagnostic-prelaunch' not in main
with tempfile.TemporaryDirectory(prefix='diagnostics-754-') as td:
    w=Path(td);binary=w/('diagnostics.exe' if os.name=='nt' else 'diagnostics')
    compiler=shutil.which('cl' if os.name=='nt' else 'c++')
    fixture=c/'tests/startup_diagnostics_754.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/MD','/D_UWP=1','/I'+str(r/'src'),str(fixture),'/Fe:'+str(binary)]
    else:cmd=[compiler,'-std=c++20','-D_UWP=1','-I'+str(r/'src'),str(fixture),'-o',str(binary)]
    subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(binary),str(w/'data')],check=True,timeout=10)
