#include <cassert>
#include <cstddef>
#include <cstdio>
using u8 = unsigned char;
using f32 = float;

struct dCamStyleData {
    struct StyleData { int algorithm = 8; float params[28]{}; };
};
struct dCamParam_c {
    dCamStyleData::StyleData* mCamStyleData = nullptr;
    int mStyleNum = 0;
    dCamStyleData::StyleData* mCurrentStyle = nullptr;
    int mStyleID = -1;
    int Algorythmn() const { return mCurrentStyle->algorithm; }
    float Val(int style, int index) const { return mCamStyleData[style].params[index]; }
    void SetVal(int style, int index, float value) { mCamStyleData[style].params[index] = value; }
};
struct camera_process_class {
    struct {
        dCamParam_c mCamParam;
        int mCamStyle = -1;
        struct { struct { float mLastPosX = 0, mLastPosY = 0; } mCStick; } mPadInfo;
    } mCamera;
};
struct daAlink_c {
    bool horse = false, boar = false;
    bool checkHorseRide() const { return horse; }
    bool checkBoarRide() const { return boar; }
};
camera_process_class* liveCam;
daAlink_c* liveLink;
camera_process_class* dComIfGp_getCamera(int) { return liveCam; }
daAlink_c* daAlink_getAlinkActorClass() { return liveLink; }

#include "staged_camera.inc"

constexpr u8 SLOT_11 = 11, SELECT_ITEM_B = 0, dItemNo_MILK_BOTTLE_e = 0x60,
             dItemNo_HALF_MILK_BOTTLE_e = 0x61, dItemNo_EMPTY_BOTTLE_e = 0x62,
             dItemNo_NONE_e = 0xff;
static u8 s_pendingSlot, s_pendingItem, s_preSelectIndex, s_preSelectPlay, s_assignedSlot;
u8 inventory[24], selectedIndex[3], selectedPlay[3];
int writes = 0;
u8 bottle_item(int idx) { return idx >= 0 && idx < 4 ? inventory[SLOT_11 + idx] : dItemNo_NONE_e; }
void dComIfGs_setItem(int idx, u8 item) { ++writes; inventory[idx] = item; }
u8 dComIfGs_getSelectItemIndex(int i) { return selectedIndex[i]; }
void dComIfGs_setSelectItemIndex(int i, u8 slot) { selectedIndex[i] = slot; }
u8 dComIfGp_getSelectItem(int i) { return selectedPlay[i]; }
void dComIfGp_setSelectItem(int i) {
    selectedPlay[i] = selectedIndex[i] < 24 ? inventory[selectedIndex[i]] : dItemNo_NONE_e;
}
struct {
    struct {
        void setSelectItem(int i, u8 item) { selectedPlay[i] = item; }
    } play;
} g_dComIfG_gameInfo;
void qb_log(const char*, ...) {}
// Mirrors the engine's first-match API used by .756, for the regression control.
void dComIfGs_setBottleItemIn(u8 current, u8 result) {
    for (int slot = 0; slot < 4; ++slot) {
        if (bottle_item(slot) == current) {
            dComIfGs_setItem(SLOT_11 + slot, result);
            return;
        }
    }
}

#include "staged_bottles.inc"

