"""Compile actual UWP save helpers, GCI builder and card control entry points."""
import argparse,os,re,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
def function(source,signature):
    start=source.index(signature);depth=0;parentheses=0
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[(){}]',source[start:]):
        if m.group()=='(':parentheses+=1
        elif m.group()==')':parentheses-=1
        elif m.group()=='{' and (depth or not parentheses):depth+=1
        elif m.group()=='}' and depth:
            depth-=1
            if not depth:return source[start:start+m.end()]
    raise RuntimeError('Unclosed '+signature)
with tempfile.TemporaryDirectory(prefix='vanilla-gci-743-') as temp:
    work=Path(temp);name='src/m_Do/m_Do_MemCard.cpp';card=(root/name).read_text();rw=(root/'src/m_Do/m_Do_MemCardRWmng.cpp').read_text()
    if a.negative_control:
        f=work/name;f.parent.mkdir(parents=True);f.write_text(card)
        subprocess.run(['git','-C',str(work),'apply','--reverse','--include='+name,str(control/'patches/xbox-vanilla-direct-gci-743-host.patch')],check=True);card=f.read_text()
    body='struct data_s { int unk_0x0,data_version;u8 data[SAVEFILE_SIZE+0x38];u32 checksum; };\n'
    for signature in ['static u32 mDoMemCdRWm_CalcCheckSum(','static u64 mDoMemCdRWm_CalcCheckSumGameData(','BOOL mDoMemCdRWm_TestCheckSumGameData(','void mDoMemCdRWm_SetCheckSumGameData(','void mDoMemCdRWm_BuildXboxGci(']:body+=function(rw,signature)+'\n'
    start=card.index('namespace {\nconstexpr char kXboxRandomizerSaveName');end=card.index('}  // namespace',start)+len('}  // namespace');body+=card[start:end]+'\n'
    for signature in ['void mDoMemCd_Ctrl_c::ThdInit(','void mDoMemCd_Ctrl_c::update(','void mDoMemCd_Ctrl_c::load(','void mDoMemCd_Ctrl_c::save(','void mDoMemCd_Ctrl_c::command_attach(','void mDoMemCd_Ctrl_c::setFileName(','const char* mDoMemCd_Ctrl_c::getFileName(']:body+=function(card,signature)+'\n'
    # Use the real public getter and write-completion code, including mounted imports.
    header=(root/'include/m_Do/m_Do_MemCard.h').read_text()
    body+=function(header,'inline const char* mDoMemCd_GetFileName(')+'\n'
    types=(root/'src/dusk/save_manager.hpp').read_text()
    manager=(root/'src/dusk/save_manager.cpp').read_text()
    body+='namespace dusk::save_manager {\n'
    for signature in ['enum class StorageKind {','struct Result {','struct SaveIdentity {','struct Storage {']:
        body+=function(types,signature)+';\n'
    for signature in ['Result success(','Result failure(','Result finish_write(']:body+=function(manager,signature)+'\n'
    body+='}\n'
    (work/'production-vanilla.inc').write_text(body)
    compiler=shutil.which('cl' if os.name=='nt' else 'c++');binary=work/('vanilla.exe' if os.name=='nt' else 'vanilla');fixture=control/'tests/vanilla_direct_gci.cpp';defs=['_UWP=1','TARGET_PC=1','PLATFORM_GCN=1','PLATFORM_WII=0','PLATFORM_SHIELD=0']+(['TPR_NEGATIVE_CONTROL=1'] if a.negative_control else [])
    if os.name=='nt':cmd=[compiler,'/nologo','/EHsc','/std:c++20','/MD',*['/D'+d for d in defs],'/I'+str(work),'/I'+str(root/'src'),str(fixture),'/Fe:'+str(binary)]
    else:cmd=[compiler,'-std=c++20','-O0',*['-D'+d for d in defs],'-I'+str(work),'-I'+str(root/'src'),str(fixture),'-o',str(binary)]
    subprocess.run(cmd,cwd=work,check=True);r=subprocess.run([str(binary),str(work/'data')],cwd=work)
    if a.negative_control:
        if r.returncode!=42:raise RuntimeError('Shipped .742 did not report no card with a valid vanilla GCI and unavailable virtual card')
        print('PASS: shipped .742 cannot open a valid vanilla save when virtual CARD reports NOCARD')
    elif r.returncode:raise SystemExit(r.returncode)
