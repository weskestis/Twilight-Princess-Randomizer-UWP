"""Compile the port's real editor filter, inventory check and native use dispatch."""
import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


def function(source, declaration):
    start = source.index(declaration)
    end = source.index("\n}", start)
    return source[start:end + 2] + "\n"


def main(root, stock):
    port = root / "mods/uwp_ports/port_twilit_essentials/src/quick_access"
    editor = (port / "quick_access_edit.cpp").read_text(encoding="utf-8")
    quick = (port / "quick_access.cpp").read_text(encoding="utf-8")
    engine = (root / "src/d/actor/d_a_alink.cpp").read_text(encoding="utf-8")
    picker = function(editor, "void quick_access_edit_enter()")
    # Both controller layouts call this common tap action after assigning a slot.
    if quick.count("qa_strip_tap_action();") < 2 or "qa_execute_item(s_assignedItem);" not in quick:
        raise ValueError("Quick Access's radial/strip tap route changed")
    if "ITEM_PROC_SPINNER_READY" not in function(engine, "int daAlink_c::checkNewItemChange("):
        raise ValueError("Native Spinner selection changed")
    if "procSpinnerReadyInit();" not in function(engine, "int daAlink_c::changeItemTriggerKeepProc("):
        raise ValueError("Native Spinner action changed")

    blacklist = re.search(r"static const u8 kCustomizeHardBlacklist\[\] = \{.*?\};", editor, re.S).group()
    candidates = re.search(r"static const u8 kQaCandidates\[\] = \{.*?\};", picker, re.S).group()
    code = blacklist + "\n" + candidates + "\n"
    for fn in ("static bool edit_list_contains(", "static bool customize_blacklisted(", "static void edit_list_add("):
        code += function(editor, fn)
    for fn in ("bool qa_custom_contains(", "bool qa_custom_replace_at(", "bool qa_is_item_available(",
               "static void execute_generic_item(", "void qa_execute_item(",
               "static bool qa_item_has_aim_mode(", "void qa_strip_tap_action("):
        code += function(quick, fn)
    if "execute_generic_item(itemNo);" not in function(quick, "void qa_execute_item("):
        raise ValueError("Quick Access generic item dispatch changed")

    item_data = (root / "include/d/d_item_data.h").read_text(encoding="utf-8")
    items = sorted(set(re.findall(r"\bdItemNo_\w+_e\b", code)) | {"dItemNo_SPINNER_e"})
    item_values = []
    for name in items:
        value = re.search(r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*" + name + r"\b", item_data).group(1)
        item_values.append(f"{name} = {value}")
    fixture = r"""
#include <algorithm>
#include <cassert>
using u8 = unsigned char;
enum Item { ITEM_VALUES };
constexpr int QA_EDIT_MAX_ITEMS=32, QA_QUICK_SLOTS=4;
constexpr u8 QA_ITEM_NONE=0xff;
u8 s_editItems[QA_EDIT_MAX_ITEMS], inventory[24];
int s_editCount=0, ok=0, errors=0, activations=0, saves=0;
u8 s_aimItem=QA_ITEM_NONE, s_assignedItem=QA_ITEM_NONE;
u8 s_customItems[4]={QA_ITEM_NONE, QA_ITEM_NONE, QA_ITEM_NONE, QA_ITEM_NONE};
u8 selected=91, marked=0, s_deferredUseItem=QA_ITEM_NONE, s_qaOoccooPending=QA_ITEM_NONE;
int s_qaOoccooPendingFrames=0;
bool terrainAllowed=true;
bool items_same_family(u8 first,u8 second) { return first==second; }
u8 dComIfGs_getItem(int slot,bool) { return inventory[slot]; }
u8 dComIfGp_getSelectItem(int slot) { assert(slot==2); return selected; }
struct {
    struct { void setSelectItem(int slot,u8 item) { assert(slot==2); selected=item; } } play;
} g_dComIfG_gameInfo;
struct daAlink_c {
    int mEquipItem=0xff, mItemButton=0;
    int checkNewItemChange(int slot) { assert(slot==2 && selected==dItemNo_SPINNER_e); return terrainAllowed?7:0; }
    void changeItemTriggerKeepProc(int slot,int type) { assert(slot==2 && type==7); ++activations; }
} link;
daAlink_c* daPy_getPlayerActorClass() { return &link; }
struct QaSelectSlotScope { explicit QaSelectSlotScope(int slot) { assert(slot==2); } };
void qa_custom_store() { ++saves; }
daAlink_c* daPy_getLinkPlayerActorClass() { return &link; }
void qa_enter_item_aim(u8) { assert(false); }
void qa_extinguish_lantern_for_item() {}
void execute_iron_boots() { assert(false); }
void execute_horse_call() { assert(false); }
void execute_lantern() { assert(false); }
void execute_fishing_rod() { assert(false); }
bool qa_bottle_use_item(u8) { return false; }
bool qa_is_rod_item(u8) { return false; }
void play_ok_se() { ++ok; }
void play_error_se() { ++errors; }
void qa_mark_held_item(u8 item) { marked=item; }
void execute_bomb_item(daAlink_c*,u8) { assert(false); }
bool is_ooccoo_item(u8) { return false; }
PRODUCTION
int main() {
    const bool stock=STOCK;
    edit_list_add(dItemNo_SPINNER_e);
    edit_list_add(dItemNo_SPINNER_e);
    edit_list_add(dItemNo_BOW_e);
    assert(s_editCount==(stock?0:1));
    if (!stock) assert(s_editItems[0]==dItemNo_SPINNER_e);
    bool candidate=false;
    for (auto value:kQaCandidates) if (value==dItemNo_SPINNER_e) candidate=true;
    assert(candidate==!stock);
    if (stock) return 0;
    assert(qa_custom_replace_at(3,s_editItems[0]));
    assert(s_assignedItem==dItemNo_SPINNER_e && s_customItems[3]==s_assignedItem && saves==1);
    std::fill_n(inventory,24,QA_ITEM_NONE);
    qa_strip_tap_action();
    assert(activations==0 && errors==1 && selected==91);
    inventory[9]=dItemNo_SPINNER_e;
    terrainAllowed=false;
    qa_strip_tap_action();
    assert(activations==0 && errors==2 && selected==91 && marked==0);
    terrainAllowed=true;
    qa_strip_tap_action();
    assert(activations==1 && ok==1 && selected==91 && marked==dItemNo_SPINNER_e);
}
"""
    fixture = fixture.replace("ITEM_VALUES", ", ".join(item_values)).replace("PRODUCTION", code)
    fixture = fixture.replace("STOCK", "true" if stock else "false")
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp)
        source = path / "spinner.cpp"
        exe = path / "spinner.exe"
        source.write_text(fixture, encoding="utf-8")
        if shutil.which("cl"):
            command = ["cl", "/nologo", "/std:c++17", "/EHsc", str(source), f"/Fe:{exe}"]
        else:
            command = ["c++", "-std=c++17", str(source), "-o", str(exe)]
        subprocess.run(command, cwd=path, check=True)
        subprocess.run([str(exe)], cwd=path, check=True)
    print("PASS: Quick Access Spinner eligibility, item-use dispatch, native refusal, and previous slot restoration")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--stock", action="store_true")
    args = parser.parse_args()
    main(args.source, args.stock)
