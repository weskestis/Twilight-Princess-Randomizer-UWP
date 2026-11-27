#include <array>
#include <cassert>
#include <cstdint>
#include <type_traits>

using u8 = unsigned char;
enum { dItemNo_SMELL_YELIA_POUCH_e = 0xB0, dItemNo_SMELL_MEDICINE_e = 0xB5,
       dItemNo_NONE_e = 0xFF };
struct ModContext {};
enum HookAction { HOOK_CONTINUE, HOOK_SKIP_ORIGINAL };
struct dSv_player_status_a_c { u8 mSelectItem[4]{0xFF, 0xFF, 0xFF, 0xFF}; } status;
namespace mods {
template <class T> T arg(void* raw, int index) {
    auto value = static_cast<std::uintptr_t*>(raw)[index];
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(value);
    else return static_cast<T>(value);
}
}

bool g_configCustomZButtonEnabled = true;
bool g_inSetSelectItemIndex = false;
u8 g_zInventorySlot = 0xFF, g_zMixSlot = 0xFF;
u8 mix[4]{0xFF, 0xFF, 0xFF, 0xFF};
std::array<u8, 24> inventory{};
u8 scent = 0xFF;
bool acquired[256]{};
struct {
    struct {
        u8 selected[4]{0xFF, 0xFF, 0xFF, 0xFF};
        void setSelectItem(int index, u8 item) { selected[index] = item; }
    } play;
} g_dComIfG_gameInfo;
struct dMsgObject_c { u8 mSmellType{}; void setSmellTypeLocal(u8); };

u8 dComIfGs_getSelectItemIndex(int index) { return status.mSelectItem[index]; }
u8 dComIfGs_getMixItemIndex(int index) { return mix[index]; }
u8 dComIfGs_getItem(u8 index, bool) { return inventory.at(index); }
void dComIfGs_setMixItemIndex(int index, u8 item) { mix[index] = item; }
u8 combine_select_item(u8 item, u8) { return item; }
void dComIfGs_setSelectItemIndex(int, u8);
void dComIfGs_onItemFirstBit(u8 type) { acquired[type] = true; }
void dComIfGs_setCollectSmell(u8 type) { scent = type; }

#include "sync.inc"
#include "assign.inc"
#include "scent.inc"

void dComIfGs_setSelectItemIndex(int index, u8 item) {
    std::uintptr_t args[]{reinterpret_cast<std::uintptr_t>(&status),
                          static_cast<std::uintptr_t>(index), item};
    if (on_set_select_item_index_pre(nullptr, args, nullptr, nullptr) == HOOK_CONTINUE)
        status.mSelectItem[index] = item;
}

int main() {
    dMsgObject_c message;
    inventory.fill(0xFF);
    inventory[4] = 0x43;
    inventory[12] = 0x46;
    mix[2] = 8;
    dComIfGs_setSelectItemIndex(2, 4);
    for (int i = dItemNo_SMELL_YELIA_POUCH_e; i <= dItemNo_SMELL_MEDICINE_e; ++i) {
        message.setSmellTypeLocal(static_cast<u8>(i));
        assert(acquired[i] && scent == i && message.mSmellType == i);
        sync_z_item_state();
        assert(status.mSelectItem[2] == 4 && g_zInventorySlot == 4);
        assert(g_zMixSlot == 8 && g_dComIfG_gameInfo.play.selected[2] == inventory[4]);
    }
    dComIfGs_setSelectItemIndex(2, 12);
    assert(g_zInventorySlot == 12 && g_dComIfG_gameInfo.play.selected[2] == inventory[12]);
    dComIfGs_setSelectItemIndex(2, 0xFF);
    assert(g_zInventorySlot == 0xFF && g_zMixSlot == 0xFF && status.mSelectItem[2] == 0xFF);
    message.setSmellTypeLocal(dItemNo_SMELL_YELIA_POUCH_e);
    assert(status.mSelectItem[2] == 0xFF);
    dComIfGs_setSelectItemIndex(0, 4);
    dComIfGs_setSelectItemIndex(1, 12);
    assert(status.mSelectItem[0] == 4 && status.mSelectItem[1] == 12);
    g_configCustomZButtonEnabled = false;
    message.setSmellTypeLocal(dItemNo_SMELL_MEDICINE_e);
    assert(status.mSelectItem[2] == dItemNo_SMELL_MEDICINE_e);
}
