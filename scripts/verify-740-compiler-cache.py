"""Verify actual Ninja compile edges, including CMake's per-edge LAUNCHER binding."""
import argparse
from pathlib import Path
import re
import tempfile

TARGETS=('dusk_internal','dusklight','randomizer')

def verify(rules, builds):
    commands={name:body for name,body in re.findall(r'(?ms)^rule ([^\r\n]+)\r?\n(.*?)(?=^rule |\Z)',rules)}
    edges=re.findall(r'(?m)^build ([^\r\n]+)\r?\n((?:[ \t]+[^\r\n]*\r?\n)*)',builds)
    counts={}
    for target in TARGETS:
        prefix='CXX_COMPILER__'+target+'_'
        matching=[(header,body) for header,body in edges if any(
            re.search(r':\s*'+re.escape(rule)+r'(?:\s|$)',header) for rule in commands if rule.startswith(prefix))]
        if not matching:
            raise RuntimeError('No actual native C++ compile edges for '+target)
        for header,body in matching:
            rule=next(rule for rule in commands if rule.startswith(prefix) and re.search(r':\s*'+re.escape(rule)+r'(?:\s|$)',header))
            command=re.search(r'(?m)^\s*command = (.*)$',commands[rule])
            if not command:
                raise RuntimeError('Missing compile command for '+rule)
            text=command[1]
            direct=re.search(r'sccache[.]exe',text,re.I)
            variable=re.search(r'\$(?:\{LAUNCHER\}|LAUNCHER\b)',text)
            binding=re.search(r'(?im)^\s*LAUNCHER = .*sccache[.]exe',body)
            if not direct and not (variable and binding):
                raise RuntimeError('Ninja bypasses the compiler cache for '+header)
        counts[target]=len(matching)
    return counts

def self_test():
    for mode in ('direct','variable'):
        rules='';builds=''
        for target in TARGETS:
            rule='CXX_COMPILER__'+target+'_unscanned_RelWithDebInfo'
            command='C:/cache/sccache.exe cl.exe' if mode=='direct' else '${LAUNCHER}cl.exe'
            rules+=f'rule {rule}\n  command = {command} $FLAGS /c $in\n\n'
            builds+=f'build CMakeFiles/{target}.dir/a.cpp.obj: {rule} a.cpp\n  FLAGS = /Z7\n'
            if mode=='variable':builds+='  LAUNCHER = C:/cache/sccache.exe \n'
            builds+='\n'
        assert verify(rules,builds)==dict.fromkeys(TARGETS,1)
        if mode=='variable':
            broken=builds.replace('  LAUNCHER = C:/cache/sccache.exe \n','',1)
        else:
            broken=builds
            rules=rules.replace('C:/cache/sccache.exe cl.exe','cl.exe',1)
        try:verify(rules,broken)
        except RuntimeError:pass
        else:raise RuntimeError('Compiler cache bypass negative control passed')
    print('PASS .740 native Ninja cache checks accept direct/per-edge launchers and reject bypasses')

p=argparse.ArgumentParser();p.add_argument('feed',type=Path,nargs='?');p.add_argument('--self-test',action='store_true')
a=p.parse_args()
if a.self_test:self_test()
if a.feed:
    result=verify((a.feed/'CMakeFiles/rules.ninja').read_text(encoding='utf-8'),(a.feed/'build.ninja').read_text(encoding='utf-8'))
    print('PASS .740 actual cached native C++ edges:',result)
if not a.self_test and not a.feed:p.error('feed or --self-test is required')
