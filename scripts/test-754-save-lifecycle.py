"""Exercise actual card entry points with real mutexes, a worker and mode switches."""
import argparse, os, re, shutil, subprocess, tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--negative-control', action='store_true')
args = parser.parse_args()
root = args.source.resolve()
control = Path(__file__).resolve().parents[1]

def function(source, signature):
    start = source.index(signature)
    depth = 0
    parentheses = 0
    for match in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[(){}]', source[start:]):
        if match.group() == '(': parentheses += 1
        elif match.group() == ')': parentheses -= 1
        elif match.group() == '{' and (depth or not parentheses): depth += 1
        elif match.group() == '}' and depth:
            depth -= 1
            if not depth: return source[start:start + match.end()]
    raise RuntimeError('Unclosed function: ' + signature)

with tempfile.TemporaryDirectory(prefix='save-lifecycle-754-') as td:
    work = Path(td)
    card = (root / 'src/m_Do/m_Do_MemCard.cpp').read_text()
    rw = (root / 'src/m_Do/m_Do_MemCardRWmng.cpp').read_text()
    body = 'struct data_s { int unk_0x0,data_version;u8 data[SAVEFILE_SIZE+0x38];u32 checksum; };\n'
    for signature in ['static u32 mDoMemCdRWm_CalcCheckSum(', 'static u64 mDoMemCdRWm_CalcCheckSumGameData(',
                      'BOOL mDoMemCdRWm_TestCheckSumGameData(', 'void mDoMemCdRWm_SetCheckSumGameData(',
                      'void mDoMemCdRWm_BuildXboxGci(']:
        body += function(rw, signature) + '\n'
    start = card.index('namespace {\nconstexpr char kXboxRandomizerSaveName')
    end = card.index('}  // namespace', start) + len('}  // namespace')
    body += card[start:end] + '\n'
    signatures = ['ThdInit', 'main', 'update', 'load', 'save', 'command_attach', 'setFileName', 'getFileName', 'getStatus', 'LoadSync', 'SaveSync']
    if not args.negative_control: signatures += ['refreshDirectSaveLocked', 'waitForIdleLocked']
    for name in signatures:
        match = re.search(r'^(?:void|bool|s32|u32|const char\*) mDoMemCd_Ctrl_c::' + name + r'\(', card, re.M)
        if not match: raise RuntimeError('Missing production method: ' + name)
        body += function(card, match.group()) + '\n'
    header = (root / 'include/m_Do/m_Do_MemCard.h').read_text()
    body += function(header, 'inline const char* mDoMemCd_GetFileName(') + '\n'
    types = (root / 'src/dusk/save_manager.hpp').read_text()
    manager = (root / 'src/dusk/save_manager.cpp').read_text()
    body += 'namespace dusk::save_manager {\n'
    for signature in ['enum class StorageKind {', 'struct Result {', 'struct SaveIdentity {', 'struct Storage {']:
        body += function(types, signature) + ';\n'
    for signature in ['Result success(', 'Result failure(', 'Result finish_write(']: body += function(manager, signature) + '\n'
    body += '}\n'
    (work / 'production-vanilla.inc').write_text(body)
    fixture = (control / 'tests/vanilla_direct_gci.cpp').read_text()
    fixture = '#include "threaded-card-os.inc"\n' + fixture
    fixture = fixture.replace('bool IsShuttingDown=false;', 'std::atomic_bool IsShuttingDown=false;')
    start = fixture.index('struct OSMutex{};')
    end = fixture.index('void OSCreateThread(', start)
    fixture = fixture[:start] + fixture[end:]
    fixture = fixture.replace('COMM_DETACH_e,COMM_RESCAN_FILE_e', 'COMM_DETACH_e,COMM_RESCAN_FILE_e,COMM_FORMAT_e')
    fixture = fixture.replace('bool mInitialized=false;std::string mFileName;',
        'bool mInitialized=false,mReattachPending=false;std::string mFileName,mPendingFileName;')
    fixture = fixture.replace('int getStatus(int){return mCardState;}',
        'u32 getStatus(u32);s32 LoadSync(void*,u32,u32);s32 SaveSync();void main();'
        'bool refreshDirectSaveLocked();bool waitForIdleLocked();void restore();void store();void format();void attach();void detach();s32 loadfile(){return 1;}')
    fixture = fixture.replace('int main(int argc,char**argv)', 'int legacy_main(int argc,char**argv)')
    (work / 'legacy-vanilla-fixture.inc').write_text(fixture)
    (work / 'threaded-card-os.inc').write_bytes((control / 'tests/threaded_card_os_754.inc').read_bytes())
    compiler = shutil.which('cl' if os.name == 'nt' else 'c++')
    binary = work / ('save.exe' if os.name == 'nt' else 'save')
    defs = ['_UWP=1', 'TARGET_PC=1', 'PLATFORM_GCN=1', 'PLATFORM_WII=0', 'PLATFORM_SHIELD=0']
    if args.negative_control: defs.append('EXPECT_749_FAILURES=1')
    fixture_path = control / 'tests/save_lifecycle_754.cpp'
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/EHsc', '/std:c++20', '/MD', *['/D'+d for d in defs],
               '/I'+str(work), '/I'+str(root/'src'), str(fixture_path), '/Fe:'+str(binary)]
    else:
        cmd = [compiler, '-std=c++20', '-O1', '-pthread', *['-D'+d for d in defs],
               '-I'+str(work), '-I'+str(root/'src'), str(fixture_path), '-o', str(binary)]
    subprocess.run(cmd, cwd=work, check=True)
    subprocess.run([str(binary), str(work/'data')], cwd=work, check=True, timeout=30)
