"""Compile and exercise production companion protocol with real Borealis TCP."""
import argparse, os, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--feed',type=Path)
p.add_argument('--json-source',type=Path);p.add_argument('--fmt-source',type=Path)
a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
json=(a.json_source or a.feed/'_deps/json-src').resolve()
if not (json/'include/nlohmann/json.hpp').exists():
 candidates=list(a.feed.glob('_deps/*/include/nlohmann/json.hpp')) if a.feed else []
 if not candidates:raise RuntimeError('Pinned JSON headers missing')
 json=candidates[0].parents[2]
fmt=(a.fmt_source or a.feed/'_deps/fmt-src').resolve();borealis=root/'extern/borealis'
with tempfile.TemporaryDirectory(prefix='companion-747-') as tmp:
 w=Path(tmp);exe=w/('companion.exe' if os.name=='nt' else 'companion')
 platform='windows' if os.name=='nt' else 'posix'
 sources=[control/'tests/companion_protocol_747.cpp',root/'src/dusk/companion/server.cpp',
  borealis/'src/net.cpp',borealis/'src/net'/('platform_'+platform+'.cpp'),borealis/'src/task.cpp',borealis/'src/url.cpp']
 includes=[root/'src/dusk/companion',borealis/'include',borealis/'src',json/'include',fmt/'include']
 if os.name=='nt':
  cmd=['cl','/nologo','/std:c++20','/EHsc','/utf-8','/MD','/O1','/DFMT_HEADER_ONLY',
   *['/I'+str(x) for x in includes],*map(str,sources),'/Fe:'+str(exe),'/link','Ws2_32.lib']
 else:cmd=['c++','-std=c++20','-O1','-pthread','-DFMT_HEADER_ONLY',*['-I'+str(x) for x in includes],*map(str,sources),'-o',str(exe)]
 subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(exe)],cwd=w,check=True,timeout=60)