void selectStyle(camera_process_class& cam, int id) {
    auto& prm = cam.mCamera.mCamParam;
    cam.mCamera.mCamStyle = prm.mStyleID = id;
    prm.mCurrentStyle = &prm.mCamStyleData[id];
}
void initStyles(dCamStyleData::StyleData (&styles)[3], float base) {
    for (int s = 0; s < 3; ++s)
        for (int i = 0; i < 28; ++i) styles[s].params[i] = base + 100 * s + i;
}
void checkRates(const dCamStyleData::StyleData& style, float base, bool patched) {
    for (int i = 0; i < 28; ++i) {
        bool recenter = i == 22 || i == 24 || i == 27;
        assert(style.params[i] == (patched && recenter ? 0.f : base + i));
    }
}
void cameraTests() {
    dCamStyleData::StyleData first[3], next[3];
    initStyles(first, 10);
    initStyles(next, 50);
    camera_process_class cam;
    daAlink_c player;
    liveCam = &cam; liveLink = &player;
    auto& prm = cam.mCamera.mCamParam;
    prm.mCamStyleData = first; prm.mStyleNum = 3;
    selectStyle(cam, 1);
    player.boar = true;
    g_configHorseCamNoRecenter = true;
    update_horse_cam(); update_horse_cam();
    checkRates(first[1], 110, true);
    selectStyle(cam, 2);
    update_horse_cam();
    checkRates(first[1], 110, false);
    checkRates(first[2], 210, true);
    cam.mCamera.mPadInfo.mCStick.mLastPosY = -0.16f;
    update_horse_cam();
    checkRates(first[2], 210, false);
    cam.mCamera.mPadInfo.mCStick.mLastPosY = 0;
    player.boar = false; player.horse = true;
    update_horse_cam();
    checkRates(first[2], 210, true);
    liveLink = nullptr;
    update_horse_cam();
    checkRates(first[2], 210, false);
    liveLink = &player;
    update_horse_cam();
    liveCam = nullptr;
    update_horse_cam();
    liveCam = &cam;
    prm.mCamStyleData = next;
    selectStyle(cam, 2);
    update_horse_cam();
    checkRates(next[2], 250, true);
    shutdown_horse_cam();
    checkRates(next[2], 250, false);
    player.horse = false;
    update_horse_cam();
    checkRates(next[2], 250, false);
    player.boar = true; next[2].algorithm = 1;
    update_horse_cam();
    checkRates(next[2], 250, false);
    next[2].algorithm = 8; prm.mCurrentStyle = nullptr;
    update_horse_cam();
    checkRates(next[2], 250, false);
    selectStyle(cam, 2); cam.mCamera.mCamStyle = -1;
    update_horse_cam();
    checkRates(next[2], 250, false);
}
void bottleTests() {
    for (u8& item : inventory) item = dItemNo_NONE_e;
    constexpr u8 used = SLOT_11 + 2;
    inventory[SLOT_11] = inventory[used] = dItemNo_MILK_BOTTLE_e;
    s_pendingSlot = 2; s_pendingItem = dItemNo_MILK_BOTTLE_e;
    s_preSelectIndex = SLOT_11; s_preSelectPlay = dItemNo_MILK_BOTTLE_e;
    selectedIndex[0] = used; selectedIndex[1] = SLOT_11; selectedIndex[2] = used;
    for (int i = 0; i < 3; ++i) dComIfGp_setSelectItem(i);
    bottles_finish_pending();
    assert(bottle_item(0) == dItemNo_MILK_BOTTLE_e);
    assert(bottle_item(2) == dItemNo_HALF_MILK_BOTTLE_e);
    assert(selectedIndex[0] == SLOT_11 && selectedPlay[0] == dItemNo_MILK_BOTTLE_e);
    assert(selectedPlay[2] == dItemNo_HALF_MILK_BOTTLE_e && writes == 1);
    bottles_finish_pending();
    assert(writes == 1);
    s_pendingSlot = 2; s_pendingItem = dItemNo_HALF_MILK_BOTTLE_e;
    s_preSelectIndex = selectedIndex[0] = used; s_preSelectPlay = dItemNo_HALF_MILK_BOTTLE_e;
    bottles_finish_pending();
    assert(bottle_item(2) == dItemNo_EMPTY_BOTTLE_e && writes == 2);
    assert(selectedIndex[0] == used && selectedPlay[0] == dItemNo_EMPTY_BOTTLE_e);
    s_pendingSlot = 2; s_pendingItem = dItemNo_MILK_BOTTLE_e;
    bottles_finish_pending();
    assert(writes == 2);
    s_pendingSlot = 0; s_assignedSlot = 0;
    bottles_reset_assignment();
    assert(s_pendingSlot == 0xff && s_assignedSlot == 0xff);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    if (argv[1][0] == 'c') cameraTests();
    else bottleTests();
    return 0;
}
