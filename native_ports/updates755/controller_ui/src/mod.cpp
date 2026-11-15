#include "global.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <dolphin/dvd.h>
#include <pad.h>
#include "d/d_meter2_draw.h"
#include "d/actor/d_a_player.h"
#include "d/d_meter_button.h"
#include "d/d_file_select.h"
#include "d/d_msg_out_font.h"
#include "d/d_msg_object.h"
#include "d/d_meter_HIO.h"
#include "d/d_meter2_info.h"
#include "d/d_menu_ring.h"
#include "d/d_menu_item_explain.h"
#include "d/d_menu_collect.h"
#include "d/d_item_data.h"
#include "d/d_pane_class.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "mods/svc/config.h"
#include "mods/svc/ui.h"

#if defined(TP_CLASSIC_RELEASE_BUILD) && DEBUG
#error "TP Classic public Release builds must use DEBUG=0; DEBUG=1 changes the game structure ABI."
#endif

DEFINE_MOD();
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(HostService, svc_host);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

namespace {

#if defined(TP_CLASSIC_DEV_OPTIONS)
constexpr bool kDeveloperOptions = true;
#else
constexpr bool kDeveloperOptions = false;
#endif

ConfigVarHandle g_controllerStyle=0;
ConfigVarHandle g_layoutSchemaVersion=0;
ConfigVarHandle g_visualHudEditorEnabled=0;
// Legacy boolean kept only for migration from the first experimental X/Y build.
ConfigVarHandle g_swapXYButtonLayout=0;
ConfigVarHandle g_buttonLayoutPreset=0;
ConfigVarHandle g_inputSyncLastPreset=0;
ConfigVarHandle g_inputSyncControllerIndex=0;
bool s_controllerStyleLocked=false;
bool s_useXbox=false;
bool s_useSwitch=false;

// Developer-only Midna diagnostics. This records geometry only; it never
// changes the HUD. Export from CONTROLLER DEV > Tools after reproducing.
std::string s_midnaDiagnostic;
bool s_midnaDiagLastEvent=false;
int s_midnaDiagPostEventFrame=-1;

ConfigVarHandle g_triX=0, g_triY=0, g_triScale=0;
ConfigVarHandle g_squareX=0, g_squareY=0, g_squareScale=0;
// Alternate X/Y layout calibration. These handles are developer-only for now.
ConfigVarHandle g_swapTriX=0, g_swapTriY=0, g_swapTriScale=0;
ConfigVarHandle g_swapSquareX=0, g_swapSquareY=0, g_swapSquareScale=0;
ConfigVarHandle g_circleX=0, g_circleY=0, g_circleScale=0;

// Derived presets get their own calibration so editing one never mutates another.
// YB = Base with GC Y and GC B faces exchanged.
// XYXB = Swap X/Y with GC X and GC B faces exchanged.
ConfigVarHandle g_ybTriX=0, g_ybTriY=0, g_ybTriScale=0;
ConfigVarHandle g_ybSquareX=0, g_ybSquareY=0, g_ybSquareScale=0;
ConfigVarHandle g_ybCircleX=0, g_ybCircleY=0, g_ybCircleScale=0;
ConfigVarHandle g_xyxbTriX=0, g_xyxbTriY=0, g_xyxbTriScale=0;
ConfigVarHandle g_xyxbSquareX=0, g_xyxbSquareY=0, g_xyxbSquareScale=0;
ConfigVarHandle g_xyxbCircleX=0, g_xyxbCircleY=0, g_xyxbCircleScale=0;
ConfigVarHandle g_crossX=0, g_crossY=0, g_crossScale=0;
ConfigVarHandle g_showGuide=0;
ConfigVarHandle g_dpadShadowsEnabled=0, g_dpadArrowsEnabled=0;
ConfigVarHandle g_dpadX=0, g_dpadY=0, g_dpadScale=0;
ConfigVarHandle g_itemTextX=0, g_itemTextY=0, g_itemTextScale=0;
ConfigVarHandle g_mapTextX=0, g_mapTextY=0, g_mapTextScale=0;
ConfigVarHandle g_dpadMapAnimation=0;
ConfigVarHandle g_r1X=0, g_r1Y=0, g_r1Scale=0;
ConfigVarHandle g_fishingCheckX=0, g_fishingCheckY=0, g_fishingCheckScale=0;
ConfigVarHandle g_guideX=0, g_guideY=0, g_guideScale=0;
ConfigVarHandle g_itemsAnchorX=0, g_itemsAnchorY=0;
ConfigVarHandle g_itemSquareX=0, g_itemSquareY=0, g_itemSquareScale=0;
ConfigVarHandle g_itemTriangleX=0, g_itemTriangleY=0, g_itemTriangleScale=0;
ConfigVarHandle g_swapItemSquareX=0, g_swapItemSquareY=0, g_swapItemSquareScale=0;
ConfigVarHandle g_swapItemTriangleX=0, g_swapItemTriangleY=0, g_swapItemTriangleScale=0;
ConfigVarHandle g_ybItemSquareX=0, g_ybItemSquareY=0, g_ybItemSquareScale=0;
ConfigVarHandle g_ybItemTriangleX=0, g_ybItemTriangleY=0, g_ybItemTriangleScale=0;
ConfigVarHandle g_xyxbItemSquareX=0, g_xyxbItemSquareY=0, g_xyxbItemSquareScale=0;
ConfigVarHandle g_xyxbItemTriangleX=0, g_xyxbItemTriangleY=0, g_xyxbItemTriangleScale=0;
ConfigVarHandle g_itemCircleX=0, g_itemCircleY=0, g_itemCircleScale=0;
ConfigVarHandle g_itemR1X=0, g_itemR1Y=0, g_itemR1Scale=0;
ConfigVarHandle g_swordX=0, g_swordY=0, g_swordScale=0;
ConfigVarHandle g_itemSquareFlipH=0, g_itemSquareFlipV=0;
ConfigVarHandle g_itemTriangleFlipH=0, g_itemTriangleFlipV=0;
ConfigVarHandle g_swapItemSquareFlipH=0, g_swapItemSquareFlipV=0;
ConfigVarHandle g_swapItemTriangleFlipH=0, g_swapItemTriangleFlipV=0;
ConfigVarHandle g_ybItemSquareFlipH=0, g_ybItemSquareFlipV=0;
ConfigVarHandle g_ybItemTriangleFlipH=0, g_ybItemTriangleFlipV=0;
ConfigVarHandle g_xyxbItemSquareFlipH=0, g_xyxbItemSquareFlipV=0;
ConfigVarHandle g_xyxbItemTriangleFlipH=0, g_xyxbItemTriangleFlipV=0;
ConfigVarHandle g_swordFlipH=0, g_swordFlipV=0;
ConfigVarHandle g_ybSwordX=0, g_ybSwordY=0, g_ybSwordScale=0, g_ybSwordFlipH=0, g_ybSwordFlipV=0;
ConfigVarHandle g_xyxbSwordX=0, g_xyxbSwordY=0, g_xyxbSwordScale=0, g_xyxbSwordFlipH=0, g_xyxbSwordFlipV=0;
ConfigVarHandle g_midnaX=0, g_midnaY=0, g_midnaScale=0;
ConfigVarHandle g_actionTextX=0, g_actionTextY=0, g_actionTextScale=0;
ConfigVarHandle g_howlActionX=0,g_howlActionY=0,g_howlActionScale=0;
ConfigVarHandle g_swapHowlActionX=0,g_swapHowlActionY=0,g_swapHowlActionScale=0;
ConfigVarHandle g_ybHowlActionX=0,g_ybHowlActionY=0,g_ybHowlActionScale=0;
ConfigVarHandle g_xyxbHowlActionX=0,g_xyxbHowlActionY=0,g_xyxbHowlActionScale=0;
ConfigVarHandle g_shopActionX=0,g_shopActionY=0,g_shopActionScale=0;
ConfigVarHandle g_swapShopActionX=0,g_swapShopActionY=0,g_swapShopActionScale=0;
ConfigVarHandle g_ybShopActionX=0,g_ybShopActionY=0,g_ybShopActionScale=0;
ConfigVarHandle g_xyxbShopActionX=0,g_xyxbShopActionY=0,g_xyxbShopActionScale=0;
ConfigVarHandle g_howlBackX=0,g_howlBackY=0,g_howlBackScale=0;
ConfigVarHandle g_swapHowlBackX=0,g_swapHowlBackY=0,g_swapHowlBackScale=0;
ConfigVarHandle g_ybHowlBackX=0,g_ybHowlBackY=0,g_ybHowlBackScale=0;
ConfigVarHandle g_xyxbHowlBackX=0,g_xyxbHowlBackY=0,g_xyxbHowlBackScale=0;
ConfigVarHandle g_shopBackX=0,g_shopBackY=0,g_shopBackScale=0;
ConfigVarHandle g_swapShopBackX=0,g_swapShopBackY=0,g_swapShopBackScale=0;
ConfigVarHandle g_ybShopBackX=0,g_ybShopBackY=0,g_ybShopBackScale=0;
ConfigVarHandle g_xyxbShopBackX=0,g_xyxbShopBackY=0,g_xyxbShopBackScale=0;
ConfigVarHandle g_dialogActionTextX=0, g_dialogActionTextY=0;
ConfigVarHandle g_swapDialogActionTextX=0, g_swapDialogActionTextY=0;
ConfigVarHandle g_ybDialogActionTextX=0, g_ybDialogActionTextY=0;
ConfigVarHandle g_xyxbDialogActionTextX=0, g_xyxbDialogActionTextY=0;

ConfigVarHandle g_whistleActionX=0,g_whistleActionY=0,g_whistleActionScale=0;
ConfigVarHandle g_swapWhistleActionX=0,g_swapWhistleActionY=0,g_swapWhistleActionScale=0;
ConfigVarHandle g_ybWhistleActionX=0,g_ybWhistleActionY=0,g_ybWhistleActionScale=0;
ConfigVarHandle g_xyxbWhistleActionX=0,g_xyxbWhistleActionY=0,g_xyxbWhistleActionScale=0;
ConfigVarHandle g_whistleBackX=0,g_whistleBackY=0,g_whistleBackScale=0;
ConfigVarHandle g_swapWhistleBackX=0,g_swapWhistleBackY=0,g_swapWhistleBackScale=0;
ConfigVarHandle g_ybWhistleBackX=0,g_ybWhistleBackY=0,g_ybWhistleBackScale=0;
ConfigVarHandle g_xyxbWhistleBackX=0,g_xyxbWhistleBackY=0,g_xyxbWhistleBackScale=0;

ConfigVarHandle g_backTextX=0, g_backTextY=0, g_backTextScale=0;
ConfigVarHandle g_ybBackTextX=0, g_ybBackTextY=0, g_ybBackTextScale=0;
ConfigVarHandle g_xyxbBackTextX=0, g_xyxbBackTextY=0, g_xyxbBackTextScale=0;
ConfigVarHandle g_wolfSenseX=0, g_wolfSenseY=0, g_wolfSenseScale=0;
ConfigVarHandle g_wolfDigX=0, g_wolfDigY=0, g_wolfDigScale=0;
ConfigVarHandle g_swapWolfSenseX=0, g_swapWolfSenseY=0, g_swapWolfSenseScale=0;
ConfigVarHandle g_swapWolfDigX=0, g_swapWolfDigY=0, g_swapWolfDigScale=0;
ConfigVarHandle g_ybWolfSenseX=0, g_ybWolfSenseY=0, g_ybWolfSenseScale=0;
ConfigVarHandle g_ybWolfDigX=0, g_ybWolfDigY=0, g_ybWolfDigScale=0;
ConfigVarHandle g_xyxbWolfSenseX=0, g_xyxbWolfSenseY=0, g_xyxbWolfSenseScale=0;
ConfigVarHandle g_xyxbWolfDigX=0, g_xyxbWolfDigY=0, g_xyxbWolfDigScale=0;
ConfigVarHandle g_backButtonAnim=0, g_backTextAnim=0;
ConfigVarHandle g_swapBackButtonAnim=0, g_swapBackTextAnim=0;
ConfigVarHandle g_ybBackButtonAnim=0, g_ybBackTextAnim=0;
ConfigVarHandle g_xyxbBackButtonAnim=0, g_xyxbBackTextAnim=0;
ConfigVarHandle g_actionGlowEnabled=0, g_actionGlowX=0, g_actionGlowY=0, g_actionGlowScale=0;
ConfigVarHandle g_backGlowEnabled=0, g_backGlowX=0, g_backGlowY=0, g_backGlowScale=0;
ConfigVarHandle g_ybBackGlowEnabled=0, g_ybBackGlowX=0, g_ybBackGlowY=0, g_ybBackGlowScale=0;
ConfigVarHandle g_xyxbBackGlowEnabled=0, g_xyxbBackGlowX=0, g_xyxbBackGlowY=0, g_xyxbBackGlowScale=0;
ConfigVarHandle g_glowPreview=0;
// Wolf X/Y button light panes (x_light / y_light).
ConfigVarHandle g_wolfXGlowEnabled=0, g_wolfXGlowX=0, g_wolfXGlowY=0, g_wolfXGlowScale=0;
ConfigVarHandle g_wolfYGlowEnabled=0, g_wolfYGlowX=0, g_wolfYGlowY=0, g_wolfYGlowScale=0;
ConfigVarHandle g_swapWolfXGlowEnabled=0, g_swapWolfXGlowX=0, g_swapWolfXGlowY=0, g_swapWolfXGlowScale=0;
ConfigVarHandle g_swapWolfYGlowEnabled=0, g_swapWolfYGlowX=0, g_swapWolfYGlowY=0, g_swapWolfYGlowScale=0;
ConfigVarHandle g_ybWolfXGlowEnabled=0, g_ybWolfXGlowX=0, g_ybWolfXGlowY=0, g_ybWolfXGlowScale=0;
ConfigVarHandle g_ybWolfYGlowEnabled=0, g_ybWolfYGlowX=0, g_ybWolfYGlowY=0, g_ybWolfYGlowScale=0;
ConfigVarHandle g_xyxbWolfXGlowEnabled=0, g_xyxbWolfXGlowX=0, g_xyxbWolfXGlowY=0, g_xyxbWolfXGlowScale=0;
ConfigVarHandle g_xyxbWolfYGlowEnabled=0, g_xyxbWolfYGlowX=0, g_xyxbWolfYGlowY=0, g_xyxbWolfYGlowScale=0;
ConfigVarHandle g_wolfGlowPreview=0;
// Save/file-select button offsets. Stored as tenths of a pixel.
ConfigVarHandle g_fileCrossX=0, g_fileCrossY=0, g_fileCircleX=0, g_fileCircleY=0;
ConfigVarHandle g_saveCrossX=0, g_saveCrossY=0, g_saveCircleX=0, g_saveCircleY=0;
// Shared Collection-style menu prompts (A/Confirm and B/Back).
ConfigVarHandle g_menuCrossX=0, g_menuCrossY=0, g_menuCrossScale=0;
ConfigVarHandle g_menuCircleX=0, g_menuCircleY=0, g_menuCircleScale=0;
ConfigVarHandle g_menuConfirmTextX=0, g_menuConfirmTextY=0, g_menuConfirmTextScale=0;
ConfigVarHandle g_menuBackTextX=0, g_menuBackTextY=0, g_menuBackTextScale=0;
ConfigVarHandle g_menuPromptOrnament=0;
ConfigVarHandle g_menuOrnamentX=0, g_menuOrnamentY=0, g_menuOrnamentScale=0;
ConfigVarHandle g_mapCrossX=0, g_mapCrossY=0, g_mapCrossScale=0;
ConfigVarHandle g_mapCircleX=0, g_mapCircleY=0, g_mapCircleScale=0;
ConfigVarHandle g_mapConfirmTextX=0, g_mapConfirmTextY=0, g_mapConfirmTextScale=0;
ConfigVarHandle g_mapBackTextX=0, g_mapBackTextY=0, g_mapBackTextScale=0;
ConfigVarHandle g_dungeonMapCrossX=0;
ConfigVarHandle g_dungeonMapCrossY=0;
ConfigVarHandle g_dungeonMapCrossScale=0;
ConfigVarHandle g_dungeonMapCircleX=0;
ConfigVarHandle g_dungeonMapCircleY=0;
ConfigVarHandle g_dungeonMapCircleScale=0;
ConfigVarHandle g_dungeonMapConfirmTextX=0;
ConfigVarHandle g_dungeonMapConfirmTextY=0;
ConfigVarHandle g_dungeonMapConfirmTextScale=0;
ConfigVarHandle g_dungeonMapBackTextX=0;
ConfigVarHandle g_dungeonMapBackTextY=0;
ConfigVarHandle g_dungeonMapBackTextScale=0;
ConfigVarHandle g_mapOrnamentEnabled=0, g_mapOrnamentX=0, g_mapOrnamentY=0, g_mapOrnamentScale=0;
// Optional decorative overlay for the gameplay HUD. Independent from the calibration guide.
ConfigVarHandle g_hudOrnamentEnabled=0, g_hudOrnamentX=0, g_hudOrnamentY=0, g_hudOrnamentScale=0;
// Item Wheel assignment prompt transforms. Completely separate from gameplay HUD.
ConfigVarHandle g_wheelSquareX=0, g_wheelSquareY=0, g_wheelSquareScale=0;
ConfigVarHandle g_wheelTriangleX=0, g_wheelTriangleY=0, g_wheelTriangleScale=0;
ConfigVarHandle g_swapWheelSquareX=0, g_swapWheelSquareY=0, g_swapWheelSquareScale=0;
ConfigVarHandle g_swapWheelTriangleX=0, g_swapWheelTriangleY=0, g_swapWheelTriangleScale=0;
ConfigVarHandle g_ybWheelSquareX=0, g_ybWheelSquareY=0, g_ybWheelSquareScale=0;
ConfigVarHandle g_ybWheelTriangleX=0, g_ybWheelTriangleY=0, g_ybWheelTriangleScale=0;
ConfigVarHandle g_xyxbWheelSquareX=0, g_xyxbWheelSquareY=0, g_xyxbWheelSquareScale=0;
ConfigVarHandle g_xyxbWheelTriangleX=0, g_xyxbWheelTriangleY=0, g_xyxbWheelTriangleScale=0;
ConfigVarHandle g_wheelSelectAnalogX=0, g_wheelSelectAnalogY=0, g_wheelSelectAnalogScale=0;
ConfigVarHandle g_wheelDirectAnalogX=0, g_wheelDirectAnalogY=0, g_wheelDirectAnalogScale=0;
ConfigVarHandle g_wheelL2X=0, g_wheelL2Y=0, g_wheelL2Scale=0;
ConfigVarHandle g_wheelR2X=0, g_wheelR2Y=0, g_wheelR2Scale=0;
ConfigVarHandle g_WorldPortalTextX=0, g_WorldPortalTextY=0;
ConfigVarHandle g_WorldMoveTextX=0, g_WorldMoveTextY=0;
ConfigVarHandle g_WorldReturnTextX=0, g_WorldReturnTextY=0;
ConfigVarHandle g_worldAnalogX=0,g_worldAnalogY=0,g_worldDpadX=0,g_worldDpadY=0,g_worldArrows=0;
ConfigVarHandle g_worldArrowX=0,g_worldArrowY=0;
ConfigVarHandle g_worldR1X=0;
ConfigVarHandle g_worldR1Y=0;
ConfigVarHandle g_worldR1Scale=0;
ConfigVarHandle g_worldAnalogScale=0;
ConfigVarHandle g_worldDpadScale=0;
ConfigVarHandle g_worldArrowScale=0;
ConfigVarHandle g_WorldPortalTextScale=0;
ConfigVarHandle g_WorldMoveTextScale=0;
ConfigVarHandle g_WorldReturnTextScale=0;
UiMenuTabHandle g_menuTab=0;
UiMenuTabHandle g_devMenuTab=0;
UiWindowHandle g_publicWindow=0;
UiWindowHandle g_layoutWindow=0;
bool g_swordDiagLogged=false;
bool s_buttonXYHookInstalled=false;
bool s_buttonCrossHookInstalled=false;

// Developer visual editor foundation. This intentionally uses a strict whitelist:
// only TP Classic elements whose transforms are already backed by known config
// handles can ever become editable. Unknown J2D panes are never discovered or
// mutated by this system.
enum class VisualEditorTargetId : u8 {
    HudY,
    HudX,
    HudB,
    HudA,
    HudR1,
    HudDpad,
    WheelSquare,
    WheelTriangle,
    WheelL2,
    WheelR2,
    WorldR1,
    WorldAnalog,
    WorldDpad,
    DungeonConfirm,
    DungeonBack,
};

struct VisualEditorTarget {
    VisualEditorTargetId id;
    const char* label;
    ConfigVarHandle* x;
    ConfigVarHandle* y;
    ConfigVarHandle* scale;
};

VisualEditorTarget s_visualEditorTargets[] = {
    {VisualEditorTargetId::HudY,          "HUD Y / Square / X",       &g_triX,              &g_triY,              &g_triScale},
    {VisualEditorTargetId::HudX,          "HUD X / Triangle / Y",     &g_squareX,           &g_squareY,           &g_squareScale},
    {VisualEditorTargetId::HudB,          "HUD B / Circle / B",       &g_circleX,           &g_circleY,           &g_circleScale},
    {VisualEditorTargetId::HudA,          "HUD A / Cross / A",        &g_crossX,            &g_crossY,            &g_crossScale},
    {VisualEditorTargetId::HudR1,         "HUD Z / R1 / RB",          &g_r1X,               &g_r1Y,               &g_r1Scale},
    {VisualEditorTargetId::HudDpad,       "HUD D-Pad",                &g_dpadX,              &g_dpadY,              &g_dpadScale},
    {VisualEditorTargetId::WheelSquare,   "Item Wheel Square / X",    &g_wheelSquareX,      &g_wheelSquareY,      &g_wheelSquareScale},
    {VisualEditorTargetId::WheelTriangle, "Item Wheel Triangle / Y",  &g_wheelTriangleX,    &g_wheelTriangleY,    &g_wheelTriangleScale},
    {VisualEditorTargetId::WheelL2,       "Item Wheel L2 / LT",       &g_wheelL2X,          &g_wheelL2Y,          &g_wheelL2Scale},
    {VisualEditorTargetId::WheelR2,       "Item Wheel R2 / RT",       &g_wheelR2X,          &g_wheelR2Y,          &g_wheelR2Scale},
    {VisualEditorTargetId::WorldR1,       "World Map R1 / RB",        &g_worldR1X,          &g_worldR1Y,          &g_worldR1Scale},
    {VisualEditorTargetId::WorldAnalog,   "World Map L3 / LS",        &g_worldAnalogX,      &g_worldAnalogY,      &g_worldAnalogScale},
    {VisualEditorTargetId::WorldDpad,     "World Map D-Pad",          &g_worldDpadX,        &g_worldDpadY,        &g_worldDpadScale},
    {VisualEditorTargetId::DungeonConfirm,"Dungeon Map Confirm",      &g_dungeonMapCrossX,  &g_dungeonMapCrossY,  &g_dungeonMapCrossScale},
    {VisualEditorTargetId::DungeonBack,   "Dungeon Map Back",         &g_dungeonMapCircleX, &g_dungeonMapCircleY, &g_dungeonMapCircleScale},
};

// Switch gets a full shadow copy of the authored visual configuration. This
// keeps PlayStation/Xbox presets independent while Switch is calibrated in
// every HUD and menu context.
ConfigVarHandle g_swWorldR1X=0;
ConfigVarHandle g_swWorldR1Y=0;
ConfigVarHandle g_swWorldR1Scale=0;
ConfigVarHandle g_swWorldAnalogScale=0;
ConfigVarHandle g_swWorldDpadScale=0;
ConfigVarHandle g_swWorldArrowScale=0;
ConfigVarHandle g_swWorldPortalTextScale=0;
ConfigVarHandle g_swWorldMoveTextScale=0;
ConfigVarHandle g_swWorldReturnTextScale=0;
ConfigVarHandle g_swWorldArrowX=0;
ConfigVarHandle g_swWorldArrowY=0;
ConfigVarHandle g_swWorldAnalogX=0;
ConfigVarHandle g_swWorldAnalogY=0;
ConfigVarHandle g_swWorldDpadX=0;
ConfigVarHandle g_swWorldDpadY=0;
ConfigVarHandle g_swWorldPortalTextX=0;
ConfigVarHandle g_swWorldPortalTextY=0;
ConfigVarHandle g_swWorldMoveTextX=0;
ConfigVarHandle g_swWorldMoveTextY=0;
ConfigVarHandle g_swWorldReturnTextX=0;
ConfigVarHandle g_swWorldReturnTextY=0;
ConfigVarHandle g_swTriX=0;
ConfigVarHandle g_swTriY=0;
ConfigVarHandle g_swTriScale=0;
ConfigVarHandle g_swSquareX=0;
ConfigVarHandle g_swSquareY=0;
ConfigVarHandle g_swSquareScale=0;
ConfigVarHandle g_swCircleX=0;
ConfigVarHandle g_swCircleY=0;
ConfigVarHandle g_swCircleScale=0;
ConfigVarHandle g_swCrossX=0;
ConfigVarHandle g_swCrossY=0;
ConfigVarHandle g_swCrossScale=0;
ConfigVarHandle g_swFishingCheckX=0;
ConfigVarHandle g_swFishingCheckY=0;
ConfigVarHandle g_swFishingCheckScale=0;
ConfigVarHandle g_swR1X=0;
ConfigVarHandle g_swR1Y=0;
ConfigVarHandle g_swR1Scale=0;
ConfigVarHandle g_swGuideX=0;
ConfigVarHandle g_swGuideY=0;
ConfigVarHandle g_swGuideScale=0;
ConfigVarHandle g_swDpadX=0;
ConfigVarHandle g_swDpadY=0;
ConfigVarHandle g_swDpadScale=0;
ConfigVarHandle g_swItemTextX=0;
ConfigVarHandle g_swItemTextY=0;
ConfigVarHandle g_swItemTextScale=0;
ConfigVarHandle g_swMapTextX=0;
ConfigVarHandle g_swMapTextY=0;
ConfigVarHandle g_swMapTextScale=0;
ConfigVarHandle g_swItemsAnchorX=0;
ConfigVarHandle g_swItemsAnchorY=0;
ConfigVarHandle g_swItemSquareX=0;
ConfigVarHandle g_swItemSquareY=0;
ConfigVarHandle g_swItemSquareScale=0;
ConfigVarHandle g_swItemTriangleX=0;
ConfigVarHandle g_swItemTriangleY=0;
ConfigVarHandle g_swItemTriangleScale=0;
ConfigVarHandle g_swItemCircleX=0;
ConfigVarHandle g_swItemCircleY=0;
ConfigVarHandle g_swItemCircleScale=0;
ConfigVarHandle g_swItemR1X=0;
ConfigVarHandle g_swItemR1Y=0;
ConfigVarHandle g_swItemR1Scale=0;
ConfigVarHandle g_swSwordX=0;
ConfigVarHandle g_swSwordY=0;
ConfigVarHandle g_swSwordScale=0;
ConfigVarHandle g_swMidnaX=0;
ConfigVarHandle g_swMidnaY=0;
ConfigVarHandle g_swMidnaScale=0;
ConfigVarHandle g_swHowlActionX=0;
ConfigVarHandle g_swHowlActionY=0;
ConfigVarHandle g_swHowlActionScale=0;
ConfigVarHandle g_swShopActionX=0;
ConfigVarHandle g_swShopActionY=0;
ConfigVarHandle g_swShopActionScale=0;
ConfigVarHandle g_swHowlBackX=0;
ConfigVarHandle g_swHowlBackY=0;
ConfigVarHandle g_swHowlBackScale=0;
ConfigVarHandle g_swShopBackX=0;
ConfigVarHandle g_swShopBackY=0;
ConfigVarHandle g_swShopBackScale=0;
ConfigVarHandle g_swActionTextX=0;
ConfigVarHandle g_swActionTextY=0;
ConfigVarHandle g_swActionTextScale=0;
ConfigVarHandle g_swDialogActionTextX=0;
ConfigVarHandle g_swDialogActionTextY=0;
ConfigVarHandle g_swWhistleActionX=0;
ConfigVarHandle g_swWhistleActionY=0;
ConfigVarHandle g_swWhistleActionScale=0;
ConfigVarHandle g_swWhistleBackX=0;
ConfigVarHandle g_swWhistleBackY=0;
ConfigVarHandle g_swWhistleBackScale=0;
ConfigVarHandle g_swBackTextX=0;
ConfigVarHandle g_swBackTextY=0;
ConfigVarHandle g_swBackTextScale=0;
ConfigVarHandle g_swWolfSenseX=0;
ConfigVarHandle g_swWolfSenseY=0;
ConfigVarHandle g_swWolfSenseScale=0;
ConfigVarHandle g_swWolfDigX=0;
ConfigVarHandle g_swWolfDigY=0;
ConfigVarHandle g_swWolfDigScale=0;
ConfigVarHandle g_swActionGlowX=0;
ConfigVarHandle g_swActionGlowY=0;
ConfigVarHandle g_swActionGlowScale=0;
ConfigVarHandle g_swBackGlowX=0;
ConfigVarHandle g_swBackGlowY=0;
ConfigVarHandle g_swBackGlowScale=0;
ConfigVarHandle g_swWolfXGlowX=0;
ConfigVarHandle g_swWolfXGlowY=0;
ConfigVarHandle g_swWolfXGlowScale=0;
ConfigVarHandle g_swWolfYGlowX=0;
ConfigVarHandle g_swWolfYGlowY=0;
ConfigVarHandle g_swWolfYGlowScale=0;
ConfigVarHandle g_swFileCrossX=0;
ConfigVarHandle g_swFileCrossY=0;
ConfigVarHandle g_swFileCircleX=0;
ConfigVarHandle g_swFileCircleY=0;
ConfigVarHandle g_swSaveCrossX=0;
ConfigVarHandle g_swSaveCrossY=0;
ConfigVarHandle g_swSaveCircleX=0;
ConfigVarHandle g_swSaveCircleY=0;
ConfigVarHandle g_swMenuCrossX=0;
ConfigVarHandle g_swMenuCrossY=0;
ConfigVarHandle g_swMenuCrossScale=0;
ConfigVarHandle g_swMenuCircleX=0;
ConfigVarHandle g_swMenuCircleY=0;
ConfigVarHandle g_swMenuCircleScale=0;
ConfigVarHandle g_swMenuConfirmTextX=0;
ConfigVarHandle g_swMenuConfirmTextY=0;
ConfigVarHandle g_swMenuConfirmTextScale=0;
ConfigVarHandle g_swMenuBackTextX=0;
ConfigVarHandle g_swMenuBackTextY=0;
ConfigVarHandle g_swMenuBackTextScale=0;
ConfigVarHandle g_swMenuOrnamentX=0;
ConfigVarHandle g_swMenuOrnamentY=0;
ConfigVarHandle g_swMenuOrnamentScale=0;
ConfigVarHandle g_swMapCrossX=0;
ConfigVarHandle g_swMapCrossY=0;
ConfigVarHandle g_swMapCrossScale=0;
ConfigVarHandle g_swMapCircleX=0;
ConfigVarHandle g_swMapCircleY=0;
ConfigVarHandle g_swMapCircleScale=0;
ConfigVarHandle g_swMapConfirmTextX=0;
ConfigVarHandle g_swMapConfirmTextY=0;
ConfigVarHandle g_swMapConfirmTextScale=0;
ConfigVarHandle g_swMapBackTextX=0;
ConfigVarHandle g_swMapBackTextY=0;
ConfigVarHandle g_swMapBackTextScale=0;
ConfigVarHandle g_swMapOrnamentX=0;
ConfigVarHandle g_swMapOrnamentY=0;
ConfigVarHandle g_swMapOrnamentScale=0;
ConfigVarHandle g_swHudOrnamentX=0;
ConfigVarHandle g_swHudOrnamentY=0;
ConfigVarHandle g_swHudOrnamentScale=0;
ConfigVarHandle g_swWheelSquareX=0;
ConfigVarHandle g_swWheelSquareY=0;
ConfigVarHandle g_swWheelSquareScale=0;
ConfigVarHandle g_swWheelTriangleX=0;
ConfigVarHandle g_swWheelTriangleY=0;
ConfigVarHandle g_swWheelTriangleScale=0;
ConfigVarHandle g_swWheelSelectAnalogX=0;
ConfigVarHandle g_swWheelSelectAnalogY=0;
ConfigVarHandle g_swWheelSelectAnalogScale=0;
ConfigVarHandle g_swWheelDirectAnalogX=0;
ConfigVarHandle g_swWheelDirectAnalogY=0;
ConfigVarHandle g_swWheelDirectAnalogScale=0;
ConfigVarHandle g_swWheelL2X=0;
ConfigVarHandle g_swWheelL2Y=0;
ConfigVarHandle g_swWheelL2Scale=0;
ConfigVarHandle g_swWheelR2X=0;
ConfigVarHandle g_swWheelR2Y=0;
ConfigVarHandle g_swWheelR2Scale=0;
ConfigVarHandle g_swDungeonMapCrossX=0;
ConfigVarHandle g_swDungeonMapCrossY=0;
ConfigVarHandle g_swDungeonMapCrossScale=0;
ConfigVarHandle g_swDungeonMapCircleX=0;
ConfigVarHandle g_swDungeonMapCircleY=0;
ConfigVarHandle g_swDungeonMapCircleScale=0;
ConfigVarHandle g_swDungeonMapConfirmTextX=0;
ConfigVarHandle g_swDungeonMapConfirmTextY=0;
ConfigVarHandle g_swDungeonMapConfirmTextScale=0;
ConfigVarHandle g_swDungeonMapBackTextX=0;
ConfigVarHandle g_swDungeonMapBackTextY=0;
ConfigVarHandle g_swDungeonMapBackTextScale=0;
ConfigVarHandle g_swDpadShadowsEnabled=0;
ConfigVarHandle g_swWorldArrows=0;
ConfigVarHandle g_swDpadArrowsEnabled=0;
ConfigVarHandle g_swDpadMapAnimation=0;
ConfigVarHandle g_swActionGlowEnabled=0;
ConfigVarHandle g_swBackGlowEnabled=0;
ConfigVarHandle g_swGlowPreview=0;
ConfigVarHandle g_swWolfXGlowEnabled=0;
ConfigVarHandle g_swWolfYGlowEnabled=0;
ConfigVarHandle g_swWolfGlowPreview=0;
ConfigVarHandle g_swBackButtonAnim=0;
ConfigVarHandle g_swBackTextAnim=0;
ConfigVarHandle g_swMenuPromptOrnament=0;
ConfigVarHandle g_swMapOrnamentEnabled=0;
ConfigVarHandle g_swHudOrnamentEnabled=0;
ConfigVarHandle g_swItemSquareFlipH=0;
ConfigVarHandle g_swItemSquareFlipV=0;
ConfigVarHandle g_swItemTriangleFlipH=0;
ConfigVarHandle g_swItemTriangleFlipV=0;
ConfigVarHandle g_swSwordFlipH=0;
ConfigVarHandle g_swSwordFlipV=0;

int64_t raw_controller_style() {
    int64_t v=0;
    if (g_controllerStyle==0 || svc_config->get_int(mod_ctx,g_controllerStyle,&v)!=MOD_OK) return 0;
    return v;
}
bool switch_controller_selected_now() {
    // A saved design change is pending until restart once textures are in use.
    // Keep the active calibration paired with the latched texture pack.
    return s_controllerStyleLocked ? s_useSwitch : raw_controller_style()==2;
}

ConfigVarHandle switch_variant(ConfigVarHandle h) {
    if (!switch_controller_selected_now()) return h;
    if (h==g_worldR1X) return g_swWorldR1X;
    if (h==g_worldR1Y) return g_swWorldR1Y;
    if (h==g_worldR1Scale) return g_swWorldR1Scale;
    if (h==g_worldAnalogScale) return g_swWorldAnalogScale;
    if (h==g_worldDpadScale) return g_swWorldDpadScale;
    if (h==g_worldArrowScale) return g_swWorldArrowScale;
    if (h==g_WorldPortalTextScale) return g_swWorldPortalTextScale;
    if (h==g_WorldMoveTextScale) return g_swWorldMoveTextScale;
    if (h==g_WorldReturnTextScale) return g_swWorldReturnTextScale;
    if (h==g_worldArrowX) return g_swWorldArrowX;
    if (h==g_worldArrowY) return g_swWorldArrowY;
    if (h==g_worldAnalogX) return g_swWorldAnalogX;
    if (h==g_worldAnalogY) return g_swWorldAnalogY;
    if (h==g_worldDpadX) return g_swWorldDpadX;
    if (h==g_worldDpadY) return g_swWorldDpadY;
    if (h==g_WorldPortalTextX) return g_swWorldPortalTextX;
    if (h==g_WorldPortalTextY) return g_swWorldPortalTextY;
    if (h==g_WorldMoveTextX) return g_swWorldMoveTextX;
    if (h==g_WorldMoveTextY) return g_swWorldMoveTextY;
    if (h==g_WorldReturnTextX) return g_swWorldReturnTextX;
    if (h==g_WorldReturnTextY) return g_swWorldReturnTextY;
    if (h==g_triX) return g_swTriX;
    if (h==g_triY) return g_swTriY;
    if (h==g_triScale) return g_swTriScale;
    if (h==g_squareX) return g_swSquareX;
    if (h==g_squareY) return g_swSquareY;
    if (h==g_squareScale) return g_swSquareScale;
    if (h==g_circleX) return g_swCircleX;
    if (h==g_circleY) return g_swCircleY;
    if (h==g_circleScale) return g_swCircleScale;
    if (h==g_crossX) return g_swCrossX;
    if (h==g_crossY) return g_swCrossY;
    if (h==g_crossScale) return g_swCrossScale;
    if (h==g_fishingCheckX) return g_swFishingCheckX;
    if (h==g_fishingCheckY) return g_swFishingCheckY;
    if (h==g_fishingCheckScale) return g_swFishingCheckScale;
    if (h==g_r1X) return g_swR1X;
    if (h==g_r1Y) return g_swR1Y;
    if (h==g_r1Scale) return g_swR1Scale;
    if (h==g_guideX) return g_swGuideX;
    if (h==g_guideY) return g_swGuideY;
    if (h==g_guideScale) return g_swGuideScale;
    if (h==g_dpadX) return g_swDpadX;
    if (h==g_dpadY) return g_swDpadY;
    if (h==g_dpadScale) return g_swDpadScale;
    if (h==g_itemTextX) return g_swItemTextX;
    if (h==g_itemTextY) return g_swItemTextY;
    if (h==g_itemTextScale) return g_swItemTextScale;
    if (h==g_mapTextX) return g_swMapTextX;
    if (h==g_mapTextY) return g_swMapTextY;
    if (h==g_mapTextScale) return g_swMapTextScale;
    if (h==g_itemsAnchorX) return g_swItemsAnchorX;
    if (h==g_itemsAnchorY) return g_swItemsAnchorY;
    if (h==g_itemSquareX) return g_swItemSquareX;
    if (h==g_itemSquareY) return g_swItemSquareY;
    if (h==g_itemSquareScale) return g_swItemSquareScale;
    if (h==g_itemTriangleX) return g_swItemTriangleX;
    if (h==g_itemTriangleY) return g_swItemTriangleY;
    if (h==g_itemTriangleScale) return g_swItemTriangleScale;
    if (h==g_itemCircleX) return g_swItemCircleX;
    if (h==g_itemCircleY) return g_swItemCircleY;
    if (h==g_itemCircleScale) return g_swItemCircleScale;
    if (h==g_itemR1X) return g_swItemR1X;
    if (h==g_itemR1Y) return g_swItemR1Y;
    if (h==g_itemR1Scale) return g_swItemR1Scale;
    if (h==g_swordX) return g_swSwordX;
    if (h==g_swordY) return g_swSwordY;
    if (h==g_swordScale) return g_swSwordScale;
    if (h==g_midnaX) return g_swMidnaX;
    if (h==g_midnaY) return g_swMidnaY;
    if (h==g_midnaScale) return g_swMidnaScale;
    if (h==g_howlActionX) return g_swHowlActionX;
    if (h==g_howlActionY) return g_swHowlActionY;
    if (h==g_howlActionScale) return g_swHowlActionScale;
    if (h==g_shopActionX) return g_swShopActionX;
    if (h==g_shopActionY) return g_swShopActionY;
    if (h==g_shopActionScale) return g_swShopActionScale;
    if (h==g_howlBackX) return g_swHowlBackX;
    if (h==g_howlBackY) return g_swHowlBackY;
    if (h==g_howlBackScale) return g_swHowlBackScale;
    if (h==g_shopBackX) return g_swShopBackX;
    if (h==g_shopBackY) return g_swShopBackY;
    if (h==g_shopBackScale) return g_swShopBackScale;
    if (h==g_actionTextX) return g_swActionTextX;
    if (h==g_actionTextY) return g_swActionTextY;
    if (h==g_actionTextScale) return g_swActionTextScale;
    if (h==g_dialogActionTextX) return g_swDialogActionTextX;
    if (h==g_dialogActionTextY) return g_swDialogActionTextY;
    if (h==g_whistleActionX) return g_swWhistleActionX;
    if (h==g_whistleActionY) return g_swWhistleActionY;
    if (h==g_whistleActionScale) return g_swWhistleActionScale;
    if (h==g_whistleBackX) return g_swWhistleBackX;
    if (h==g_whistleBackY) return g_swWhistleBackY;
    if (h==g_whistleBackScale) return g_swWhistleBackScale;
    if (h==g_backTextX) return g_swBackTextX;
    if (h==g_backTextY) return g_swBackTextY;
    if (h==g_backTextScale) return g_swBackTextScale;
    if (h==g_wolfSenseX) return g_swWolfSenseX;
    if (h==g_wolfSenseY) return g_swWolfSenseY;
    if (h==g_wolfSenseScale) return g_swWolfSenseScale;
    if (h==g_wolfDigX) return g_swWolfDigX;
    if (h==g_wolfDigY) return g_swWolfDigY;
    if (h==g_wolfDigScale) return g_swWolfDigScale;
    if (h==g_actionGlowX) return g_swActionGlowX;
    if (h==g_actionGlowY) return g_swActionGlowY;
    if (h==g_actionGlowScale) return g_swActionGlowScale;
    if (h==g_backGlowX) return g_swBackGlowX;
    if (h==g_backGlowY) return g_swBackGlowY;
    if (h==g_backGlowScale) return g_swBackGlowScale;
    if (h==g_wolfXGlowX) return g_swWolfXGlowX;
    if (h==g_wolfXGlowY) return g_swWolfXGlowY;
    if (h==g_wolfXGlowScale) return g_swWolfXGlowScale;
    if (h==g_wolfYGlowX) return g_swWolfYGlowX;
    if (h==g_wolfYGlowY) return g_swWolfYGlowY;
    if (h==g_wolfYGlowScale) return g_swWolfYGlowScale;
    if (h==g_fileCrossX) return g_swFileCrossX;
    if (h==g_fileCrossY) return g_swFileCrossY;
    if (h==g_fileCircleX) return g_swFileCircleX;
    if (h==g_fileCircleY) return g_swFileCircleY;
    if (h==g_saveCrossX) return g_swSaveCrossX;
    if (h==g_saveCrossY) return g_swSaveCrossY;
    if (h==g_saveCircleX) return g_swSaveCircleX;
    if (h==g_saveCircleY) return g_swSaveCircleY;
    if (h==g_menuCrossX) return g_swMenuCrossX;
    if (h==g_menuCrossY) return g_swMenuCrossY;
    if (h==g_menuCrossScale) return g_swMenuCrossScale;
    if (h==g_menuCircleX) return g_swMenuCircleX;
    if (h==g_menuCircleY) return g_swMenuCircleY;
    if (h==g_menuCircleScale) return g_swMenuCircleScale;
    if (h==g_menuConfirmTextX) return g_swMenuConfirmTextX;
    if (h==g_menuConfirmTextY) return g_swMenuConfirmTextY;
    if (h==g_menuConfirmTextScale) return g_swMenuConfirmTextScale;
    if (h==g_menuBackTextX) return g_swMenuBackTextX;
    if (h==g_menuBackTextY) return g_swMenuBackTextY;
    if (h==g_menuBackTextScale) return g_swMenuBackTextScale;
    if (h==g_menuOrnamentX) return g_swMenuOrnamentX;
    if (h==g_menuOrnamentY) return g_swMenuOrnamentY;
    if (h==g_menuOrnamentScale) return g_swMenuOrnamentScale;
    if (h==g_mapCrossX) return g_swMapCrossX;
    if (h==g_mapCrossY) return g_swMapCrossY;
    if (h==g_mapCrossScale) return g_swMapCrossScale;
    if (h==g_mapCircleX) return g_swMapCircleX;
    if (h==g_mapCircleY) return g_swMapCircleY;
    if (h==g_mapCircleScale) return g_swMapCircleScale;
    if (h==g_mapConfirmTextX) return g_swMapConfirmTextX;
    if (h==g_mapConfirmTextY) return g_swMapConfirmTextY;
    if (h==g_mapConfirmTextScale) return g_swMapConfirmTextScale;
    if (h==g_mapBackTextX) return g_swMapBackTextX;
    if (h==g_mapBackTextY) return g_swMapBackTextY;
    if (h==g_mapBackTextScale) return g_swMapBackTextScale;
    if (h==g_mapOrnamentX) return g_swMapOrnamentX;
    if (h==g_mapOrnamentY) return g_swMapOrnamentY;
    if (h==g_mapOrnamentScale) return g_swMapOrnamentScale;
    if (h==g_hudOrnamentX) return g_swHudOrnamentX;
    if (h==g_hudOrnamentY) return g_swHudOrnamentY;
    if (h==g_hudOrnamentScale) return g_swHudOrnamentScale;
    if (h==g_wheelSquareX) return g_swWheelSquareX;
    if (h==g_wheelSquareY) return g_swWheelSquareY;
    if (h==g_wheelSquareScale) return g_swWheelSquareScale;
    if (h==g_wheelTriangleX) return g_swWheelTriangleX;
    if (h==g_wheelTriangleY) return g_swWheelTriangleY;
    if (h==g_wheelTriangleScale) return g_swWheelTriangleScale;
    if (h==g_wheelSelectAnalogX) return g_swWheelSelectAnalogX;
    if (h==g_wheelSelectAnalogY) return g_swWheelSelectAnalogY;
    if (h==g_wheelSelectAnalogScale) return g_swWheelSelectAnalogScale;
    if (h==g_wheelDirectAnalogX) return g_swWheelDirectAnalogX;
    if (h==g_wheelDirectAnalogY) return g_swWheelDirectAnalogY;
    if (h==g_wheelDirectAnalogScale) return g_swWheelDirectAnalogScale;
    if (h==g_wheelL2X) return g_swWheelL2X;
    if (h==g_wheelL2Y) return g_swWheelL2Y;
    if (h==g_wheelL2Scale) return g_swWheelL2Scale;
    if (h==g_wheelR2X) return g_swWheelR2X;
    if (h==g_wheelR2Y) return g_swWheelR2Y;
    if (h==g_wheelR2Scale) return g_swWheelR2Scale;
    if (h==g_dungeonMapCrossX) return g_swDungeonMapCrossX;
    if (h==g_dungeonMapCrossY) return g_swDungeonMapCrossY;
    if (h==g_dungeonMapCrossScale) return g_swDungeonMapCrossScale;
    if (h==g_dungeonMapCircleX) return g_swDungeonMapCircleX;
    if (h==g_dungeonMapCircleY) return g_swDungeonMapCircleY;
    if (h==g_dungeonMapCircleScale) return g_swDungeonMapCircleScale;
    if (h==g_dungeonMapConfirmTextX) return g_swDungeonMapConfirmTextX;
    if (h==g_dungeonMapConfirmTextY) return g_swDungeonMapConfirmTextY;
    if (h==g_dungeonMapConfirmTextScale) return g_swDungeonMapConfirmTextScale;
    if (h==g_dungeonMapBackTextX) return g_swDungeonMapBackTextX;
    if (h==g_dungeonMapBackTextY) return g_swDungeonMapBackTextY;
    if (h==g_dungeonMapBackTextScale) return g_swDungeonMapBackTextScale;
    if (h==g_dpadShadowsEnabled) return g_swDpadShadowsEnabled;
    if (h==g_worldArrows) return g_swWorldArrows;
    if (h==g_dpadArrowsEnabled) return g_swDpadArrowsEnabled;
    if (h==g_dpadMapAnimation) return g_swDpadMapAnimation;
    if (h==g_actionGlowEnabled) return g_swActionGlowEnabled;
    if (h==g_backGlowEnabled) return g_swBackGlowEnabled;
    if (h==g_glowPreview) return g_swGlowPreview;
    if (h==g_wolfXGlowEnabled) return g_swWolfXGlowEnabled;
    if (h==g_wolfYGlowEnabled) return g_swWolfYGlowEnabled;
    if (h==g_wolfGlowPreview) return g_swWolfGlowPreview;
    if (h==g_backButtonAnim) return g_swBackButtonAnim;
    if (h==g_backTextAnim) return g_swBackTextAnim;
    if (h==g_menuPromptOrnament) return g_swMenuPromptOrnament;
    if (h==g_mapOrnamentEnabled) return g_swMapOrnamentEnabled;
    if (h==g_hudOrnamentEnabled) return g_swHudOrnamentEnabled;
    if (h==g_itemSquareFlipH) return g_swItemSquareFlipH;
    if (h==g_itemSquareFlipV) return g_swItemSquareFlipV;
    if (h==g_itemTriangleFlipH) return g_swItemTriangleFlipH;
    if (h==g_itemTriangleFlipV) return g_swItemTriangleFlipV;
    if (h==g_swordFlipH) return g_swSwordFlipH;
    if (h==g_swordFlipV) return g_swSwordFlipV;
    return h;
}

int64_t cfg_int(ConfigVarHandle h, int64_t fallback) {
    h=switch_variant(h);
    int64_t v=fallback;
    if (h==0 || svc_config->get_int(mod_ctx,h,&v)!=MOD_OK) return fallback;
    return v;
}
float cfg_pos(ConfigVarHandle h, float fallback) {
    return static_cast<float>(cfg_int(h, static_cast<int64_t>(fallback*10.0f))) / 10.0f;
}
float cfg_scale(ConfigVarHandle h, float fallback) {
    return static_cast<float>(cfg_int(h, static_cast<int64_t>(fallback*100.0f))) / 100.0f;
}
bool cfg_bool(ConfigVarHandle h, bool fallback) {
    h=switch_variant(h);
    bool v=fallback;
    if (h==0 || svc_config->get_bool(mod_ctx,h,&v)!=MOD_OK) return fallback;
    return v;
}

bool visual_editor_enabled() {
    return kDeveloperOptions && cfg_bool(g_visualHudEditorEnabled,false);
}

enum class ButtonLayoutPreset : int64_t {
    Base = 0,
    SwapXY = 1,
    SwapYB = 2,
    SwapXYXB = 3,
};

ButtonLayoutPreset current_layout_preset() {
    // Switch always uses its native B/A/Y/X physical arrangement. The persisted
    // PS/Xbox preset is left untouched and becomes active again when returning
    // to those controller designs.
    if (switch_controller_selected_now()) return ButtonLayoutPreset::Base;
    // Preset selection is a public feature. Only the calibration/editor UI is
    // developer-only; release builds must honor the persisted preset as well.
    if (g_buttonLayoutPreset == 0) return ButtonLayoutPreset::Base;
    int64_t raw=cfg_int(g_buttonLayoutPreset,0);
    if (raw < 0 || raw > 3) raw=0;
    return static_cast<ButtonLayoutPreset>(raw);
}

bool swap_xy_layout_enabled() {
    const ButtonLayoutPreset p=current_layout_preset();
    return p==ButtonLayoutPreset::SwapXY || p==ButtonLayoutPreset::SwapXYXB;
}

ConfigVarHandle layout_handle4(ConfigVarHandle base, ConfigVarHandle swapXY,
                               ConfigVarHandle swapYB, ConfigVarHandle swapXYXB) {
    if (switch_controller_selected_now()) return switch_variant(base);
    switch(current_layout_preset()) {
    case ButtonLayoutPreset::SwapXY:   return swapXY!=0 ? swapXY : base;
    case ButtonLayoutPreset::SwapYB:   return swapYB!=0 ? swapYB : base;
    case ButtonLayoutPreset::SwapXYXB: return swapXYXB!=0 ? swapXYXB : (swapXY!=0 ? swapXY : base);
    default:                            return base;
    }
}

ConfigVarHandle layout_handle(ConfigVarHandle normal, ConfigVarHandle swapped) {
    return layout_handle4(normal,swapped,normal,swapped);
}

ModResult reg_bool(const char* name, bool def, ConfigVarHandle& out, ModError* err) {
    ConfigVarDesc d=CONFIG_VAR_DESC_INIT;
    d.name=name; d.type=CONFIG_VAR_BOOL; d.default_bool=def;
    ModResult r=svc_config->register_var(mod_ctx,&d,&out);
    if(r!=MOD_OK) return mods::set_error(err,r,"failed to register layout option");
    return MOD_OK;
}
ModResult reg_int(const char* name, int64_t def, ConfigVarHandle& out, ModError* err) {
    ConfigVarDesc d=CONFIG_VAR_DESC_INIT;
    d.name=name; d.type=CONFIG_VAR_INT; d.default_int=def;
    ModResult r=svc_config->register_var(mod_ctx,&d,&out);
    if(r!=MOD_OK) return mods::set_error(err,r,"failed to register layout option");
    return MOD_OK;
}

// One-time compatibility migration for users updating from the original 1.0.0
// layout. Dusklight persists config vars, so changing a registered default alone
// does not move an existing user's HUD to the new calibrated positions.
//
// Preserve real customizations: a value is migrated only when it still exactly
// matches the old published default. New installs already start on the new
// defaults and therefore pass through unchanged.
void migrate_int_default(ConfigVarHandle handle, int64_t oldDefault, int64_t newDefault,
                         int& migratedCount) {
    if (handle == 0 || oldDefault == newDefault) return;
    int64_t value = 0;
    if (svc_config->get_int(mod_ctx, handle, &value) != MOD_OK) return;
    if (value != oldDefault) return;
    if (svc_config->set_int(mod_ctx, handle, newDefault) == MOD_OK)
        ++migratedCount;
}

void apply_layout_schema_migrations() {
    if (g_layoutSchemaVersion == 0) return;

    int64_t schemaVersion = 0;
    if (svc_config->get_int(mod_ctx, g_layoutSchemaVersion, &schemaVersion) != MOD_OK)
        return;
    int migratedCount = 0;

    if (schemaVersion < 1) {
    // 1.0.0 -> 1.1.0 calibrated defaults.
    migrate_int_default(g_worldR1X,              0,    80, migratedCount);
    migrate_int_default(g_worldR1Y,            -80,   180, migratedCount);
    migrate_int_default(g_worldArrowX,          210,  -225, migratedCount);
    migrate_int_default(g_worldArrowY,            0,  -230, migratedCount);
    migrate_int_default(g_worldAnalogX,         150,   180, migratedCount);
    migrate_int_default(g_worldAnalogY,         -10,   680, migratedCount);
    migrate_int_default(g_worldDpadX,          -190,   260, migratedCount);
    migrate_int_default(g_worldDpadY,             0,   220, migratedCount);

    migrate_int_default(g_wheelSquareX,           0,  -170, migratedCount);
    migrate_int_default(g_wheelSquareScale,     100,    87, migratedCount);
    migrate_int_default(g_wheelTriangleX,         0,  -100, migratedCount);
    migrate_int_default(g_wheelTriangleScale,   100,    90, migratedCount);
    migrate_int_default(g_wheelSelectAnalogX,     0,    50, migratedCount);
    migrate_int_default(g_wheelSelectAnalogScale,100,    90, migratedCount);
    migrate_int_default(g_wheelDirectAnalogX,     0,   100, migratedCount);
    migrate_int_default(g_wheelDirectAnalogScale,100,    90, migratedCount);
    migrate_int_default(g_wheelL2X,              80,   250, migratedCount);
    migrate_int_default(g_wheelL2Scale,         100,    90, migratedCount);
    migrate_int_default(g_wheelR2X,               0,  -130, migratedCount);

    svc_config->set_int(mod_ctx, g_layoutSchemaVersion, 1);
    schemaVersion = 1;
    }

    // Schema 2 corrects the normal Wolf glow calibration. The previous published
    // defaults are preserved as the alternate Swap X/Y profile instead.
    if (schemaVersion < 2) {
        migrate_int_default(g_wolfXGlowX, 5, 235, migratedCount);
        migrate_int_default(g_wolfXGlowY, -55, -295, migratedCount);
        migrate_int_default(g_wolfYGlowX, -75, -300, migratedCount);
        migrate_int_default(g_wolfYGlowY, 20, 260, migratedCount);
        svc_config->set_int(mod_ctx, g_layoutSchemaVersion, 2);
        schemaVersion = 2;
    }

    if (schemaVersion < 3 && kDeveloperOptions) {
        // Preserve the first experimental boolean selection when upgrading to the enum.
        if (g_swapXYButtonLayout != 0 && g_buttonLayoutPreset != 0 &&
            cfg_bool(g_swapXYButtonLayout,false) && cfg_int(g_buttonLayoutPreset,0)==0)
            svc_config->set_int(mod_ctx,g_buttonLayoutPreset,1);
        svc_config->set_int(mod_ctx,g_layoutSchemaVersion,3);
        schemaVersion=3;
    }

    if (schemaVersion < 4) {
        migrate_int_default(g_triX,960,962,migratedCount);
        migrate_int_default(g_triY,386,387,migratedCount);
        migrate_int_default(g_squareX,1217,1218,migratedCount);
        migrate_int_default(g_wolfSenseY,-770,-760,migratedCount);
        migrate_int_default(g_wolfDigX,500,490,migratedCount);
        migrate_int_default(g_wolfDigY,570,575,migratedCount);
        migrate_int_default(g_wolfXGlowX,235,5,migratedCount);
        migrate_int_default(g_wolfXGlowY,-295,-55,migratedCount);
        migrate_int_default(g_wolfYGlowX,-300,-70,migratedCount);
        migrate_int_default(g_wolfYGlowY,260,20,migratedCount);
        if (kDeveloperOptions) {
            migrate_int_default(g_swapTriX,1210,1208,migratedCount);
            migrate_int_default(g_swapSquareX,972,971,migratedCount);
            migrate_int_default(g_swapSquareY,379,380,migratedCount);
            migrate_int_default(g_swapItemSquareX,-670,-690,migratedCount);
        }
        svc_config->set_int(mod_ctx,g_layoutSchemaVersion,4);
        schemaVersion=4;
    }

    if (schemaVersion < 5) {
        migrate_int_default(g_whistleActionX,250,460,migratedCount);
        migrate_int_default(g_whistleActionY,220,840,migratedCount);
        migrate_int_default(g_whistleBackX,820,910,migratedCount);
        migrate_int_default(g_whistleBackY,-350,310,migratedCount);

        migrate_int_default(g_swapWhistleActionX,250,460,migratedCount);
        migrate_int_default(g_swapWhistleActionY,220,840,migratedCount);
        migrate_int_default(g_swapWhistleBackX,820,910,migratedCount);
        migrate_int_default(g_swapWhistleBackY,-350,310,migratedCount);

        migrate_int_default(g_ybTriX,962,1467,migratedCount);
        migrate_int_default(g_ybCircleX,1505,997,migratedCount);
        migrate_int_default(g_ybItemTriangleX,220,880,migratedCount);
        migrate_int_default(g_ybSwordX,83,14,migratedCount);
        migrate_int_default(g_ybSwordY,-52,-40,migratedCount);
        migrate_int_default(g_ybBackTextX,820,310,migratedCount);
        migrate_int_default(g_ybBackTextY,-350,-345,migratedCount);
        migrate_int_default(g_ybWhistleActionX,250,460,migratedCount);
        migrate_int_default(g_ybWhistleActionY,220,840,migratedCount);
        migrate_int_default(g_ybWhistleBackX,820,410,migratedCount);
        migrate_int_default(g_ybWhistleBackY,-350,300,migratedCount);
        migrate_int_default(g_ybHowlBackX,910,400,migratedCount);
        migrate_int_default(g_ybShopBackX,900,400,migratedCount);
        migrate_int_default(g_ybWolfDigX,490,1000,migratedCount);
        migrate_int_default(g_ybWheelSquareX,-170,-430,migratedCount);
        migrate_int_default(g_ybWheelTriangleX,-100,160,migratedCount);

        migrate_int_default(g_xyxbCircleX,1505,997,migratedCount);
        migrate_int_default(g_xyxbSquareX,971,1477,migratedCount);
        migrate_int_default(g_xyxbSquareY,380,379,migratedCount);
        migrate_int_default(g_xyxbItemSquareX,-690,-30,migratedCount);
        migrate_int_default(g_xyxbSwordX,83,14,migratedCount);
        migrate_int_default(g_xyxbSwordY,-52,-40,migratedCount);
        migrate_int_default(g_xyxbBackTextX,820,310,migratedCount);
        migrate_int_default(g_xyxbBackTextY,-350,-345,migratedCount);
        migrate_int_default(g_xyxbWolfSenseX,-920,-400,migratedCount);
        migrate_int_default(g_xyxbWhistleActionX,250,460,migratedCount);
        migrate_int_default(g_xyxbWhistleActionY,220,840,migratedCount);
        migrate_int_default(g_xyxbWhistleBackX,820,410,migratedCount);
        migrate_int_default(g_xyxbWhistleBackY,-350,300,migratedCount);
        migrate_int_default(g_xyxbHowlBackX,910,400,migratedCount);
        migrate_int_default(g_xyxbShopBackX,900,400,migratedCount);
        migrate_int_default(g_xyxbWheelSquareX,-430,-170,migratedCount);
        migrate_int_default(g_xyxbWheelTriangleX,160,-100,migratedCount);

        svc_config->set_int(mod_ctx,g_layoutSchemaVersion,5);
        schemaVersion=5;
    }

    if (svc_log != nullptr && migratedCount > 0)
        svc_log->info(mod_ctx, "Layout config migrated to latest schema");
}
void add_num(UiElementHandle pane,const char* label,ConfigVarHandle h,int64_t mn,int64_t mx,int64_t step,const char* suffix,const char* help) {
    h=switch_variant(h);
    UiControlDesc c=UI_CONTROL_DESC_INIT;
    c.kind=UI_CONTROL_NUMBER; c.label=label; c.help_rml=help;
    c.binding=UI_BINDING_CONFIG_VAR; c.config_var=h;
    c.min=mn; c.max=mx; c.step=step; c.suffix=suffix;
    svc_ui->pane_add_control(mod_ctx,pane,&c,nullptr);
}

void reset_layout(ModContext*, void*) {
    if (switch_controller_selected_now()) {
        svc_config->set_int(mod_ctx,g_swWorldR1X,80);
        svc_config->set_int(mod_ctx,g_swWorldR1Y,180);
        svc_config->set_int(mod_ctx,g_swWorldR1Scale,100);
        svc_config->set_int(mod_ctx,g_swWorldAnalogScale,100);
        svc_config->set_int(mod_ctx,g_swWorldDpadScale,100);
        svc_config->set_int(mod_ctx,g_swWorldArrowScale,100);
        svc_config->set_int(mod_ctx,g_swWorldPortalTextScale,75);
        svc_config->set_int(mod_ctx,g_swWorldMoveTextScale,75);
        svc_config->set_int(mod_ctx,g_swWorldReturnTextScale,75);
        svc_config->set_int(mod_ctx,g_swWorldArrowX,-225);
        svc_config->set_int(mod_ctx,g_swWorldArrowY,-230);
        svc_config->set_int(mod_ctx,g_swWorldAnalogX,180);
        svc_config->set_int(mod_ctx,g_swWorldAnalogY,680);
        svc_config->set_int(mod_ctx,g_swWorldDpadX,260);
        svc_config->set_int(mod_ctx,g_swWorldDpadY,220);
        svc_config->set_int(mod_ctx,g_swWorldPortalTextX,-130);
        svc_config->set_int(mod_ctx,g_swWorldPortalTextY,0);
        svc_config->set_int(mod_ctx,g_swWorldMoveTextX,270);
        svc_config->set_int(mod_ctx,g_swWorldMoveTextY,20);
        svc_config->set_int(mod_ctx,g_swWorldReturnTextX,0);
        svc_config->set_int(mod_ctx,g_swWorldReturnTextY,20);
        svc_config->set_int(mod_ctx,g_swTriX,962);
        svc_config->set_int(mod_ctx,g_swTriY,387);
        svc_config->set_int(mod_ctx,g_swTriScale,90);
        svc_config->set_int(mod_ctx,g_swSquareX,1218);
        svc_config->set_int(mod_ctx,g_swSquareY,119);
        svc_config->set_int(mod_ctx,g_swSquareScale,90);
        svc_config->set_int(mod_ctx,g_swCircleX,1245);
        svc_config->set_int(mod_ctx,g_swCircleY,665);
        svc_config->set_int(mod_ctx,g_swCircleScale,90);
        svc_config->set_int(mod_ctx,g_swCrossX,1432);
        svc_config->set_int(mod_ctx,g_swCrossY,343);
        svc_config->set_int(mod_ctx,g_swCrossScale,90);
        svc_config->set_int(mod_ctx,g_swFishingCheckX,190);
        svc_config->set_int(mod_ctx,g_swFishingCheckY,110);
        svc_config->set_int(mod_ctx,g_swFishingCheckScale,65);
        svc_config->set_int(mod_ctx,g_swR1X,1620);
        svc_config->set_int(mod_ctx,g_swR1Y,-90);
        svc_config->set_int(mod_ctx,g_swR1Scale,90);
        svc_config->set_int(mod_ctx,g_swGuideX,870);
        svc_config->set_int(mod_ctx,g_swGuideY,30);
        svc_config->set_int(mod_ctx,g_swGuideScale,100);
        svc_config->set_int(mod_ctx,g_swDpadX,0);
        svc_config->set_int(mod_ctx,g_swDpadY,0);
        svc_config->set_int(mod_ctx,g_swDpadScale,100);
        svc_config->set_int(mod_ctx,g_swItemTextX,0);
        svc_config->set_int(mod_ctx,g_swItemTextY,0);
        svc_config->set_int(mod_ctx,g_swItemTextScale,100);
        svc_config->set_int(mod_ctx,g_swMapTextX,0);
        svc_config->set_int(mod_ctx,g_swMapTextY,0);
        svc_config->set_int(mod_ctx,g_swMapTextScale,100);
        svc_config->set_int(mod_ctx,g_swItemsAnchorX,0);
        svc_config->set_int(mod_ctx,g_swItemsAnchorY,0);
        svc_config->set_int(mod_ctx,g_swItemSquareX,-430);
        svc_config->set_int(mod_ctx,g_swItemSquareY,-640);
        svc_config->set_int(mod_ctx,g_swItemSquareScale,50);
        svc_config->set_int(mod_ctx,g_swItemTriangleX,220);
        svc_config->set_int(mod_ctx,g_swItemTriangleY,230);
        svc_config->set_int(mod_ctx,g_swItemTriangleScale,50);
        svc_config->set_int(mod_ctx,g_swItemCircleX,0);
        svc_config->set_int(mod_ctx,g_swItemCircleY,0);
        svc_config->set_int(mod_ctx,g_swItemCircleScale,100);
        svc_config->set_int(mod_ctx,g_swItemR1X,0);
        svc_config->set_int(mod_ctx,g_swItemR1Y,0);
        svc_config->set_int(mod_ctx,g_swItemR1Scale,100);
        svc_config->set_int(mod_ctx,g_swSwordX,40);
        svc_config->set_int(mod_ctx,g_swSwordY,-13);
        svc_config->set_int(mod_ctx,g_swSwordScale,50);
        svc_config->set_int(mod_ctx,g_swMidnaX,80);
        svc_config->set_int(mod_ctx,g_swMidnaY,-90);
        svc_config->set_int(mod_ctx,g_swMidnaScale,65);
        svc_config->set_int(mod_ctx,g_swHowlActionX,710);
        svc_config->set_int(mod_ctx,g_swHowlActionY,580);
        svc_config->set_int(mod_ctx,g_swHowlActionScale,65);
        svc_config->set_int(mod_ctx,g_swShopActionX,700);
        svc_config->set_int(mod_ctx,g_swShopActionY,570);
        svc_config->set_int(mod_ctx,g_swShopActionScale,65);
        svc_config->set_int(mod_ctx,g_swHowlBackX,640);
        svc_config->set_int(mod_ctx,g_swHowlBackY,550);
        svc_config->set_int(mod_ctx,g_swHowlBackScale,65);
        svc_config->set_int(mod_ctx,g_swShopBackX,640);
        svc_config->set_int(mod_ctx,g_swShopBackY,550);
        svc_config->set_int(mod_ctx,g_swShopBackScale,65);
        svc_config->set_int(mod_ctx,g_swActionTextX,520);
        svc_config->set_int(mod_ctx,g_swActionTextY,-35);
        svc_config->set_int(mod_ctx,g_swActionTextScale,55);
        svc_config->set_int(mod_ctx,g_swDialogActionTextX,200);
        svc_config->set_int(mod_ctx,g_swDialogActionTextY,480);
        svc_config->set_int(mod_ctx,g_swWhistleActionX,710);
        svc_config->set_int(mod_ctx,g_swWhistleActionY,585);
        svc_config->set_int(mod_ctx,g_swWhistleActionScale,55);
        svc_config->set_int(mod_ctx,g_swWhistleBackX,640);
        svc_config->set_int(mod_ctx,g_swWhistleBackY,555);
        svc_config->set_int(mod_ctx,g_swWhistleBackScale,55);
        svc_config->set_int(mod_ctx,g_swBackTextX,550);
        svc_config->set_int(mod_ctx,g_swBackTextY,-90);
        svc_config->set_int(mod_ctx,g_swBackTextScale,55);
        svc_config->set_int(mod_ctx,g_swWolfSenseX,-670);
        svc_config->set_int(mod_ctx,g_swWolfSenseY,-760);
        svc_config->set_int(mod_ctx,g_swWolfSenseScale,55);
        svc_config->set_int(mod_ctx,g_swWolfDigX,490);
        svc_config->set_int(mod_ctx,g_swWolfDigY,575);
        svc_config->set_int(mod_ctx,g_swWolfDigScale,55);
        svc_config->set_int(mod_ctx,g_swActionGlowX,-30);
        svc_config->set_int(mod_ctx,g_swActionGlowY,-25);
        svc_config->set_int(mod_ctx,g_swActionGlowScale,50);
        svc_config->set_int(mod_ctx,g_swBackGlowX,10);
        svc_config->set_int(mod_ctx,g_swBackGlowY,30);
        svc_config->set_int(mod_ctx,g_swBackGlowScale,100);
        svc_config->set_int(mod_ctx,g_swWolfXGlowX,5);
        svc_config->set_int(mod_ctx,g_swWolfXGlowY,-55);
        svc_config->set_int(mod_ctx,g_swWolfXGlowScale,50);
        svc_config->set_int(mod_ctx,g_swWolfYGlowX,-70);
        svc_config->set_int(mod_ctx,g_swWolfYGlowY,20);
        svc_config->set_int(mod_ctx,g_swWolfYGlowScale,50);
        svc_config->set_int(mod_ctx,g_swFileCrossX,15);
        svc_config->set_int(mod_ctx,g_swFileCrossY,0);
        svc_config->set_int(mod_ctx,g_swFileCircleX,10);
        svc_config->set_int(mod_ctx,g_swFileCircleY,30);
        svc_config->set_int(mod_ctx,g_swSaveCrossX,15);
        svc_config->set_int(mod_ctx,g_swSaveCrossY,0);
        svc_config->set_int(mod_ctx,g_swSaveCircleX,20);
        svc_config->set_int(mod_ctx,g_swSaveCircleY,25);
        svc_config->set_int(mod_ctx,g_swMenuCrossX,-122);
        svc_config->set_int(mod_ctx,g_swMenuCrossY,30);
        svc_config->set_int(mod_ctx,g_swMenuCrossScale,70);
        svc_config->set_int(mod_ctx,g_swMenuCircleX,3);
        svc_config->set_int(mod_ctx,g_swMenuCircleY,-10);
        svc_config->set_int(mod_ctx,g_swMenuCircleScale,100);
        svc_config->set_int(mod_ctx,g_swMenuConfirmTextX,-40);
        svc_config->set_int(mod_ctx,g_swMenuConfirmTextY,60);
        svc_config->set_int(mod_ctx,g_swMenuConfirmTextScale,60);
        svc_config->set_int(mod_ctx,g_swMenuBackTextX,60);
        svc_config->set_int(mod_ctx,g_swMenuBackTextY,10);
        svc_config->set_int(mod_ctx,g_swMenuBackTextScale,60);
        svc_config->set_int(mod_ctx,g_swMenuOrnamentX,-290);
        svc_config->set_int(mod_ctx,g_swMenuOrnamentY,50);
        svc_config->set_int(mod_ctx,g_swMenuOrnamentScale,75);
        svc_config->set_int(mod_ctx,g_swMapCrossX,-115);
        svc_config->set_int(mod_ctx,g_swMapCrossY,8);
        svc_config->set_int(mod_ctx,g_swMapCrossScale,75);
        svc_config->set_int(mod_ctx,g_swMapCircleX,15);
        svc_config->set_int(mod_ctx,g_swMapCircleY,-13);
        svc_config->set_int(mod_ctx,g_swMapCircleScale,90);
        svc_config->set_int(mod_ctx,g_swMapConfirmTextX,-20);
        svc_config->set_int(mod_ctx,g_swMapConfirmTextY,20);
        svc_config->set_int(mod_ctx,g_swMapConfirmTextScale,75);
        svc_config->set_int(mod_ctx,g_swMapBackTextX,100);
        svc_config->set_int(mod_ctx,g_swMapBackTextY,0);
        svc_config->set_int(mod_ctx,g_swMapBackTextScale,75);
        svc_config->set_int(mod_ctx,g_swMapOrnamentX,-300);
        svc_config->set_int(mod_ctx,g_swMapOrnamentY,0);
        svc_config->set_int(mod_ctx,g_swMapOrnamentScale,75);
        svc_config->set_int(mod_ctx,g_swHudOrnamentX,20);
        svc_config->set_int(mod_ctx,g_swHudOrnamentY,20);
        svc_config->set_int(mod_ctx,g_swHudOrnamentScale,100);
        svc_config->set_int(mod_ctx,g_swWheelSquareX,-170);
        svc_config->set_int(mod_ctx,g_swWheelSquareY,-10);
        svc_config->set_int(mod_ctx,g_swWheelSquareScale,87);
        svc_config->set_int(mod_ctx,g_swWheelTriangleX,-100);
        svc_config->set_int(mod_ctx,g_swWheelTriangleY,-40);
        svc_config->set_int(mod_ctx,g_swWheelTriangleScale,90);
        svc_config->set_int(mod_ctx,g_swWheelSelectAnalogX,50);
        svc_config->set_int(mod_ctx,g_swWheelSelectAnalogY,0);
        svc_config->set_int(mod_ctx,g_swWheelSelectAnalogScale,90);
        svc_config->set_int(mod_ctx,g_swWheelDirectAnalogX,100);
        svc_config->set_int(mod_ctx,g_swWheelDirectAnalogY,0);
        svc_config->set_int(mod_ctx,g_swWheelDirectAnalogScale,90);
        svc_config->set_int(mod_ctx,g_swWheelL2X,250);
        svc_config->set_int(mod_ctx,g_swWheelL2Y,-10);
        svc_config->set_int(mod_ctx,g_swWheelL2Scale,90);
        svc_config->set_int(mod_ctx,g_swWheelR2X,-130);
        svc_config->set_int(mod_ctx,g_swWheelR2Y,0);
        svc_config->set_int(mod_ctx,g_swWheelR2Scale,100);
        svc_config->set_int(mod_ctx,g_swDungeonMapCrossX,-120);
        svc_config->set_int(mod_ctx,g_swDungeonMapCrossY,30);
        svc_config->set_int(mod_ctx,g_swDungeonMapCrossScale,70);
        svc_config->set_int(mod_ctx,g_swDungeonMapCircleX,0);
        svc_config->set_int(mod_ctx,g_swDungeonMapCircleY,-10);
        svc_config->set_int(mod_ctx,g_swDungeonMapCircleScale,100);
        svc_config->set_int(mod_ctx,g_swDungeonMapConfirmTextX,-50);
        svc_config->set_int(mod_ctx,g_swDungeonMapConfirmTextY,60);
        svc_config->set_int(mod_ctx,g_swDungeonMapConfirmTextScale,75);
        svc_config->set_int(mod_ctx,g_swDungeonMapBackTextX,70);
        svc_config->set_int(mod_ctx,g_swDungeonMapBackTextY,10);
        svc_config->set_int(mod_ctx,g_swDungeonMapBackTextScale,75);
        svc_config->set_bool(mod_ctx,g_swDpadShadowsEnabled,true);
        svc_config->set_bool(mod_ctx,g_swWorldArrows,true);
        svc_config->set_bool(mod_ctx,g_swDpadArrowsEnabled,true);
        svc_config->set_bool(mod_ctx,g_swDpadMapAnimation,true);
        svc_config->set_bool(mod_ctx,g_swActionGlowEnabled,true);
        svc_config->set_bool(mod_ctx,g_swBackGlowEnabled,true);
        svc_config->set_bool(mod_ctx,g_swGlowPreview,false);
        svc_config->set_bool(mod_ctx,g_swWolfXGlowEnabled,true);
        svc_config->set_bool(mod_ctx,g_swWolfYGlowEnabled,true);
        svc_config->set_bool(mod_ctx,g_swWolfGlowPreview,false);
        svc_config->set_bool(mod_ctx,g_swBackButtonAnim,false);
        svc_config->set_bool(mod_ctx,g_swBackTextAnim,false);
        svc_config->set_bool(mod_ctx,g_swMenuPromptOrnament,true);
        svc_config->set_bool(mod_ctx,g_swMapOrnamentEnabled,true);
        svc_config->set_bool(mod_ctx,g_swHudOrnamentEnabled,true);
        svc_config->set_bool(mod_ctx,g_swItemSquareFlipH,false);
        svc_config->set_bool(mod_ctx,g_swItemSquareFlipV,false);
        svc_config->set_bool(mod_ctx,g_swItemTriangleFlipH,false);
        svc_config->set_bool(mod_ctx,g_swItemTriangleFlipV,false);
        // Keep Restore Default UI complete: every registered Switch ConfigVar
        // must be reset so a DEV recalibration starts from one deterministic state.
        svc_config->set_bool(mod_ctx,g_swSwordFlipH,false);
        svc_config->set_bool(mod_ctx,g_swSwordFlipV,false);
        return;
    }
    // Matches the registered defaults captured from the accepted user layout.
    if (g_buttonLayoutPreset != 0) svc_config->set_int(mod_ctx,g_buttonLayoutPreset,0);
    svc_config->set_int(mod_ctx,g_worldR1X,80);
    svc_config->set_int(mod_ctx,g_worldR1Y,180);
    svc_config->set_int(mod_ctx,g_worldR1Scale,100);
    svc_config->set_int(mod_ctx,g_worldAnalogScale,100);
    svc_config->set_int(mod_ctx,g_worldDpadScale,100);
    svc_config->set_int(mod_ctx,g_worldArrowScale,100);
    svc_config->set_int(mod_ctx,g_WorldPortalTextScale,75);
    svc_config->set_int(mod_ctx,g_WorldMoveTextScale,75);
    svc_config->set_int(mod_ctx,g_WorldReturnTextScale,75);
    svc_config->set_int(mod_ctx,g_worldArrowX,-225);
    svc_config->set_int(mod_ctx,g_worldArrowY,-230);
    svc_config->set_int(mod_ctx,g_worldAnalogX,180);
    svc_config->set_int(mod_ctx,g_worldAnalogY,680);
    svc_config->set_int(mod_ctx,g_worldDpadX,260);
    svc_config->set_int(mod_ctx,g_worldDpadY,220);
    svc_config->set_int(mod_ctx,g_WorldPortalTextX,-130);
    svc_config->set_int(mod_ctx,g_WorldPortalTextY,0);
    svc_config->set_int(mod_ctx,g_WorldMoveTextX,270);
    svc_config->set_int(mod_ctx,g_WorldMoveTextY,20);
    svc_config->set_int(mod_ctx,g_WorldReturnTextX,0);
    svc_config->set_int(mod_ctx,g_WorldReturnTextY,20);
    svc_config->set_int(mod_ctx,g_triX,962);
    svc_config->set_int(mod_ctx,g_triY,387);
    svc_config->set_int(mod_ctx,g_triScale,90);
    svc_config->set_int(mod_ctx,g_squareX,1218);
    svc_config->set_int(mod_ctx,g_squareY,119);
    svc_config->set_int(mod_ctx,g_squareScale,90);
    svc_config->set_int(mod_ctx,g_circleX,1505);
    svc_config->set_int(mod_ctx,g_circleY,414);
    svc_config->set_int(mod_ctx,g_circleScale,90);
    svc_config->set_int(mod_ctx,g_crossX,1174);
    svc_config->set_int(mod_ctx,g_crossY,597);
    svc_config->set_int(mod_ctx,g_crossScale,90);
    svc_config->set_int(mod_ctx,g_fishingCheckX,190);
    svc_config->set_int(mod_ctx,g_fishingCheckY,110);
    svc_config->set_int(mod_ctx,g_fishingCheckScale,65);
    svc_config->set_int(mod_ctx,g_r1X,1620);
    svc_config->set_int(mod_ctx,g_r1Y,-90);
    svc_config->set_int(mod_ctx,g_r1Scale,90);
    svc_config->set_int(mod_ctx,g_guideX,870);
    svc_config->set_int(mod_ctx,g_guideY,30);
    svc_config->set_int(mod_ctx,g_guideScale,100);
    svc_config->set_int(mod_ctx,g_dpadX,0);
    svc_config->set_int(mod_ctx,g_dpadY,0);
    svc_config->set_int(mod_ctx,g_dpadScale,100);
    svc_config->set_int(mod_ctx,g_itemTextX,0);
    svc_config->set_int(mod_ctx,g_itemTextY,0);
    svc_config->set_int(mod_ctx,g_itemTextScale,100);
    svc_config->set_int(mod_ctx,g_mapTextX,0);
    svc_config->set_int(mod_ctx,g_mapTextY,0);
    svc_config->set_int(mod_ctx,g_mapTextScale,100);
    svc_config->set_int(mod_ctx,g_itemsAnchorX,0);
    svc_config->set_int(mod_ctx,g_itemsAnchorY,0);
    svc_config->set_int(mod_ctx,g_itemSquareX,-430);
    svc_config->set_int(mod_ctx,g_itemSquareY,-640);
    svc_config->set_int(mod_ctx,g_itemSquareScale,50);
    svc_config->set_int(mod_ctx,g_itemTriangleX,220);
    svc_config->set_int(mod_ctx,g_itemTriangleY,230);
    svc_config->set_int(mod_ctx,g_itemTriangleScale,50);
    svc_config->set_int(mod_ctx,g_itemCircleX,0);
    svc_config->set_int(mod_ctx,g_itemCircleY,0);
    svc_config->set_int(mod_ctx,g_itemCircleScale,100);
    svc_config->set_int(mod_ctx,g_itemR1X,0);
    svc_config->set_int(mod_ctx,g_itemR1Y,0);
    svc_config->set_int(mod_ctx,g_itemR1Scale,100);
    svc_config->set_int(mod_ctx,g_swordX,83);
    svc_config->set_int(mod_ctx,g_swordY,-52);
    svc_config->set_int(mod_ctx,g_swordScale,50);
    svc_config->set_int(mod_ctx,g_midnaX,80);
    svc_config->set_int(mod_ctx,g_midnaY,-90);
    svc_config->set_int(mod_ctx,g_midnaScale,65);
    svc_config->set_int(mod_ctx,g_howlActionX,440);
    svc_config->set_int(mod_ctx,g_howlActionY,830);
    svc_config->set_int(mod_ctx,g_howlActionScale,65);
    svc_config->set_int(mod_ctx,g_shopActionX,450);
    svc_config->set_int(mod_ctx,g_shopActionY,830);
    svc_config->set_int(mod_ctx,g_shopActionScale,65);
    svc_config->set_int(mod_ctx,g_howlBackX,910);
    svc_config->set_int(mod_ctx,g_howlBackY,300);
    svc_config->set_int(mod_ctx,g_howlBackScale,65);
    svc_config->set_int(mod_ctx,g_shopBackX,900);
    svc_config->set_int(mod_ctx,g_shopBackY,300);
    svc_config->set_int(mod_ctx,g_shopBackScale,65);
    svc_config->set_int(mod_ctx,g_actionTextX,250);
    svc_config->set_int(mod_ctx,g_actionTextY,220);
    svc_config->set_int(mod_ctx,g_actionTextScale,55);
    svc_config->set_int(mod_ctx,g_dialogActionTextX,200);
    svc_config->set_int(mod_ctx,g_dialogActionTextY,480);
    svc_config->set_int(mod_ctx,g_whistleActionX,460); svc_config->set_int(mod_ctx,g_whistleActionY,840); svc_config->set_int(mod_ctx,g_whistleActionScale,55);
    svc_config->set_int(mod_ctx,g_whistleBackX,910); svc_config->set_int(mod_ctx,g_whistleBackY,310); svc_config->set_int(mod_ctx,g_whistleBackScale,55);
    svc_config->set_int(mod_ctx,g_backTextX,820);
    svc_config->set_int(mod_ctx,g_backTextY,-350);
    svc_config->set_int(mod_ctx,g_backTextScale,55);
    svc_config->set_int(mod_ctx,g_wolfSenseX,-670);
    svc_config->set_int(mod_ctx,g_wolfSenseY,-760);
    svc_config->set_int(mod_ctx,g_wolfSenseScale,55);
    svc_config->set_int(mod_ctx,g_wolfDigX,490);
    svc_config->set_int(mod_ctx,g_wolfDigY,575);
    svc_config->set_int(mod_ctx,g_wolfDigScale,55);
    svc_config->set_int(mod_ctx,g_actionGlowX,-30);
    svc_config->set_int(mod_ctx,g_actionGlowY,-25);
    svc_config->set_int(mod_ctx,g_actionGlowScale,50);
    svc_config->set_int(mod_ctx,g_backGlowX,10);
    svc_config->set_int(mod_ctx,g_backGlowY,30);
    svc_config->set_int(mod_ctx,g_backGlowScale,100);
    svc_config->set_int(mod_ctx,g_wolfXGlowX,5);
    svc_config->set_int(mod_ctx,g_wolfXGlowY,-55);
    svc_config->set_int(mod_ctx,g_wolfXGlowScale,50);
    svc_config->set_int(mod_ctx,g_wolfYGlowX,-70);
    svc_config->set_int(mod_ctx,g_wolfYGlowY,20);
    svc_config->set_int(mod_ctx,g_wolfYGlowScale,50);
    svc_config->set_int(mod_ctx,g_fileCrossX,15);
    svc_config->set_int(mod_ctx,g_fileCrossY,0);
    svc_config->set_int(mod_ctx,g_fileCircleX,10);
    svc_config->set_int(mod_ctx,g_fileCircleY,30);
    svc_config->set_int(mod_ctx,g_saveCrossX,15);
    svc_config->set_int(mod_ctx,g_saveCrossY,0);
    svc_config->set_int(mod_ctx,g_saveCircleX,20);
    svc_config->set_int(mod_ctx,g_saveCircleY,25);
    svc_config->set_int(mod_ctx,g_menuCrossX,-228);
    svc_config->set_int(mod_ctx,g_menuCrossY,260);
    svc_config->set_int(mod_ctx,g_menuCrossScale,70);
    svc_config->set_int(mod_ctx,g_menuCircleX,110);
    svc_config->set_int(mod_ctx,g_menuCircleY,-240);
    svc_config->set_int(mod_ctx,g_menuCircleScale,100);
    svc_config->set_int(mod_ctx,g_menuConfirmTextX,-140);
    svc_config->set_int(mod_ctx,g_menuConfirmTextY,280);
    svc_config->set_int(mod_ctx,g_menuConfirmTextScale,60);
    svc_config->set_int(mod_ctx,g_menuBackTextX,170);
    svc_config->set_int(mod_ctx,g_menuBackTextY,-220);
    svc_config->set_int(mod_ctx,g_menuBackTextScale,60);
    svc_config->set_int(mod_ctx,g_menuOrnamentX,-290);
    svc_config->set_int(mod_ctx,g_menuOrnamentY,50);
    svc_config->set_int(mod_ctx,g_menuOrnamentScale,75);
    svc_config->set_int(mod_ctx,g_mapCrossX,-220);
    svc_config->set_int(mod_ctx,g_mapCrossY,238);
    svc_config->set_int(mod_ctx,g_mapCrossScale,75);
    svc_config->set_int(mod_ctx,g_mapCircleX,119);
    svc_config->set_int(mod_ctx,g_mapCircleY,-243);
    svc_config->set_int(mod_ctx,g_mapCircleScale,90);
    svc_config->set_int(mod_ctx,g_mapConfirmTextX,-120);
    svc_config->set_int(mod_ctx,g_mapConfirmTextY,270);
    svc_config->set_int(mod_ctx,g_mapConfirmTextScale,75);
    svc_config->set_int(mod_ctx,g_mapBackTextX,230);
    svc_config->set_int(mod_ctx,g_mapBackTextY,-240);
    svc_config->set_int(mod_ctx,g_mapBackTextScale,75);
    svc_config->set_int(mod_ctx,g_mapOrnamentX,-300);
    svc_config->set_int(mod_ctx,g_mapOrnamentY,0);
    svc_config->set_int(mod_ctx,g_mapOrnamentScale,75);
    svc_config->set_int(mod_ctx,g_hudOrnamentX,20);
    svc_config->set_int(mod_ctx,g_hudOrnamentY,20);
    svc_config->set_int(mod_ctx,g_hudOrnamentScale,100);
    svc_config->set_int(mod_ctx,g_wheelSquareX,-170);
    svc_config->set_int(mod_ctx,g_wheelSquareY,-10);
    svc_config->set_int(mod_ctx,g_wheelSquareScale,87);
    svc_config->set_int(mod_ctx,g_wheelTriangleX,-100);
    svc_config->set_int(mod_ctx,g_wheelTriangleY,-40);
    svc_config->set_int(mod_ctx,g_wheelTriangleScale,90);
    svc_config->set_int(mod_ctx,g_wheelSelectAnalogX,50);
    svc_config->set_int(mod_ctx,g_wheelSelectAnalogY,0);
    svc_config->set_int(mod_ctx,g_wheelSelectAnalogScale,90);
    svc_config->set_int(mod_ctx,g_wheelDirectAnalogX,100);
    svc_config->set_int(mod_ctx,g_wheelDirectAnalogY,0);
    svc_config->set_int(mod_ctx,g_wheelDirectAnalogScale,90);
    svc_config->set_int(mod_ctx,g_wheelL2X,250);
    svc_config->set_int(mod_ctx,g_wheelL2Y,-10);
    svc_config->set_int(mod_ctx,g_wheelL2Scale,90);
    svc_config->set_int(mod_ctx,g_wheelR2X,-130);
    svc_config->set_int(mod_ctx,g_wheelR2Y,0);
    svc_config->set_int(mod_ctx,g_wheelR2Scale,100);
    svc_config->set_int(mod_ctx,g_dungeonMapCrossX,-120);
    svc_config->set_int(mod_ctx,g_dungeonMapCrossY,30);
    svc_config->set_int(mod_ctx,g_dungeonMapCrossScale,70);
    svc_config->set_int(mod_ctx,g_dungeonMapCircleX,0);
    svc_config->set_int(mod_ctx,g_dungeonMapCircleY,-10);
    svc_config->set_int(mod_ctx,g_dungeonMapCircleScale,100);
    svc_config->set_int(mod_ctx,g_dungeonMapConfirmTextX,-50);
    svc_config->set_int(mod_ctx,g_dungeonMapConfirmTextY,60);
    svc_config->set_int(mod_ctx,g_dungeonMapConfirmTextScale,75);
    svc_config->set_int(mod_ctx,g_dungeonMapBackTextX,70);
    svc_config->set_int(mod_ctx,g_dungeonMapBackTextY,10);
    svc_config->set_int(mod_ctx,g_dungeonMapBackTextScale,75);
    svc_config->set_bool(mod_ctx,g_showGuide,false);
    svc_config->set_bool(mod_ctx,g_dpadShadowsEnabled,true);
    svc_config->set_bool(mod_ctx,g_worldArrows,true);
    svc_config->set_bool(mod_ctx,g_dpadArrowsEnabled,true);
    svc_config->set_bool(mod_ctx,g_dpadMapAnimation,true);
    svc_config->set_bool(mod_ctx,g_actionGlowEnabled,true);
    svc_config->set_bool(mod_ctx,g_backGlowEnabled,true);
    svc_config->set_bool(mod_ctx,g_glowPreview,false);
    svc_config->set_bool(mod_ctx,g_wolfXGlowEnabled,false);
    svc_config->set_bool(mod_ctx,g_wolfYGlowEnabled,false);
    svc_config->set_bool(mod_ctx,g_wolfGlowPreview,false);
    svc_config->set_bool(mod_ctx,g_backButtonAnim,false);
    svc_config->set_bool(mod_ctx,g_backTextAnim,false);
    svc_config->set_bool(mod_ctx,g_menuPromptOrnament,true);
    svc_config->set_bool(mod_ctx,g_mapOrnamentEnabled,true);
    svc_config->set_bool(mod_ctx,g_hudOrnamentEnabled,true);
    svc_config->set_bool(mod_ctx,g_itemSquareFlipH,false);
    svc_config->set_bool(mod_ctx,g_itemSquareFlipV,false);
    svc_config->set_bool(mod_ctx,g_itemTriangleFlipH,false);
    svc_config->set_bool(mod_ctx,g_itemTriangleFlipV,false);
    svc_config->set_bool(mod_ctx,g_swordFlipH,false);
    svc_config->set_bool(mod_ctx,g_swordFlipV,false);
    if (kDeveloperOptions) {
        svc_config->set_int(mod_ctx,g_swapTriX,1208); svc_config->set_int(mod_ctx,g_swapTriY,126); svc_config->set_int(mod_ctx,g_swapTriScale,90);
        svc_config->set_int(mod_ctx,g_swapSquareX,971); svc_config->set_int(mod_ctx,g_swapSquareY,380); svc_config->set_int(mod_ctx,g_swapSquareScale,90);
        svc_config->set_int(mod_ctx,g_swapItemSquareX,-690); svc_config->set_int(mod_ctx,g_swapItemSquareY,-380); svc_config->set_int(mod_ctx,g_swapItemSquareScale,50);
        svc_config->set_int(mod_ctx,g_swapItemTriangleX,480); svc_config->set_int(mod_ctx,g_swapItemTriangleY,-30); svc_config->set_int(mod_ctx,g_swapItemTriangleScale,50);
        svc_config->set_int(mod_ctx,g_swapDialogActionTextX,200); svc_config->set_int(mod_ctx,g_swapDialogActionTextY,480);
        svc_config->set_bool(mod_ctx,g_swapBackButtonAnim,false); svc_config->set_bool(mod_ctx,g_swapBackTextAnim,false);
        svc_config->set_int(mod_ctx,g_swapWhistleActionX,460); svc_config->set_int(mod_ctx,g_swapWhistleActionY,840); svc_config->set_int(mod_ctx,g_swapWhistleActionScale,55);
        svc_config->set_int(mod_ctx,g_swapWhistleBackX,910); svc_config->set_int(mod_ctx,g_swapWhistleBackY,310); svc_config->set_int(mod_ctx,g_swapWhistleBackScale,55);
        svc_config->set_int(mod_ctx,g_swapHowlActionX,440); svc_config->set_int(mod_ctx,g_swapHowlActionY,830); svc_config->set_int(mod_ctx,g_swapHowlActionScale,65);
        svc_config->set_int(mod_ctx,g_swapHowlBackX,910); svc_config->set_int(mod_ctx,g_swapHowlBackY,300); svc_config->set_int(mod_ctx,g_swapHowlBackScale,65);
        svc_config->set_int(mod_ctx,g_swapShopActionX,450); svc_config->set_int(mod_ctx,g_swapShopActionY,830); svc_config->set_int(mod_ctx,g_swapShopActionScale,65);
        svc_config->set_int(mod_ctx,g_swapShopBackX,900); svc_config->set_int(mod_ctx,g_swapShopBackY,300); svc_config->set_int(mod_ctx,g_swapShopBackScale,65);
        svc_config->set_int(mod_ctx,g_swapWolfSenseX,-920); svc_config->set_int(mod_ctx,g_swapWolfSenseY,-500); svc_config->set_int(mod_ctx,g_swapWolfSenseScale,55);
        svc_config->set_int(mod_ctx,g_swapWolfDigX,730); svc_config->set_int(mod_ctx,g_swapWolfDigY,310); svc_config->set_int(mod_ctx,g_swapWolfDigScale,55);
        svc_config->set_int(mod_ctx,g_swapWolfXGlowX,5); svc_config->set_int(mod_ctx,g_swapWolfXGlowY,-55); svc_config->set_int(mod_ctx,g_swapWolfXGlowScale,50);
        svc_config->set_int(mod_ctx,g_swapWolfYGlowX,-75); svc_config->set_int(mod_ctx,g_swapWolfYGlowY,20); svc_config->set_int(mod_ctx,g_swapWolfYGlowScale,50);
        svc_config->set_int(mod_ctx,g_swapWheelSquareX,-430); svc_config->set_int(mod_ctx,g_swapWheelSquareY,-10); svc_config->set_int(mod_ctx,g_swapWheelSquareScale,87);
        svc_config->set_int(mod_ctx,g_swapWheelTriangleX,160); svc_config->set_int(mod_ctx,g_swapWheelTriangleY,-40); svc_config->set_int(mod_ctx,g_swapWheelTriangleScale,90);
        svc_config->set_bool(mod_ctx,g_swapItemSquareFlipH,false); svc_config->set_bool(mod_ctx,g_swapItemSquareFlipV,false);
        svc_config->set_bool(mod_ctx,g_swapItemTriangleFlipH,false); svc_config->set_bool(mod_ctx,g_swapItemTriangleFlipV,false);
        svc_config->set_bool(mod_ctx,g_swapWolfXGlowEnabled,true); svc_config->set_bool(mod_ctx,g_swapWolfYGlowEnabled,true);
        svc_config->set_int(mod_ctx,g_ybDialogActionTextX,200); svc_config->set_int(mod_ctx,g_ybDialogActionTextY,480);
        svc_config->set_bool(mod_ctx,g_ybBackButtonAnim,false); svc_config->set_bool(mod_ctx,g_ybBackTextAnim,false);
        svc_config->set_int(mod_ctx,g_ybWhistleActionX,460); svc_config->set_int(mod_ctx,g_ybWhistleActionY,840); svc_config->set_int(mod_ctx,g_ybWhistleActionScale,55);
        svc_config->set_int(mod_ctx,g_ybWhistleBackX,410); svc_config->set_int(mod_ctx,g_ybWhistleBackY,300); svc_config->set_int(mod_ctx,g_ybWhistleBackScale,55);
        svc_config->set_int(mod_ctx,g_ybHowlActionX,440); svc_config->set_int(mod_ctx,g_ybHowlActionY,830); svc_config->set_int(mod_ctx,g_ybHowlActionScale,65);
        svc_config->set_int(mod_ctx,g_ybHowlBackX,400); svc_config->set_int(mod_ctx,g_ybHowlBackY,300); svc_config->set_int(mod_ctx,g_ybHowlBackScale,65);
        svc_config->set_int(mod_ctx,g_ybShopActionX,450); svc_config->set_int(mod_ctx,g_ybShopActionY,830); svc_config->set_int(mod_ctx,g_ybShopActionScale,65);
        svc_config->set_int(mod_ctx,g_ybShopBackX,400); svc_config->set_int(mod_ctx,g_ybShopBackY,300); svc_config->set_int(mod_ctx,g_ybShopBackScale,65);
        svc_config->set_int(mod_ctx,g_xyxbDialogActionTextX,200); svc_config->set_int(mod_ctx,g_xyxbDialogActionTextY,480);
        svc_config->set_bool(mod_ctx,g_xyxbBackButtonAnim,false); svc_config->set_bool(mod_ctx,g_xyxbBackTextAnim,false);
        svc_config->set_int(mod_ctx,g_xyxbWhistleActionX,460); svc_config->set_int(mod_ctx,g_xyxbWhistleActionY,840); svc_config->set_int(mod_ctx,g_xyxbWhistleActionScale,55);
        svc_config->set_int(mod_ctx,g_xyxbWhistleBackX,410); svc_config->set_int(mod_ctx,g_xyxbWhistleBackY,300); svc_config->set_int(mod_ctx,g_xyxbWhistleBackScale,55);
        svc_config->set_int(mod_ctx,g_xyxbHowlActionX,440); svc_config->set_int(mod_ctx,g_xyxbHowlActionY,830); svc_config->set_int(mod_ctx,g_xyxbHowlActionScale,65);
        svc_config->set_int(mod_ctx,g_xyxbHowlBackX,400); svc_config->set_int(mod_ctx,g_xyxbHowlBackY,300); svc_config->set_int(mod_ctx,g_xyxbHowlBackScale,65);
        svc_config->set_int(mod_ctx,g_xyxbShopActionX,450); svc_config->set_int(mod_ctx,g_xyxbShopActionY,830); svc_config->set_int(mod_ctx,g_xyxbShopActionScale,65);
        svc_config->set_int(mod_ctx,g_xyxbShopBackX,400); svc_config->set_int(mod_ctx,g_xyxbShopBackY,300); svc_config->set_int(mod_ctx,g_xyxbShopBackScale,65);
        svc_config->set_int(mod_ctx,g_ybBackTextX,310); svc_config->set_int(mod_ctx,g_ybBackTextY,-345); svc_config->set_int(mod_ctx,g_ybBackTextScale,55);
        svc_config->set_int(mod_ctx,g_ybBackGlowX,10); svc_config->set_int(mod_ctx,g_ybBackGlowY,30); svc_config->set_int(mod_ctx,g_ybBackGlowScale,100);
        svc_config->set_bool(mod_ctx,g_ybBackGlowEnabled,true);
        svc_config->set_int(mod_ctx,g_xyxbBackTextX,310); svc_config->set_int(mod_ctx,g_xyxbBackTextY,-345); svc_config->set_int(mod_ctx,g_xyxbBackTextScale,55);
        svc_config->set_int(mod_ctx,g_xyxbBackGlowX,10); svc_config->set_int(mod_ctx,g_xyxbBackGlowY,30); svc_config->set_int(mod_ctx,g_xyxbBackGlowScale,100);
        svc_config->set_bool(mod_ctx,g_xyxbBackGlowEnabled,true);
    }
}
void add_toggle(UiElementHandle pane,const char* label,ConfigVarHandle h,const char* help) {
    h=switch_variant(h);
    UiControlDesc c=UI_CONTROL_DESC_INIT;
    c.kind=UI_CONTROL_TOGGLE; c.label=label; c.help_rml=help;
    c.binding=UI_BINDING_CONFIG_VAR; c.config_var=h;
    svc_ui->pane_add_control(mod_ctx,pane,&c,nullptr);
}
void add_button(UiElementHandle pane,const char* label,UiPressedFn fn,const char* help) {
    UiControlDesc c=UI_CONTROL_DESC_INIT;
    c.kind=UI_CONTROL_BUTTON; c.label=label; c.help_rml=help; c.on_pressed=fn;
    svc_ui->pane_add_control(mod_ctx,pane,&c,nullptr);
}

void json_int(std::string& out,const char* key,ConfigVarHandle h,bool& first) {
    if (!first) out += ",\n";
    first=false;
    out += "      \"";
    out += key;
    out += "\": ";
    out += std::to_string(cfg_int(h,0));
}
void json_bool(std::string& out,const char* key,ConfigVarHandle h,bool& first) {
    if (!first) out += ",\n";
    first=false;
    out += "      \"";
    out += key;
    out += "\": ";
    out += cfg_bool(h,false) ? "true" : "false";
}

void export_midna_diagnostic(ModContext*,void*);

void export_calibration(ModContext*,void*) {
    if (!kDeveloperOptions || svc_host == nullptr || svc_log == nullptr) return;

    const char* dataDir=nullptr;
    if (svc_host->data_dir(mod_ctx,&dataDir) != MOD_OK || dataDir == nullptr || dataDir[0] == '\0') {
        svc_log->error(mod_ctx,"Calibration export failed: mod data directory is unavailable");
        return;
    }

    std::string json;
    json.reserve(32768);
    json += "{\n";
    json += "  \"format\": \"tp-classic-controller-ui-calibration\",\n";
    json += "  \"version\": 2,\n";
    json += "  \"activeControllerDesign\": \"";
    switch(raw_controller_style()) {
    case 1: json += "xbox"; break;
    case 2: json += "switch"; break;
    default: json += "playstation"; break;
    }
    json += "\",\n";
    json += "  \"profiles\": {\n";
    json += "    \"playstationXbox\": {\n";
    json += "      \"activePreset\": \"";
    switch(current_layout_preset()) {
    case ButtonLayoutPreset::SwapXY: json += "swapXY"; break;
    case ButtonLayoutPreset::SwapYB: json += "swapYB"; break;
    case ButtonLayoutPreset::SwapXYXB: json += "swapXYXB"; break;
    default: json += "base"; break;
    }
    json += "\",\n";

    json += "      \"base\": {\n";
    bool first=true;
    json_int(json,"hudY.x",g_triX,first); json_int(json,"hudY.y",g_triY,first); json_int(json,"hudY.scale",g_triScale,first);
    json_int(json,"hudX.x",g_squareX,first); json_int(json,"hudX.y",g_squareY,first); json_int(json,"hudX.scale",g_squareScale,first);
    json_int(json,"itemX.x",g_itemSquareX,first); json_int(json,"itemX.y",g_itemSquareY,first); json_int(json,"itemX.scale",g_itemSquareScale,first);
    json_bool(json,"itemX.flipH",g_itemSquareFlipH,first); json_bool(json,"itemX.flipV",g_itemSquareFlipV,first);
    json_int(json,"itemY.x",g_itemTriangleX,first); json_int(json,"itemY.y",g_itemTriangleY,first); json_int(json,"itemY.scale",g_itemTriangleScale,first);
    json_bool(json,"itemY.flipH",g_itemTriangleFlipH,first); json_bool(json,"itemY.flipV",g_itemTriangleFlipV,first);
    json_int(json,"sensesText.x",g_wolfSenseX,first); json_int(json,"sensesText.y",g_wolfSenseY,first); json_int(json,"sensesText.scale",g_wolfSenseScale,first);
    json_int(json,"dialogueAction.x",g_dialogActionTextX,first); json_int(json,"dialogueAction.y",g_dialogActionTextY,first);
    json_bool(json,"dialogueBack.buttonAnimation",g_backButtonAnim,first); json_bool(json,"dialogueBack.textAnimation",g_backTextAnim,first);
    json_int(json,"whistleAction.x",g_whistleActionX,first); json_int(json,"whistleAction.y",g_whistleActionY,first); json_int(json,"whistleAction.scale",g_whistleActionScale,first);
    json_int(json,"whistleBack.x",g_whistleBackX,first); json_int(json,"whistleBack.y",g_whistleBackY,first); json_int(json,"whistleBack.scale",g_whistleBackScale,first);
    json_int(json,"howl.x",g_howlActionX,first); json_int(json,"howl.y",g_howlActionY,first); json_int(json,"howl.scale",g_howlActionScale,first);
    json_int(json,"howlExit.x",g_howlBackX,first); json_int(json,"howlExit.y",g_howlBackY,first); json_int(json,"howlExit.scale",g_howlBackScale,first);
    json_int(json,"shopConfirm.x",g_shopActionX,first); json_int(json,"shopConfirm.y",g_shopActionY,first); json_int(json,"shopConfirm.scale",g_shopActionScale,first);
    json_int(json,"shopExit.x",g_shopBackX,first); json_int(json,"shopExit.y",g_shopBackY,first); json_int(json,"shopExit.scale",g_shopBackScale,first);
    json_int(json,"digText.x",g_wolfDigX,first); json_int(json,"digText.y",g_wolfDigY,first); json_int(json,"digText.scale",g_wolfDigScale,first);
    json_int(json,"sensesGlow.x",g_wolfXGlowX,first); json_int(json,"sensesGlow.y",g_wolfXGlowY,first); json_int(json,"sensesGlow.scale",g_wolfXGlowScale,first);
    json_bool(json,"sensesGlow.enabled",g_wolfXGlowEnabled,first);
    json_int(json,"digGlow.x",g_wolfYGlowX,first); json_int(json,"digGlow.y",g_wolfYGlowY,first); json_int(json,"digGlow.scale",g_wolfYGlowScale,first);
    json_bool(json,"digGlow.enabled",g_wolfYGlowEnabled,first);
    json_int(json,"wheelX.x",g_wheelSquareX,first); json_int(json,"wheelX.y",g_wheelSquareY,first); json_int(json,"wheelX.scale",g_wheelSquareScale,first);
    json_int(json,"wheelY.x",g_wheelTriangleX,first); json_int(json,"wheelY.y",g_wheelTriangleY,first); json_int(json,"wheelY.scale",g_wheelTriangleScale,first);
    json += "\n      },\n";

    json += "  \"swapXY\": {\n";
    first=true;
    json_int(json,"hudY.x",g_swapTriX,first); json_int(json,"hudY.y",g_swapTriY,first); json_int(json,"hudY.scale",g_swapTriScale,first);
    json_int(json,"hudX.x",g_swapSquareX,first); json_int(json,"hudX.y",g_swapSquareY,first); json_int(json,"hudX.scale",g_swapSquareScale,first);
    json_int(json,"itemX.x",g_swapItemSquareX,first); json_int(json,"itemX.y",g_swapItemSquareY,first); json_int(json,"itemX.scale",g_swapItemSquareScale,first);
    json_bool(json,"itemX.flipH",g_swapItemSquareFlipH,first); json_bool(json,"itemX.flipV",g_swapItemSquareFlipV,first);
    json_int(json,"itemY.x",g_swapItemTriangleX,first); json_int(json,"itemY.y",g_swapItemTriangleY,first); json_int(json,"itemY.scale",g_swapItemTriangleScale,first);
    json_bool(json,"itemY.flipH",g_swapItemTriangleFlipH,first); json_bool(json,"itemY.flipV",g_swapItemTriangleFlipV,first);
    json_int(json,"sensesText.x",g_swapWolfSenseX,first); json_int(json,"sensesText.y",g_swapWolfSenseY,first); json_int(json,"sensesText.scale",g_swapWolfSenseScale,first);
    json_int(json,"dialogueAction.x",g_swapDialogActionTextX,first); json_int(json,"dialogueAction.y",g_swapDialogActionTextY,first);
    json_bool(json,"dialogueBack.buttonAnimation",g_swapBackButtonAnim,first); json_bool(json,"dialogueBack.textAnimation",g_swapBackTextAnim,first);
    json_int(json,"whistleAction.x",g_swapWhistleActionX,first); json_int(json,"whistleAction.y",g_swapWhistleActionY,first); json_int(json,"whistleAction.scale",g_swapWhistleActionScale,first);
    json_int(json,"whistleBack.x",g_swapWhistleBackX,first); json_int(json,"whistleBack.y",g_swapWhistleBackY,first); json_int(json,"whistleBack.scale",g_swapWhistleBackScale,first);
    json_int(json,"howl.x",g_swapHowlActionX,first); json_int(json,"howl.y",g_swapHowlActionY,first); json_int(json,"howl.scale",g_swapHowlActionScale,first);
    json_int(json,"howlExit.x",g_swapHowlBackX,first); json_int(json,"howlExit.y",g_swapHowlBackY,first); json_int(json,"howlExit.scale",g_swapHowlBackScale,first);
    json_int(json,"shopConfirm.x",g_swapShopActionX,first); json_int(json,"shopConfirm.y",g_swapShopActionY,first); json_int(json,"shopConfirm.scale",g_swapShopActionScale,first);
    json_int(json,"shopExit.x",g_swapShopBackX,first); json_int(json,"shopExit.y",g_swapShopBackY,first); json_int(json,"shopExit.scale",g_swapShopBackScale,first);
    json_int(json,"digText.x",g_swapWolfDigX,first); json_int(json,"digText.y",g_swapWolfDigY,first); json_int(json,"digText.scale",g_swapWolfDigScale,first);
    json_int(json,"sensesGlow.x",g_swapWolfXGlowX,first); json_int(json,"sensesGlow.y",g_swapWolfXGlowY,first); json_int(json,"sensesGlow.scale",g_swapWolfXGlowScale,first);
    json_bool(json,"sensesGlow.enabled",g_swapWolfXGlowEnabled,first);
    json_int(json,"digGlow.x",g_swapWolfYGlowX,first); json_int(json,"digGlow.y",g_swapWolfYGlowY,first); json_int(json,"digGlow.scale",g_swapWolfYGlowScale,first);
    json_bool(json,"digGlow.enabled",g_swapWolfYGlowEnabled,first);
    json_int(json,"wheelX.x",g_swapWheelSquareX,first); json_int(json,"wheelX.y",g_swapWheelSquareY,first); json_int(json,"wheelX.scale",g_swapWheelSquareScale,first);
    json_int(json,"wheelY.x",g_swapWheelTriangleX,first); json_int(json,"wheelY.y",g_swapWheelTriangleY,first); json_int(json,"wheelY.scale",g_swapWheelTriangleScale,first);
    json += "\n      },\n";

    json += "  \"swapYB\": {\n";
    first=true;
    json_int(json,"hudB.x",g_ybCircleX,first); json_int(json,"hudB.y",g_ybCircleY,first); json_int(json,"hudB.scale",g_ybCircleScale,first);
    json_int(json,"hudY.x",g_ybTriX,first); json_int(json,"hudY.y",g_ybTriY,first); json_int(json,"hudY.scale",g_ybTriScale,first);
    json_int(json,"hudX.x",g_ybSquareX,first); json_int(json,"hudX.y",g_ybSquareY,first); json_int(json,"hudX.scale",g_ybSquareScale,first);
    json_int(json,"itemX.x",g_ybItemSquareX,first); json_int(json,"itemX.y",g_ybItemSquareY,first); json_int(json,"itemX.scale",g_ybItemSquareScale,first);
    json_bool(json,"itemX.flipH",g_ybItemSquareFlipH,first); json_bool(json,"itemX.flipV",g_ybItemSquareFlipV,first);
    json_int(json,"itemY.x",g_ybItemTriangleX,first); json_int(json,"itemY.y",g_ybItemTriangleY,first); json_int(json,"itemY.scale",g_ybItemTriangleScale,first);
    json_bool(json,"itemY.flipH",g_ybItemTriangleFlipH,first); json_bool(json,"itemY.flipV",g_ybItemTriangleFlipV,first);
    json_int(json,"sword.x",g_ybSwordX,first); json_int(json,"sword.y",g_ybSwordY,first); json_int(json,"sword.scale",g_ybSwordScale,first);
    json_int(json,"backText.x",g_ybBackTextX,first); json_int(json,"backText.y",g_ybBackTextY,first); json_int(json,"backText.scale",g_ybBackTextScale,first);
    json_int(json,"backGlow.x",g_ybBackGlowX,first); json_int(json,"backGlow.y",g_ybBackGlowY,first); json_int(json,"backGlow.scale",g_ybBackGlowScale,first);
    json_bool(json,"backGlow.enabled",g_ybBackGlowEnabled,first);
    json_int(json,"sensesText.x",g_ybWolfSenseX,first); json_int(json,"sensesText.y",g_ybWolfSenseY,first); json_int(json,"sensesText.scale",g_ybWolfSenseScale,first);
    json_int(json,"dialogueAction.x",g_ybDialogActionTextX,first); json_int(json,"dialogueAction.y",g_ybDialogActionTextY,first);
    json_bool(json,"dialogueBack.buttonAnimation",g_ybBackButtonAnim,first); json_bool(json,"dialogueBack.textAnimation",g_ybBackTextAnim,first);
    json_int(json,"whistleAction.x",g_ybWhistleActionX,first); json_int(json,"whistleAction.y",g_ybWhistleActionY,first); json_int(json,"whistleAction.scale",g_ybWhistleActionScale,first);
    json_int(json,"whistleBack.x",g_ybWhistleBackX,first); json_int(json,"whistleBack.y",g_ybWhistleBackY,first); json_int(json,"whistleBack.scale",g_ybWhistleBackScale,first);
    json_int(json,"howl.x",g_ybHowlActionX,first); json_int(json,"howl.y",g_ybHowlActionY,first); json_int(json,"howl.scale",g_ybHowlActionScale,first);
    json_int(json,"howlExit.x",g_ybHowlBackX,first); json_int(json,"howlExit.y",g_ybHowlBackY,first); json_int(json,"howlExit.scale",g_ybHowlBackScale,first);
    json_int(json,"shopConfirm.x",g_ybShopActionX,first); json_int(json,"shopConfirm.y",g_ybShopActionY,first); json_int(json,"shopConfirm.scale",g_ybShopActionScale,first);
    json_int(json,"shopExit.x",g_ybShopBackX,first); json_int(json,"shopExit.y",g_ybShopBackY,first); json_int(json,"shopExit.scale",g_ybShopBackScale,first);
    json_int(json,"digText.x",g_ybWolfDigX,first); json_int(json,"digText.y",g_ybWolfDigY,first); json_int(json,"digText.scale",g_ybWolfDigScale,first);
    json_int(json,"sensesGlow.x",g_ybWolfXGlowX,first); json_int(json,"sensesGlow.y",g_ybWolfXGlowY,first); json_int(json,"sensesGlow.scale",g_ybWolfXGlowScale,first);
    json_int(json,"digGlow.x",g_ybWolfYGlowX,first); json_int(json,"digGlow.y",g_ybWolfYGlowY,first); json_int(json,"digGlow.scale",g_ybWolfYGlowScale,first);
    json_int(json,"wheelX.x",g_ybWheelSquareX,first); json_int(json,"wheelX.y",g_ybWheelSquareY,first); json_int(json,"wheelX.scale",g_ybWheelSquareScale,first);
    json_int(json,"wheelY.x",g_ybWheelTriangleX,first); json_int(json,"wheelY.y",g_ybWheelTriangleY,first); json_int(json,"wheelY.scale",g_ybWheelTriangleScale,first);
    json += "\n      },\n";

    json += "  \"swapXYXB\": {\n";
    first=true;
    json_int(json,"hudB.x",g_xyxbCircleX,first); json_int(json,"hudB.y",g_xyxbCircleY,first); json_int(json,"hudB.scale",g_xyxbCircleScale,first);
    json_int(json,"hudY.x",g_xyxbTriX,first); json_int(json,"hudY.y",g_xyxbTriY,first); json_int(json,"hudY.scale",g_xyxbTriScale,first);
    json_int(json,"hudX.x",g_xyxbSquareX,first); json_int(json,"hudX.y",g_xyxbSquareY,first); json_int(json,"hudX.scale",g_xyxbSquareScale,first);
    json_int(json,"itemX.x",g_xyxbItemSquareX,first); json_int(json,"itemX.y",g_xyxbItemSquareY,first); json_int(json,"itemX.scale",g_xyxbItemSquareScale,first);
    json_bool(json,"itemX.flipH",g_xyxbItemSquareFlipH,first); json_bool(json,"itemX.flipV",g_xyxbItemSquareFlipV,first);
    json_int(json,"itemY.x",g_xyxbItemTriangleX,first); json_int(json,"itemY.y",g_xyxbItemTriangleY,first); json_int(json,"itemY.scale",g_xyxbItemTriangleScale,first);
    json_bool(json,"itemY.flipH",g_xyxbItemTriangleFlipH,first); json_bool(json,"itemY.flipV",g_xyxbItemTriangleFlipV,first);
    json_int(json,"sword.x",g_xyxbSwordX,first); json_int(json,"sword.y",g_xyxbSwordY,first); json_int(json,"sword.scale",g_xyxbSwordScale,first);
    json_int(json,"backText.x",g_xyxbBackTextX,first); json_int(json,"backText.y",g_xyxbBackTextY,first); json_int(json,"backText.scale",g_xyxbBackTextScale,first);
    json_int(json,"backGlow.x",g_xyxbBackGlowX,first); json_int(json,"backGlow.y",g_xyxbBackGlowY,first); json_int(json,"backGlow.scale",g_xyxbBackGlowScale,first);
    json_bool(json,"backGlow.enabled",g_xyxbBackGlowEnabled,first);
    json_int(json,"sensesText.x",g_xyxbWolfSenseX,first); json_int(json,"sensesText.y",g_xyxbWolfSenseY,first); json_int(json,"sensesText.scale",g_xyxbWolfSenseScale,first);
    json_int(json,"dialogueAction.x",g_xyxbDialogActionTextX,first); json_int(json,"dialogueAction.y",g_xyxbDialogActionTextY,first);
    json_bool(json,"dialogueBack.buttonAnimation",g_xyxbBackButtonAnim,first); json_bool(json,"dialogueBack.textAnimation",g_xyxbBackTextAnim,first);
    json_int(json,"whistleAction.x",g_xyxbWhistleActionX,first); json_int(json,"whistleAction.y",g_xyxbWhistleActionY,first); json_int(json,"whistleAction.scale",g_xyxbWhistleActionScale,first);
    json_int(json,"whistleBack.x",g_xyxbWhistleBackX,first); json_int(json,"whistleBack.y",g_xyxbWhistleBackY,first); json_int(json,"whistleBack.scale",g_xyxbWhistleBackScale,first);
    json_int(json,"howl.x",g_xyxbHowlActionX,first); json_int(json,"howl.y",g_xyxbHowlActionY,first); json_int(json,"howl.scale",g_xyxbHowlActionScale,first);
    json_int(json,"howlExit.x",g_xyxbHowlBackX,first); json_int(json,"howlExit.y",g_xyxbHowlBackY,first); json_int(json,"howlExit.scale",g_xyxbHowlBackScale,first);
    json_int(json,"shopConfirm.x",g_xyxbShopActionX,first); json_int(json,"shopConfirm.y",g_xyxbShopActionY,first); json_int(json,"shopConfirm.scale",g_xyxbShopActionScale,first);
    json_int(json,"shopExit.x",g_xyxbShopBackX,first); json_int(json,"shopExit.y",g_xyxbShopBackY,first); json_int(json,"shopExit.scale",g_xyxbShopBackScale,first);
    json_int(json,"digText.x",g_xyxbWolfDigX,first); json_int(json,"digText.y",g_xyxbWolfDigY,first); json_int(json,"digText.scale",g_xyxbWolfDigScale,first);
    json_int(json,"sensesGlow.x",g_xyxbWolfXGlowX,first); json_int(json,"sensesGlow.y",g_xyxbWolfXGlowY,first); json_int(json,"sensesGlow.scale",g_xyxbWolfXGlowScale,first);
    json_int(json,"digGlow.x",g_xyxbWolfYGlowX,first); json_int(json,"digGlow.y",g_xyxbWolfYGlowY,first); json_int(json,"digGlow.scale",g_xyxbWolfYGlowScale,first);
    json_int(json,"wheelX.x",g_xyxbWheelSquareX,first); json_int(json,"wheelX.y",g_xyxbWheelSquareY,first); json_int(json,"wheelX.scale",g_xyxbWheelSquareScale,first);
    json_int(json,"wheelY.x",g_xyxbWheelTriangleX,first); json_int(json,"wheelY.y",g_xyxbWheelTriangleY,first); json_int(json,"wheelY.scale",g_xyxbWheelTriangleScale,first);
    json += "\n      },\n";

    json += "  \"shared\": {\n";
    first=true;
    json_int(json,"hudA.x",g_crossX,first); json_int(json,"hudA.y",g_crossY,first); json_int(json,"hudA.scale",g_crossScale,first);
    json_int(json,"hudB.x",g_circleX,first); json_int(json,"hudB.y",g_circleY,first); json_int(json,"hudB.scale",g_circleScale,first);
    json_int(json,"hudR1.x",g_r1X,first); json_int(json,"hudR1.y",g_r1Y,first); json_int(json,"hudR1.scale",g_r1Scale,first);
    json_int(json,"dpad.x",g_dpadX,first); json_int(json,"dpad.y",g_dpadY,first); json_int(json,"dpad.scale",g_dpadScale,first);
    json_int(json,"itemsAnchor.x",g_itemsAnchorX,first); json_int(json,"itemsAnchor.y",g_itemsAnchorY,first);
    json_int(json,"wheelL2.x",g_wheelL2X,first); json_int(json,"wheelL2.y",g_wheelL2Y,first); json_int(json,"wheelL2.scale",g_wheelL2Scale,first);
    json_int(json,"wheelR2.x",g_wheelR2X,first); json_int(json,"wheelR2.y",g_wheelR2Y,first); json_int(json,"wheelR2.scale",g_wheelR2Scale,first);
    json += "\n      }\n";
    json += "    },\n";
    json += "    \"switch\": {\n";
    json += "      \"activePreset\": \"base\",\n";
    json += "      \"base\": {\n";
    first=true;
    json_int(json,"worldR1X",g_swWorldR1X,first);
    json_int(json,"worldR1Y",g_swWorldR1Y,first);
    json_int(json,"worldR1Scale",g_swWorldR1Scale,first);
    json_int(json,"worldAnalogScale",g_swWorldAnalogScale,first);
    json_int(json,"worldDpadScale",g_swWorldDpadScale,first);
    json_int(json,"worldArrowScale",g_swWorldArrowScale,first);
    json_int(json,"WorldPortalTextScale",g_swWorldPortalTextScale,first);
    json_int(json,"WorldMoveTextScale",g_swWorldMoveTextScale,first);
    json_int(json,"WorldReturnTextScale",g_swWorldReturnTextScale,first);
    json_int(json,"worldArrowX",g_swWorldArrowX,first);
    json_int(json,"worldArrowY",g_swWorldArrowY,first);
    json_int(json,"worldAnalogX",g_swWorldAnalogX,first);
    json_int(json,"worldAnalogY",g_swWorldAnalogY,first);
    json_int(json,"worldDpadX",g_swWorldDpadX,first);
    json_int(json,"worldDpadY",g_swWorldDpadY,first);
    json_int(json,"WorldPortalTextX",g_swWorldPortalTextX,first);
    json_int(json,"WorldPortalTextY",g_swWorldPortalTextY,first);
    json_int(json,"WorldMoveTextX",g_swWorldMoveTextX,first);
    json_int(json,"WorldMoveTextY",g_swWorldMoveTextY,first);
    json_int(json,"WorldReturnTextX",g_swWorldReturnTextX,first);
    json_int(json,"WorldReturnTextY",g_swWorldReturnTextY,first);
    json_int(json,"triX",g_swTriX,first);
    json_int(json,"triY",g_swTriY,first);
    json_int(json,"triScale",g_swTriScale,first);
    json_int(json,"squareX",g_swSquareX,first);
    json_int(json,"squareY",g_swSquareY,first);
    json_int(json,"squareScale",g_swSquareScale,first);
    json_int(json,"circleX",g_swCircleX,first);
    json_int(json,"circleY",g_swCircleY,first);
    json_int(json,"circleScale",g_swCircleScale,first);
    json_int(json,"crossX",g_swCrossX,first);
    json_int(json,"crossY",g_swCrossY,first);
    json_int(json,"crossScale",g_swCrossScale,first);
    json_int(json,"fishingCheckX",g_swFishingCheckX,first);
    json_int(json,"fishingCheckY",g_swFishingCheckY,first);
    json_int(json,"fishingCheckScale",g_swFishingCheckScale,first);
    json_int(json,"r1X",g_swR1X,first);
    json_int(json,"r1Y",g_swR1Y,first);
    json_int(json,"r1Scale",g_swR1Scale,first);
    json_int(json,"guideX",g_swGuideX,first);
    json_int(json,"guideY",g_swGuideY,first);
    json_int(json,"guideScale",g_swGuideScale,first);
    json_int(json,"dpadX",g_swDpadX,first);
    json_int(json,"dpadY",g_swDpadY,first);
    json_int(json,"dpadScale",g_swDpadScale,first);
    json_int(json,"itemTextX",g_swItemTextX,first);
    json_int(json,"itemTextY",g_swItemTextY,first);
    json_int(json,"itemTextScale",g_swItemTextScale,first);
    json_int(json,"mapTextX",g_swMapTextX,first);
    json_int(json,"mapTextY",g_swMapTextY,first);
    json_int(json,"mapTextScale",g_swMapTextScale,first);
    json_int(json,"itemsAnchorX",g_swItemsAnchorX,first);
    json_int(json,"itemsAnchorY",g_swItemsAnchorY,first);
    json_int(json,"itemSquareX",g_swItemSquareX,first);
    json_int(json,"itemSquareY",g_swItemSquareY,first);
    json_int(json,"itemSquareScale",g_swItemSquareScale,first);
    json_int(json,"itemTriangleX",g_swItemTriangleX,first);
    json_int(json,"itemTriangleY",g_swItemTriangleY,first);
    json_int(json,"itemTriangleScale",g_swItemTriangleScale,first);
    json_int(json,"itemCircleX",g_swItemCircleX,first);
    json_int(json,"itemCircleY",g_swItemCircleY,first);
    json_int(json,"itemCircleScale",g_swItemCircleScale,first);
    json_int(json,"itemR1X",g_swItemR1X,first);
    json_int(json,"itemR1Y",g_swItemR1Y,first);
    json_int(json,"itemR1Scale",g_swItemR1Scale,first);
    json_int(json,"swordX",g_swSwordX,first);
    json_int(json,"swordY",g_swSwordY,first);
    json_int(json,"swordScale",g_swSwordScale,first);
    json_int(json,"midnaX",g_swMidnaX,first);
    json_int(json,"midnaY",g_swMidnaY,first);
    json_int(json,"midnaScale",g_swMidnaScale,first);
    json_int(json,"howlActionX",g_swHowlActionX,first);
    json_int(json,"howlActionY",g_swHowlActionY,first);
    json_int(json,"howlActionScale",g_swHowlActionScale,first);
    json_int(json,"shopActionX",g_swShopActionX,first);
    json_int(json,"shopActionY",g_swShopActionY,first);
    json_int(json,"shopActionScale",g_swShopActionScale,first);
    json_int(json,"howlBackX",g_swHowlBackX,first);
    json_int(json,"howlBackY",g_swHowlBackY,first);
    json_int(json,"howlBackScale",g_swHowlBackScale,first);
    json_int(json,"shopBackX",g_swShopBackX,first);
    json_int(json,"shopBackY",g_swShopBackY,first);
    json_int(json,"shopBackScale",g_swShopBackScale,first);
    json_int(json,"actionTextX",g_swActionTextX,first);
    json_int(json,"actionTextY",g_swActionTextY,first);
    json_int(json,"actionTextScale",g_swActionTextScale,first);
    json_int(json,"dialogActionTextX",g_swDialogActionTextX,first);
    json_int(json,"dialogActionTextY",g_swDialogActionTextY,first);
    json_int(json,"whistleActionX",g_swWhistleActionX,first);
    json_int(json,"whistleActionY",g_swWhistleActionY,first);
    json_int(json,"whistleActionScale",g_swWhistleActionScale,first);
    json_int(json,"whistleBackX",g_swWhistleBackX,first);
    json_int(json,"whistleBackY",g_swWhistleBackY,first);
    json_int(json,"whistleBackScale",g_swWhistleBackScale,first);
    json_int(json,"backTextX",g_swBackTextX,first);
    json_int(json,"backTextY",g_swBackTextY,first);
    json_int(json,"backTextScale",g_swBackTextScale,first);
    json_int(json,"wolfSenseX",g_swWolfSenseX,first);
    json_int(json,"wolfSenseY",g_swWolfSenseY,first);
    json_int(json,"wolfSenseScale",g_swWolfSenseScale,first);
    json_int(json,"wolfDigX",g_swWolfDigX,first);
    json_int(json,"wolfDigY",g_swWolfDigY,first);
    json_int(json,"wolfDigScale",g_swWolfDigScale,first);
    json_int(json,"actionGlowX",g_swActionGlowX,first);
    json_int(json,"actionGlowY",g_swActionGlowY,first);
    json_int(json,"actionGlowScale",g_swActionGlowScale,first);
    json_int(json,"backGlowX",g_swBackGlowX,first);
    json_int(json,"backGlowY",g_swBackGlowY,first);
    json_int(json,"backGlowScale",g_swBackGlowScale,first);
    json_int(json,"wolfXGlowX",g_swWolfXGlowX,first);
    json_int(json,"wolfXGlowY",g_swWolfXGlowY,first);
    json_int(json,"wolfXGlowScale",g_swWolfXGlowScale,first);
    json_int(json,"wolfYGlowX",g_swWolfYGlowX,first);
    json_int(json,"wolfYGlowY",g_swWolfYGlowY,first);
    json_int(json,"wolfYGlowScale",g_swWolfYGlowScale,first);
    json_int(json,"fileCrossX",g_swFileCrossX,first);
    json_int(json,"fileCrossY",g_swFileCrossY,first);
    json_int(json,"fileCircleX",g_swFileCircleX,first);
    json_int(json,"fileCircleY",g_swFileCircleY,first);
    json_int(json,"saveCrossX",g_swSaveCrossX,first);
    json_int(json,"saveCrossY",g_swSaveCrossY,first);
    json_int(json,"saveCircleX",g_swSaveCircleX,first);
    json_int(json,"saveCircleY",g_swSaveCircleY,first);
    json_int(json,"menuCrossX",g_swMenuCrossX,first);
    json_int(json,"menuCrossY",g_swMenuCrossY,first);
    json_int(json,"menuCrossScale",g_swMenuCrossScale,first);
    json_int(json,"menuCircleX",g_swMenuCircleX,first);
    json_int(json,"menuCircleY",g_swMenuCircleY,first);
    json_int(json,"menuCircleScale",g_swMenuCircleScale,first);
    json_int(json,"menuConfirmTextX",g_swMenuConfirmTextX,first);
    json_int(json,"menuConfirmTextY",g_swMenuConfirmTextY,first);
    json_int(json,"menuConfirmTextScale",g_swMenuConfirmTextScale,first);
    json_int(json,"menuBackTextX",g_swMenuBackTextX,first);
    json_int(json,"menuBackTextY",g_swMenuBackTextY,first);
    json_int(json,"menuBackTextScale",g_swMenuBackTextScale,first);
    json_int(json,"menuOrnamentX",g_swMenuOrnamentX,first);
    json_int(json,"menuOrnamentY",g_swMenuOrnamentY,first);
    json_int(json,"menuOrnamentScale",g_swMenuOrnamentScale,first);
    json_int(json,"mapCrossX",g_swMapCrossX,first);
    json_int(json,"mapCrossY",g_swMapCrossY,first);
    json_int(json,"mapCrossScale",g_swMapCrossScale,first);
    json_int(json,"mapCircleX",g_swMapCircleX,first);
    json_int(json,"mapCircleY",g_swMapCircleY,first);
    json_int(json,"mapCircleScale",g_swMapCircleScale,first);
    json_int(json,"mapConfirmTextX",g_swMapConfirmTextX,first);
    json_int(json,"mapConfirmTextY",g_swMapConfirmTextY,first);
    json_int(json,"mapConfirmTextScale",g_swMapConfirmTextScale,first);
    json_int(json,"mapBackTextX",g_swMapBackTextX,first);
    json_int(json,"mapBackTextY",g_swMapBackTextY,first);
    json_int(json,"mapBackTextScale",g_swMapBackTextScale,first);
    json_int(json,"mapOrnamentX",g_swMapOrnamentX,first);
    json_int(json,"mapOrnamentY",g_swMapOrnamentY,first);
    json_int(json,"mapOrnamentScale",g_swMapOrnamentScale,first);
    json_int(json,"hudOrnamentX",g_swHudOrnamentX,first);
    json_int(json,"hudOrnamentY",g_swHudOrnamentY,first);
    json_int(json,"hudOrnamentScale",g_swHudOrnamentScale,first);
    json_int(json,"wheelSquareX",g_swWheelSquareX,first);
    json_int(json,"wheelSquareY",g_swWheelSquareY,first);
    json_int(json,"wheelSquareScale",g_swWheelSquareScale,first);
    json_int(json,"wheelTriangleX",g_swWheelTriangleX,first);
    json_int(json,"wheelTriangleY",g_swWheelTriangleY,first);
    json_int(json,"wheelTriangleScale",g_swWheelTriangleScale,first);
    json_int(json,"wheelSelectAnalogX",g_swWheelSelectAnalogX,first);
    json_int(json,"wheelSelectAnalogY",g_swWheelSelectAnalogY,first);
    json_int(json,"wheelSelectAnalogScale",g_swWheelSelectAnalogScale,first);
    json_int(json,"wheelDirectAnalogX",g_swWheelDirectAnalogX,first);
    json_int(json,"wheelDirectAnalogY",g_swWheelDirectAnalogY,first);
    json_int(json,"wheelDirectAnalogScale",g_swWheelDirectAnalogScale,first);
    json_int(json,"wheelL2X",g_swWheelL2X,first);
    json_int(json,"wheelL2Y",g_swWheelL2Y,first);
    json_int(json,"wheelL2Scale",g_swWheelL2Scale,first);
    json_int(json,"wheelR2X",g_swWheelR2X,first);
    json_int(json,"wheelR2Y",g_swWheelR2Y,first);
    json_int(json,"wheelR2Scale",g_swWheelR2Scale,first);
    json_int(json,"dungeonMapCrossX",g_swDungeonMapCrossX,first);
    json_int(json,"dungeonMapCrossY",g_swDungeonMapCrossY,first);
    json_int(json,"dungeonMapCrossScale",g_swDungeonMapCrossScale,first);
    json_int(json,"dungeonMapCircleX",g_swDungeonMapCircleX,first);
    json_int(json,"dungeonMapCircleY",g_swDungeonMapCircleY,first);
    json_int(json,"dungeonMapCircleScale",g_swDungeonMapCircleScale,first);
    json_int(json,"dungeonMapConfirmTextX",g_swDungeonMapConfirmTextX,first);
    json_int(json,"dungeonMapConfirmTextY",g_swDungeonMapConfirmTextY,first);
    json_int(json,"dungeonMapConfirmTextScale",g_swDungeonMapConfirmTextScale,first);
    json_int(json,"dungeonMapBackTextX",g_swDungeonMapBackTextX,first);
    json_int(json,"dungeonMapBackTextY",g_swDungeonMapBackTextY,first);
    json_int(json,"dungeonMapBackTextScale",g_swDungeonMapBackTextScale,first);
    json_bool(json,"dpadShadowsEnabled",g_swDpadShadowsEnabled,first);
    json_bool(json,"worldMapArrowsEnabled",g_swWorldArrows,first);
    json_bool(json,"dpadArrowsEnabled",g_swDpadArrowsEnabled,first);
    json_bool(json,"dpadMapAnimation",g_swDpadMapAnimation,first);
    json_bool(json,"actionGlowEnabled",g_swActionGlowEnabled,first);
    json_bool(json,"backGlowEnabled",g_swBackGlowEnabled,first);
    json_bool(json,"glowAdjustmentPreview",g_swGlowPreview,first);
    json_bool(json,"wolfXGlowEnabled",g_swWolfXGlowEnabled,first);
    json_bool(json,"wolfYGlowEnabled",g_swWolfYGlowEnabled,first);
    json_bool(json,"wolfGlowPreview",g_swWolfGlowPreview,first);
    json_bool(json,"backButtonAnimation",g_swBackButtonAnim,first);
    json_bool(json,"backTextAnimation",g_swBackTextAnim,first);
    json_bool(json,"menuPromptOrnament",g_swMenuPromptOrnament,first);
    json_bool(json,"mapOrnamentEnabled",g_swMapOrnamentEnabled,first);
    json_bool(json,"hudOrnamentEnabled",g_swHudOrnamentEnabled,first);
    json_bool(json,"itemSquareFlipH",g_swItemSquareFlipH,first);
    json_bool(json,"itemSquareFlipV",g_swItemSquareFlipV,first);
    json_bool(json,"itemTriangleFlipH",g_swItemTriangleFlipH,first);
    json_bool(json,"itemTriangleFlipV",g_swItemTriangleFlipV,first);
    json_bool(json,"swordFlipH",g_swSwordFlipH,first);
    json_bool(json,"swordFlipV",g_swSwordFlipV,first);
    json += "\n      }\n";
    json += "    }\n";
    json += "  }\n";
    json += "}\n";

    std::string path=dataDir;
    if (!path.empty() && path.back()!='/' && path.back()!='\\') path += '/';
    path += "calibration_export.json";

    std::FILE* fp=std::fopen(path.c_str(),"wb");
    if (fp == nullptr) {
        svc_log->error(mod_ctx,"Calibration export failed: could not open calibration_export.json");
        return;
    }
    const size_t written=std::fwrite(json.data(),1,json.size(),fp);
    std::fclose(fp);
    if (written != json.size()) {
        svc_log->error(mod_ctx,"Calibration export failed: incomplete write");
        return;
    }

    std::string message="Calibration exported to: ";
    message += path;
    svc_log->info(mod_ctx,message.c_str());
}


bool base_xy_preset_selected(ModContext*,void*);
bool swap_xy_preset_selected(ModContext*,void*);
bool swap_yb_preset_selected(ModContext*,void*);
bool swap_xyxb_preset_selected(ModContext*,void*);
void select_base_xy_preset(ModContext*,void*);
void select_swap_xy_preset(ModContext*,void*);
void select_swap_yb_preset(ModContext*,void*);
void select_swap_xyxb_preset(ModContext*,void*);



void export_midna_diagnostic(ModContext*,void*) {
    if (!kDeveloperOptions || svc_host == nullptr || svc_log == nullptr) return;

    const char* dataDir=nullptr;
    if (svc_host->data_dir(mod_ctx,&dataDir) != MOD_OK || dataDir == nullptr || dataDir[0] == '\0') {
        svc_log->error(mod_ctx,"Midna diagnostic export failed: mod data directory unavailable");
        return;
    }

    std::string path=dataDir;
    if (!path.empty() && path.back()!='/' && path.back()!='\\') path += '/';
    path += "midna_diagnostic.txt";

    std::FILE* fp=std::fopen(path.c_str(),"wb");
    if (fp==nullptr) {
        svc_log->error(mod_ctx,"Midna diagnostic export failed: could not open output file");
        return;
    }

    const char* header =
        "TP Classic Modern Controller UI - Midna Diagnostic\n"
        "Reproduce the bad Midna size, then export this file.\n\n";
    std::fwrite(header,1,std::strlen(header),fp);
    if (!s_midnaDiagnostic.empty())
        std::fwrite(s_midnaDiagnostic.data(),1,s_midnaDiagnostic.size(),fp);
    else {
        const char* empty="No Midna snapshots were captured yet.\n";
        std::fwrite(empty,1,std::strlen(empty),fp);
    }
    std::fclose(fp);

    std::string msg="Midna diagnostic exported to: ";
    msg+=path;
    svc_log->info(mod_ctx,msg.c_str());
}

ModResult build_derived_b_swap_panel(UiElementHandle pane,bool xyBase) {
    const bool xyxb=xyBase;
    svc_ui->pane_add_text(mod_ctx,pane,
        xyxb ? "Swap X/Y base with the original GC X and GC B faces exchanged. GC B keeps the sword function."
             : "Base layout with the original GC Y and GC B faces exchanged. GC B keeps the sword function.",
        nullptr);
    UiControlDesc activate=UI_CONTROL_DESC_INIT;
    activate.kind=UI_CONTROL_BUTTON;
    activate.label=xyxb ? "Use Swap X/Y + X/B Preset" : "Use Swap Y/B Preset";
    activate.on_pressed=xyxb ? select_swap_xyxb_preset : select_swap_yb_preset;
    activate.is_selected=xyxb ? swap_xyxb_preset_selected : swap_yb_preset_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&activate,nullptr);

    ConfigVarHandle bx=xyxb?g_xyxbCircleX:g_ybCircleX, by=xyxb?g_xyxbCircleY:g_ybCircleY, bs=xyxb?g_xyxbCircleScale:g_ybCircleScale;
    ConfigVarHandle xx=xyxb?g_xyxbSquareX:g_ybSquareX, xy=xyxb?g_xyxbSquareY:g_ybSquareY, xs=xyxb?g_xyxbSquareScale:g_ybSquareScale;
    ConfigVarHandle yx=xyxb?g_xyxbTriX:g_ybTriX, yy=xyxb?g_xyxbTriY:g_ybTriY, ys=xyxb?g_xyxbTriScale:g_ybTriScale;
    ConfigVarHandle ixx=xyxb?g_xyxbItemSquareX:g_ybItemSquareX, ixy=xyxb?g_xyxbItemSquareY:g_ybItemSquareY, ixs=xyxb?g_xyxbItemSquareScale:g_ybItemSquareScale;
    ConfigVarHandle iyx=xyxb?g_xyxbItemTriangleX:g_ybItemTriangleX, iyy=xyxb?g_xyxbItemTriangleY:g_ybItemTriangleY, iys=xyxb?g_xyxbItemTriangleScale:g_ybItemTriangleScale;
    ConfigVarHandle ixh=xyxb?g_xyxbItemSquareFlipH:g_ybItemSquareFlipH, ixv=xyxb?g_xyxbItemSquareFlipV:g_ybItemSquareFlipV;
    ConfigVarHandle iyh=xyxb?g_xyxbItemTriangleFlipH:g_ybItemTriangleFlipH, iyv=xyxb?g_xyxbItemTriangleFlipV:g_ybItemTriangleFlipV;
    ConfigVarHandle swx=xyxb?g_xyxbSwordX:g_ybSwordX, swy=xyxb?g_xyxbSwordY:g_ybSwordY, sws=xyxb?g_xyxbSwordScale:g_ybSwordScale;
    ConfigVarHandle swh=xyxb?g_xyxbSwordFlipH:g_ybSwordFlipH, swv=xyxb?g_xyxbSwordFlipV:g_ybSwordFlipV;
    ConfigVarHandle btx=xyxb?g_xyxbBackTextX:g_ybBackTextX, bty=xyxb?g_xyxbBackTextY:g_ybBackTextY, bts=xyxb?g_xyxbBackTextScale:g_ybBackTextScale;
    ConfigVarHandle bgx=xyxb?g_xyxbBackGlowX:g_ybBackGlowX, bgy=xyxb?g_xyxbBackGlowY:g_ybBackGlowY, bgs=xyxb?g_xyxbBackGlowScale:g_ybBackGlowScale, bge=xyxb?g_xyxbBackGlowEnabled:g_ybBackGlowEnabled;
    ConfigVarHandle dax=xyxb?g_xyxbDialogActionTextX:g_ybDialogActionTextX, day=xyxb?g_xyxbDialogActionTextY:g_ybDialogActionTextY;
    ConfigVarHandle dbba=xyxb?g_xyxbBackButtonAnim:g_ybBackButtonAnim, dbta=xyxb?g_xyxbBackTextAnim:g_ybBackTextAnim;
    ConfigVarHandle wax=xyxb?g_xyxbWhistleActionX:g_ybWhistleActionX, way=xyxb?g_xyxbWhistleActionY:g_ybWhistleActionY, was=xyxb?g_xyxbWhistleActionScale:g_ybWhistleActionScale;
    ConfigVarHandle wbx=xyxb?g_xyxbWhistleBackX:g_ybWhistleBackX, wby=xyxb?g_xyxbWhistleBackY:g_ybWhistleBackY, wbs=xyxb?g_xyxbWhistleBackScale:g_ybWhistleBackScale;
    ConfigVarHandle hax=xyxb?g_xyxbHowlActionX:g_ybHowlActionX, hay=xyxb?g_xyxbHowlActionY:g_ybHowlActionY, has=xyxb?g_xyxbHowlActionScale:g_ybHowlActionScale;
    ConfigVarHandle hbx=xyxb?g_xyxbHowlBackX:g_ybHowlBackX, hby=xyxb?g_xyxbHowlBackY:g_ybHowlBackY, hbs=xyxb?g_xyxbHowlBackScale:g_ybHowlBackScale;
    ConfigVarHandle sax=xyxb?g_xyxbShopActionX:g_ybShopActionX, say=xyxb?g_xyxbShopActionY:g_ybShopActionY, sas=xyxb?g_xyxbShopActionScale:g_ybShopActionScale;
    ConfigVarHandle sbx=xyxb?g_xyxbShopBackX:g_ybShopBackX, sby=xyxb?g_xyxbShopBackY:g_ybShopBackY, sbs=xyxb?g_xyxbShopBackScale:g_ybShopBackScale;
    ConfigVarHandle stx=xyxb?g_xyxbWolfSenseX:g_ybWolfSenseX, sty=xyxb?g_xyxbWolfSenseY:g_ybWolfSenseY, sts=xyxb?g_xyxbWolfSenseScale:g_ybWolfSenseScale;
    ConfigVarHandle dtx=xyxb?g_xyxbWolfDigX:g_ybWolfDigX, dty=xyxb?g_xyxbWolfDigY:g_ybWolfDigY, dts=xyxb?g_xyxbWolfDigScale:g_ybWolfDigScale;
    ConfigVarHandle sgx=xyxb?g_xyxbWolfXGlowX:g_ybWolfXGlowX, sgy=xyxb?g_xyxbWolfXGlowY:g_ybWolfXGlowY, sgs=xyxb?g_xyxbWolfXGlowScale:g_ybWolfXGlowScale, sge=xyxb?g_xyxbWolfXGlowEnabled:g_ybWolfXGlowEnabled;
    ConfigVarHandle dgx=xyxb?g_xyxbWolfYGlowX:g_ybWolfYGlowX, dgy=xyxb?g_xyxbWolfYGlowY:g_ybWolfYGlowY, dgs=xyxb?g_xyxbWolfYGlowScale:g_ybWolfYGlowScale, dge=xyxb?g_xyxbWolfYGlowEnabled:g_ybWolfYGlowEnabled;
    ConfigVarHandle wx=xyxb?g_xyxbWheelSquareX:g_ybWheelSquareX, wy=xyxb?g_xyxbWheelSquareY:g_ybWheelSquareY, ws=xyxb?g_xyxbWheelSquareScale:g_ybWheelSquareScale;
    ConfigVarHandle wyx=xyxb?g_xyxbWheelTriangleX:g_ybWheelTriangleX, wyy=xyxb?g_xyxbWheelTriangleY:g_ybWheelTriangleY, wys=xyxb?g_xyxbWheelTriangleScale:g_ybWheelTriangleScale;

    svc_ui->pane_add_section(mod_ctx,pane,"HUD - GC B Function (Sword / Back)");
    add_num(pane,"GC B Slot - X",bx,0,2500,10," /10 px","Position of the GC B function slot.");
    add_num(pane,"GC B Slot - Y",by,-1000,2000,10," /10 px","Position of the GC B function slot.");
    add_num(pane,"GC B Slot - Scale",bs,50,200,1,"%","Scale of the face assigned to GC B.");
    svc_ui->pane_add_section(mod_ctx,pane,"HUD - GC X Function");
    add_num(pane,"GC X Slot - X",xx,0,2500,10," /10 px","Position of the GC X function slot.");
    add_num(pane,"GC X Slot - Y",xy,-1000,2000,10," /10 px","Position of the GC X function slot.");
    add_num(pane,"GC X Slot - Scale",xs,50,200,1,"%","Scale of the face assigned to GC X.");
    svc_ui->pane_add_section(mod_ctx,pane,"HUD - GC Y Function");
    add_num(pane,"GC Y Slot - X",yx,0,2500,10," /10 px","Position of the GC Y function slot.");
    add_num(pane,"GC Y Slot - Y",yy,-1000,2000,10," /10 px","Position of the GC Y function slot.");
    add_num(pane,"GC Y Slot - Scale",ys,50,200,1,"%","Scale of the face assigned to GC Y.");

    svc_ui->pane_add_section(mod_ctx,pane,"Sword on GC B");
    add_num(pane,"Sword - X",swx,-300,300,1," px","Sword offset for this preset.");
    add_num(pane,"Sword - Y",swy,-300,300,1," px","Sword offset for this preset.");
    add_num(pane,"Sword - Scale",sws,30,300,1,"%","Sword scale for this preset.");
    add_toggle(pane,"Sword - Flip H",swh,"Preset-local sword horizontal flip.");
    add_toggle(pane,"Sword - Flip V",swv,"Preset-local sword vertical flip.");

    svc_ui->pane_add_section(mod_ctx,pane,"GC B Back Text");
    add_num(pane,"Back Text - X",btx,-3000,3000,10,"/10 px","Preset-local contextual text offset for the GC B function.");
    add_num(pane,"Back Text - Y",bty,-3000,3000,10,"/10 px","Preset-local contextual text offset for the GC B function.");
    add_num(pane,"Back Text - Scale",bts,50,200,1,"%","Preset-local contextual text scale for the GC B function.");

    svc_ui->pane_add_section(mod_ctx,pane,"GC B Glow");
    add_num(pane,"Back Glow - X",bgx,-3000,3000,10,"/10 px","Preset-local Pikari glow offset for the GC B function.");
    add_num(pane,"Back Glow - Y",bgy,-3000,3000,10,"/10 px","Preset-local Pikari glow offset for the GC B function.");
    add_num(pane,"Back Glow - Scale",bgs,25,300,1,"%","Preset-local Pikari glow scale for the GC B function.");
    add_toggle(pane,"Back Glow Enabled",bge,"Enable the GC B Pikari glow in this preset.");
    add_toggle(pane,"Glow Adjustment Preview",g_glowPreview,"Force Action/Back glows visible while calibrating.");

    svc_ui->pane_add_section(mod_ctx,pane,"Whistle / Hawk Text");
    add_num(pane,"Whistle Action - X",wax,-3000,3000,10,"/10 px","Preset-local Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Y",way,-3000,3000,10,"/10 px","Preset-local Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Scale",was,25,250,1,"%","Preset-local Assoprar/action text scale.");
    add_num(pane,"Whistle Back - X",wbx,-3000,3000,10,"/10 px","Preset-local Voltar text offset.");
    add_num(pane,"Whistle Back - Y",wby,-3000,3000,10,"/10 px","Preset-local Voltar text offset.");
    add_num(pane,"Whistle Back - Scale",wbs,25,250,1,"%","Preset-local Voltar text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Dialogue");
    add_num(pane,"Dialogue Action Text - X",dax,-3000,3000,10,"/10 px","Preset-local additional action-text offset during dialogue.");
    add_num(pane,"Dialogue Action Text - Y",day,-3000,3000,10,"/10 px","Preset-local additional action-text offset during dialogue.");
    add_toggle(pane,"Dialogue Back Button Animation",dbba,"Keep the original GC B dialogue-prompt animation.");
    add_toggle(pane,"Dialogue Back Text Animation",dbta,"Keep the original dialogue Back-text animation.");

    svc_ui->pane_add_section(mod_ctx,pane,"Howling Text");
    add_num(pane,"Howl - X",hax,-3000,3000,10,"/10 px","Preset-local Howl text offset.");
    add_num(pane,"Howl - Y",hay,-3000,3000,10,"/10 px","Preset-local Howl text offset.");
    add_num(pane,"Howl - Scale",has,25,250,1,"%","Preset-local Howl text scale.");
    add_num(pane,"Exit - X",hbx,-3000,3000,10,"/10 px","Preset-local Exit text offset while howling.");
    add_num(pane,"Exit - Y",hby,-3000,3000,10,"/10 px","Preset-local Exit text offset while howling.");
    add_num(pane,"Exit - Scale",hbs,25,250,1,"%","Preset-local Exit text scale while howling.");

    svc_ui->pane_add_section(mod_ctx,pane,"Shop Text");
    add_num(pane,"Shop Confirm - X",sax,-3000,3000,10,"/10 px","Preset-local shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Y",say,-3000,3000,10,"/10 px","Preset-local shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Scale",sas,25,250,1,"%","Preset-local shop Confirm text scale.");
    add_num(pane,"Shop Exit - X",sbx,-3000,3000,10,"/10 px","Preset-local shop Exit text offset.");
    add_num(pane,"Shop Exit - Y",sby,-3000,3000,10,"/10 px","Preset-local shop Exit text offset.");
    add_num(pane,"Shop Exit - Scale",sbs,25,250,1,"%","Preset-local shop Exit text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"GC X Item");
    add_num(pane,"GC X Item - X",ixx,-3000,3000,10,"/10 px","Item offset for the original GC X function.");
    add_num(pane,"GC X Item - Y",ixy,-3000,3000,10,"/10 px","Item offset for the original GC X function.");
    add_num(pane,"GC X Item - Scale",ixs,30,200,1,"%","Item scale.");
    add_toggle(pane,"GC X Item - Flip H",ixh,"Horizontal flip.");
    add_toggle(pane,"GC X Item - Flip V",ixv,"Vertical flip.");
    svc_ui->pane_add_section(mod_ctx,pane,"GC Y Item");
    add_num(pane,"GC Y Item - X",iyx,-3000,3000,10,"/10 px","Item offset for the original GC Y function.");
    add_num(pane,"GC Y Item - Y",iyy,-3000,3000,10,"/10 px","Item offset for the original GC Y function.");
    add_num(pane,"GC Y Item - Scale",iys,30,200,1,"%","Item scale.");
    add_toggle(pane,"GC Y Item - Flip H",iyh,"Horizontal flip.");
    add_toggle(pane,"GC Y Item - Flip V",iyv,"Vertical flip.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf Text");
    add_num(pane,"Senses Text - X",stx,-3000,3000,10,"/10 px","Preset-local Senses text.");
    add_num(pane,"Senses Text - Y",sty,-3000,3000,10,"/10 px","Preset-local Senses text.");
    add_num(pane,"Senses Text - Scale",sts,50,200,1,"%","Preset-local Senses text scale.");
    add_num(pane,"Dig Text - X",dtx,-3000,3000,10,"/10 px","Preset-local Dig text.");
    add_num(pane,"Dig Text - Y",dty,-3000,3000,10,"/10 px","Preset-local Dig text.");
    add_num(pane,"Dig Text - Scale",dts,50,200,1,"%","Preset-local Dig text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf Glows");
    add_num(pane,"Senses Glow - X",sgx,-3000,3000,10,"/10 px","Preset-local Senses glow.");
    add_num(pane,"Senses Glow - Y",sgy,-3000,3000,10,"/10 px","Preset-local Senses glow.");
    add_num(pane,"Senses Glow - Scale",sgs,25,300,1,"%","Preset-local Senses glow scale.");
    add_toggle(pane,"Senses Glow Enabled",sge,"Enable Senses glow.");
    add_num(pane,"Dig Glow - X",dgx,-3000,3000,10,"/10 px","Preset-local Dig glow.");
    add_num(pane,"Dig Glow - Y",dgy,-3000,3000,10,"/10 px","Preset-local Dig glow.");
    add_num(pane,"Dig Glow - Scale",dgs,25,300,1,"%","Preset-local Dig glow scale.");
    add_toggle(pane,"Dig Glow Enabled",dge,"Enable Dig glow.");
    add_toggle(pane,"Wolf Glow Preview",g_wolfGlowPreview,"Force contextual glows visible while calibrating.");

    svc_ui->pane_add_section(mod_ctx,pane,"Item Wheel");
    add_num(pane,"GC X Assignment - X",wx,-3000,3000,10,"/10 px","Preset-local GC X assignment prompt.");
    add_num(pane,"GC X Assignment - Y",wy,-3000,3000,10,"/10 px","Preset-local GC X assignment prompt.");
    add_num(pane,"GC X Assignment - Scale",ws,30,250,1,"%","Preset-local GC X assignment scale.");
    add_num(pane,"GC Y Assignment - X",wyx,-3000,3000,10,"/10 px","Preset-local GC Y assignment prompt.");
    add_num(pane,"GC Y Assignment - Y",wyy,-3000,3000,10,"/10 px","Preset-local GC Y assignment prompt.");
    add_num(pane,"GC Y Assignment - Scale",wys,30,250,1,"%","Preset-local GC Y assignment scale.");
    return MOD_OK;
}
ModResult build_swap_yb_preset_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    return build_derived_b_swap_panel(pane,false);
}
ModResult build_swap_xyxb_preset_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    return build_derived_b_swap_panel(pane,true);
}

ModResult build_base_xy_preset_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    if (switch_controller_selected_now()) {
        svc_ui->pane_add_text(mod_ctx,pane,
            "Switch-only calibration. These values are stored separately from every PlayStation/Xbox preset.",
            nullptr);
    } else {
        svc_ui->pane_add_text(mod_ctx,pane,
            "Base X/Y layout. Activate this preset before calibrating these controls.",
            nullptr);
        UiControlDesc activate=UI_CONTROL_DESC_INIT;
        activate.kind=UI_CONTROL_BUTTON;
        activate.label="Use Base Preset";
        activate.on_pressed=select_base_xy_preset;
        activate.is_selected=base_xy_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&activate,nullptr);
    }

    svc_ui->pane_add_section(mod_ctx,pane,
        switch_controller_selected_now() ? "HUD - Y (GC) / X (Switch)" : "HUD - Y (GC) / Square (PS) / X (XB)");
    add_num(pane,"Y Slot - X",g_triX,0,2500,10," /10 px","Base-preset position of the original GC Y slot.");
    add_num(pane,"Y Slot - Y",g_triY,-1000,2000,10," /10 px","Base-preset position of the original GC Y slot.");
    add_num(pane,"Y Slot - Scale",g_triScale,50,200,1,"%","Base-preset Y-slot scale.");
    svc_ui->pane_add_section(mod_ctx,pane,
        switch_controller_selected_now() ? "HUD - X (GC) / Y (Switch)" : "HUD - X (GC) / Triangle (PS) / Y (XB)");
    add_num(pane,"X Slot - X",g_squareX,0,2500,10," /10 px","Base-preset position of the original GC X slot.");
    add_num(pane,"X Slot - Y",g_squareY,-1000,2000,10," /10 px","Base-preset position of the original GC X slot.");
    add_num(pane,"X Slot - Scale",g_squareScale,50,200,1,"%","Base-preset X-slot scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"HUD Item - X Slot");
    add_num(pane,"X Item - X",g_itemSquareX,-3000,3000,10,"/10 px","Base-preset item offset for original GC X.");
    add_num(pane,"X Item - Y",g_itemSquareY,-3000,3000,10,"/10 px","Base-preset item offset for original GC X.");
    add_num(pane,"X Item - Scale",g_itemSquareScale,30,200,1,"%","Base-preset item scale.");
    add_toggle(pane,"X Item - Flip H",g_itemSquareFlipH,"Base-preset horizontal flip.");
    add_toggle(pane,"X Item - Flip V",g_itemSquareFlipV,"Base-preset vertical flip.");
    svc_ui->pane_add_section(mod_ctx,pane,"HUD Item - Y Slot");
    add_num(pane,"Y Item - X",g_itemTriangleX,-3000,3000,10,"/10 px","Base-preset item offset for original GC Y.");
    add_num(pane,"Y Item - Y",g_itemTriangleY,-3000,3000,10,"/10 px","Base-preset item offset for original GC Y.");
    add_num(pane,"Y Item - Scale",g_itemTriangleScale,30,200,1,"%","Base-preset item scale.");
    add_toggle(pane,"Y Item - Flip H",g_itemTriangleFlipH,"Base-preset horizontal flip.");
    add_toggle(pane,"Y Item - Flip V",g_itemTriangleFlipV,"Base-preset vertical flip.");

    svc_ui->pane_add_section(mod_ctx,pane,"Whistle / Hawk Text");
    add_num(pane,"Whistle Action - X",g_whistleActionX,-3000,3000,10,"/10 px","Base-preset Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Y",g_whistleActionY,-3000,3000,10,"/10 px","Base-preset Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Scale",g_whistleActionScale,25,250,1,"%","Base-preset Assoprar/action text scale.");
    add_num(pane,"Whistle Back - X",g_whistleBackX,-3000,3000,10,"/10 px","Base-preset Voltar text offset.");
    add_num(pane,"Whistle Back - Y",g_whistleBackY,-3000,3000,10,"/10 px","Base-preset Voltar text offset.");
    add_num(pane,"Whistle Back - Scale",g_whistleBackScale,25,250,1,"%","Base-preset Voltar text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Dialogue");
    add_num(pane,"Dialogue Action Text - X",g_dialogActionTextX,-3000,3000,10,"/10 px","Base-preset additional action-text offset while dialogue is active.");
    add_num(pane,"Dialogue Action Text - Y",g_dialogActionTextY,-3000,3000,10,"/10 px","Base-preset additional action-text offset while dialogue is active.");
    add_toggle(pane,"Dialogue Back Button Animation",g_backButtonAnim,"Keep the original GC B dialogue-prompt animation.");
    add_toggle(pane,"Dialogue Back Text Animation",g_backTextAnim,"Keep the original dialogue Back-text animation.");

    svc_ui->pane_add_section(mod_ctx,pane,"Howling Text");
    add_num(pane,"Howl - X",g_howlActionX,-3000,3000,10,"/10 px","Base-preset Howl text offset.");
    add_num(pane,"Howl - Y",g_howlActionY,-3000,3000,10,"/10 px","Base-preset Howl text offset.");
    add_num(pane,"Howl - Scale",g_howlActionScale,25,250,1,"%","Base-preset Howl text scale.");
    add_num(pane,"Exit - X",g_howlBackX,-3000,3000,10,"/10 px","Base-preset Exit text offset while howling.");
    add_num(pane,"Exit - Y",g_howlBackY,-3000,3000,10,"/10 px","Base-preset Exit text offset while howling.");
    add_num(pane,"Exit - Scale",g_howlBackScale,25,250,1,"%","Base-preset Exit text scale while howling.");

    svc_ui->pane_add_section(mod_ctx,pane,"Shop Text");
    add_num(pane,"Shop Confirm - X",g_shopActionX,-3000,3000,10,"/10 px","Base-preset shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Y",g_shopActionY,-3000,3000,10,"/10 px","Base-preset shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Scale",g_shopActionScale,25,250,1,"%","Base-preset shop Confirm text scale.");
    add_num(pane,"Shop Exit - X",g_shopBackX,-3000,3000,10,"/10 px","Base-preset shop Exit text offset.");
    add_num(pane,"Shop Exit - Y",g_shopBackY,-3000,3000,10,"/10 px","Base-preset shop Exit text offset.");
    add_num(pane,"Shop Exit - Scale",g_shopBackScale,25,250,1,"%","Base-preset shop Exit text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Senses Text");
    add_num(pane,"Senses Text - X",g_wolfSenseX,-3000,3000,10,"/10 px","Base-preset Senses text offset.");
    add_num(pane,"Senses Text - Y",g_wolfSenseY,-3000,3000,10,"/10 px","Base-preset Senses text offset.");
    add_num(pane,"Senses Text - Scale",g_wolfSenseScale,50,200,1,"%","Base-preset Senses text scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Dig Text");
    add_num(pane,"Dig Text - X",g_wolfDigX,-3000,3000,10,"/10 px","Base-preset Dig text offset.");
    add_num(pane,"Dig Text - Y",g_wolfDigY,-3000,3000,10,"/10 px","Base-preset Dig text offset.");
    add_num(pane,"Dig Text - Scale",g_wolfDigScale,50,200,1,"%","Base-preset Dig text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Senses Glow");
    add_num(pane,"Senses Glow - X",g_wolfXGlowX,-3000,3000,10,"/10 px","Base-preset Senses glow offset.");
    add_num(pane,"Senses Glow - Y",g_wolfXGlowY,-3000,3000,10,"/10 px","Base-preset Senses glow offset.");
    add_num(pane,"Senses Glow - Scale",g_wolfXGlowScale,25,300,1,"%","Base-preset Senses glow scale.");
    add_toggle(pane,"Senses Glow Enabled",g_wolfXGlowEnabled,"Enable the base-preset Senses glow.");
    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Dig Glow");
    add_num(pane,"Dig Glow - X",g_wolfYGlowX,-700,3000,10,"/10 px","Base-preset Dig glow offset.");
    add_num(pane,"Dig Glow - Y",g_wolfYGlowY,-3000,3000,10,"/10 px","Base-preset Dig glow offset.");
    add_num(pane,"Dig Glow - Scale",g_wolfYGlowScale,25,300,1,"%","Base-preset Dig glow scale.");
    add_toggle(pane,"Dig Glow Enabled",g_wolfYGlowEnabled,"Enable the base-preset Dig glow.");
    add_toggle(pane,"Wolf Glow Preview",g_wolfGlowPreview,"Force the contextual X/Y glows visible while calibrating.");

    svc_ui->pane_add_section(mod_ctx,pane,"Item Wheel - X Assignment");
    add_num(pane,"X Assignment - X",g_wheelSquareX,-3000,3000,10,"/10 px","Base-preset Item Wheel X-slot offset.");
    add_num(pane,"X Assignment - Y",g_wheelSquareY,-3000,3000,10,"/10 px","Base-preset Item Wheel X-slot offset.");
    add_num(pane,"X Assignment - Scale",g_wheelSquareScale,30,250,1,"%","Base-preset Item Wheel X-slot scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"Item Wheel - Y Assignment");
    add_num(pane,"Y Assignment - X",g_wheelTriangleX,-3000,3000,10,"/10 px","Base-preset Item Wheel Y-slot offset.");
    add_num(pane,"Y Assignment - Y",g_wheelTriangleY,-3000,3000,10,"/10 px","Base-preset Item Wheel Y-slot offset.");
    add_num(pane,"Y Assignment - Scale",g_wheelTriangleScale,30,250,1,"%","Base-preset Item Wheel Y-slot scale.");
    return MOD_OK;
}

ModResult build_swap_xy_preset_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,
        "Alternate X/Y layout. Activate this preset before calibrating these controls.",
        nullptr);
    UiControlDesc activate=UI_CONTROL_DESC_INIT;
    activate.kind=UI_CONTROL_BUTTON;
    activate.label="Use Swap X/Y Preset";
    activate.on_pressed=select_swap_xy_preset;
    activate.is_selected=swap_xy_preset_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&activate,nullptr);

    svc_ui->pane_add_section(mod_ctx,pane,"HUD - Y (GC) / Triangle (PS) / Y (XB)");
    add_num(pane,"Y Slot - X",g_swapTriX,0,2500,10," /10 px","Swap-preset position of the original GC Y slot.");
    add_num(pane,"Y Slot - Y",g_swapTriY,-1000,2000,10," /10 px","Swap-preset position of the original GC Y slot.");
    add_num(pane,"Y Slot - Scale",g_swapTriScale,50,200,1,"%","Swap-preset Y-slot scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"HUD - X (GC) / Square (PS) / X (XB)");
    add_num(pane,"X Slot - X",g_swapSquareX,0,2500,10," /10 px","Swap-preset position of the original GC X slot.");
    add_num(pane,"X Slot - Y",g_swapSquareY,-1000,2000,10," /10 px","Swap-preset position of the original GC X slot.");
    add_num(pane,"X Slot - Scale",g_swapSquareScale,50,200,1,"%","Swap-preset X-slot scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"HUD Item - X Slot");
    add_num(pane,"X Item - X",g_swapItemSquareX,-3000,3000,10,"/10 px","Swap-preset item offset for original GC X.");
    add_num(pane,"X Item - Y",g_swapItemSquareY,-3000,3000,10,"/10 px","Swap-preset item offset for original GC X.");
    add_num(pane,"X Item - Scale",g_swapItemSquareScale,30,200,1,"%","Swap-preset item scale.");
    add_toggle(pane,"X Item - Flip H",g_swapItemSquareFlipH,"Swap-preset horizontal flip.");
    add_toggle(pane,"X Item - Flip V",g_swapItemSquareFlipV,"Swap-preset vertical flip.");
    svc_ui->pane_add_section(mod_ctx,pane,"HUD Item - Y Slot");
    add_num(pane,"Y Item - X",g_swapItemTriangleX,-3000,3000,10,"/10 px","Swap-preset item offset for original GC Y.");
    add_num(pane,"Y Item - Y",g_swapItemTriangleY,-3000,3000,10,"/10 px","Swap-preset item offset for original GC Y.");
    add_num(pane,"Y Item - Scale",g_swapItemTriangleScale,30,200,1,"%","Swap-preset item scale.");
    add_toggle(pane,"Y Item - Flip H",g_swapItemTriangleFlipH,"Swap-preset horizontal flip.");
    add_toggle(pane,"Y Item - Flip V",g_swapItemTriangleFlipV,"Swap-preset vertical flip.");

    svc_ui->pane_add_section(mod_ctx,pane,"Whistle / Hawk Text");
    add_num(pane,"Whistle Action - X",g_swapWhistleActionX,-3000,3000,10,"/10 px","Swap X/Y Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Y",g_swapWhistleActionY,-3000,3000,10,"/10 px","Swap X/Y Assoprar/action text offset.");
    add_num(pane,"Whistle Action - Scale",g_swapWhistleActionScale,25,250,1,"%","Swap X/Y Assoprar/action text scale.");
    add_num(pane,"Whistle Back - X",g_swapWhistleBackX,-3000,3000,10,"/10 px","Swap X/Y Voltar text offset.");
    add_num(pane,"Whistle Back - Y",g_swapWhistleBackY,-3000,3000,10,"/10 px","Swap X/Y Voltar text offset.");
    add_num(pane,"Whistle Back - Scale",g_swapWhistleBackScale,25,250,1,"%","Swap X/Y Voltar text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Dialogue");
    add_num(pane,"Dialogue Action Text - X",g_swapDialogActionTextX,-3000,3000,10,"/10 px","Swap X/Y additional action-text offset while dialogue is active.");
    add_num(pane,"Dialogue Action Text - Y",g_swapDialogActionTextY,-3000,3000,10,"/10 px","Swap X/Y additional action-text offset while dialogue is active.");
    add_toggle(pane,"Dialogue Back Button Animation",g_swapBackButtonAnim,"Keep the original GC B dialogue-prompt animation.");
    add_toggle(pane,"Dialogue Back Text Animation",g_swapBackTextAnim,"Keep the original dialogue Back-text animation.");

    svc_ui->pane_add_section(mod_ctx,pane,"Howling Text");
    add_num(pane,"Howl - X",g_swapHowlActionX,-3000,3000,10,"/10 px","Swap X/Y Howl text offset.");
    add_num(pane,"Howl - Y",g_swapHowlActionY,-3000,3000,10,"/10 px","Swap X/Y Howl text offset.");
    add_num(pane,"Howl - Scale",g_swapHowlActionScale,25,250,1,"%","Swap X/Y Howl text scale.");
    add_num(pane,"Exit - X",g_swapHowlBackX,-3000,3000,10,"/10 px","Swap X/Y Exit text offset while howling.");
    add_num(pane,"Exit - Y",g_swapHowlBackY,-3000,3000,10,"/10 px","Swap X/Y Exit text offset while howling.");
    add_num(pane,"Exit - Scale",g_swapHowlBackScale,25,250,1,"%","Swap X/Y Exit text scale while howling.");

    svc_ui->pane_add_section(mod_ctx,pane,"Shop Text");
    add_num(pane,"Shop Confirm - X",g_swapShopActionX,-3000,3000,10,"/10 px","Swap X/Y shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Y",g_swapShopActionY,-3000,3000,10,"/10 px","Swap X/Y shop Confirm text offset.");
    add_num(pane,"Shop Confirm - Scale",g_swapShopActionScale,25,250,1,"%","Swap X/Y shop Confirm text scale.");
    add_num(pane,"Shop Exit - X",g_swapShopBackX,-3000,3000,10,"/10 px","Swap X/Y shop Exit text offset.");
    add_num(pane,"Shop Exit - Y",g_swapShopBackY,-3000,3000,10,"/10 px","Swap X/Y shop Exit text offset.");
    add_num(pane,"Shop Exit - Scale",g_swapShopBackScale,25,250,1,"%","Swap X/Y shop Exit text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Senses Text");
    add_num(pane,"Senses Text - X",g_swapWolfSenseX,-3000,3000,10,"/10 px","Swap-preset Senses text offset.");
    add_num(pane,"Senses Text - Y",g_swapWolfSenseY,-3000,3000,10,"/10 px","Swap-preset Senses text offset.");
    add_num(pane,"Senses Text - Scale",g_swapWolfSenseScale,50,200,1,"%","Swap-preset Senses text scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Dig Text");
    add_num(pane,"Dig Text - X",g_swapWolfDigX,-3000,3000,10,"/10 px","Swap-preset Dig text offset.");
    add_num(pane,"Dig Text - Y",g_swapWolfDigY,-3000,3000,10,"/10 px","Swap-preset Dig text offset.");
    add_num(pane,"Dig Text - Scale",g_swapWolfDigScale,50,200,1,"%","Swap-preset Dig text scale.");

    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Senses Glow");
    add_num(pane,"Senses Glow - X",g_swapWolfXGlowX,-3000,3000,10,"/10 px","Swap-preset Senses glow offset.");
    add_num(pane,"Senses Glow - Y",g_swapWolfXGlowY,-3000,3000,10,"/10 px","Swap-preset Senses glow offset.");
    add_num(pane,"Senses Glow - Scale",g_swapWolfXGlowScale,25,300,1,"%","Swap-preset Senses glow scale.");
    add_toggle(pane,"Senses Glow Enabled",g_swapWolfXGlowEnabled,"Enable the swap-preset Senses glow.");
    svc_ui->pane_add_section(mod_ctx,pane,"Wolf - Dig Glow");
    add_num(pane,"Dig Glow - X",g_swapWolfYGlowX,-3000,3000,10,"/10 px","Swap-preset Dig glow offset.");
    add_num(pane,"Dig Glow - Y",g_swapWolfYGlowY,-3000,3000,10,"/10 px","Swap-preset Dig glow offset.");
    add_num(pane,"Dig Glow - Scale",g_swapWolfYGlowScale,25,300,1,"%","Swap-preset Dig glow scale.");
    add_toggle(pane,"Dig Glow Enabled",g_swapWolfYGlowEnabled,"Enable the swap-preset Dig glow.");
    add_toggle(pane,"Wolf Glow Preview",g_wolfGlowPreview,"Force the contextual X/Y glows visible while calibrating.");

    svc_ui->pane_add_section(mod_ctx,pane,"Item Wheel - X Assignment");
    add_num(pane,"X Assignment - X",g_swapWheelSquareX,-3000,3000,10,"/10 px","Swap-preset Item Wheel X-slot offset.");
    add_num(pane,"X Assignment - Y",g_swapWheelSquareY,-3000,3000,10,"/10 px","Swap-preset Item Wheel X-slot offset.");
    add_num(pane,"X Assignment - Scale",g_swapWheelSquareScale,30,250,1,"%","Swap-preset Item Wheel X-slot scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"Item Wheel - Y Assignment");
    add_num(pane,"Y Assignment - X",g_swapWheelTriangleX,-3000,3000,10,"/10 px","Swap-preset Item Wheel Y-slot offset.");
    add_num(pane,"Y Assignment - Y",g_swapWheelTriangleY,-3000,3000,10,"/10 px","Swap-preset Item Wheel Y-slot offset.");
    add_num(pane,"Y Assignment - Scale",g_swapWheelTriangleScale,30,250,1,"%","Swap-preset Item Wheel Y-slot scale.");
    return MOD_OK;
}

ModResult build_settings_0_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Shared gameplay HUD controls used by both X/Y presets.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"B (GC) / Circle (PS) / B (XB)");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - X",g_circleX,0,2500,10," /10 px","X position of the original B root.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Y",g_circleY,-1000,2000,10," /10 px","Y position of the original B root.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Scale",g_circleScale,50,200,1,"%","Visual scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"A (GC) / Cross (PS) / A (XB)");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - X",g_crossX,0,2500,10," /10 px","X position of the original A root.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Y",g_crossY,-1000,2000,10," /10 px","Y position of the original A root.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Scale",g_crossScale,50,200,1,"%","Visual scale.");
    svc_ui->pane_add_section(mod_ctx,pane,"Z (GC) / R1 (PS) / RB (XB)");
    add_num(pane,"Z (GC) -> R1 (PS) / RB (XB) - X",g_r1X,-1000,3000,10,"/10 px","Horizontal position of R1.");
    add_num(pane,"Z (GC) -> R1 (PS) / RB (XB) - Y",g_r1Y,-1000,3000,10,"/10 px","Vertical position of R1.");
    add_num(pane,"Z (GC) -> R1 (PS) / RB (XB) - Scale",g_r1Scale,50,200,1,"%","Visual scale of R1.");
    svc_ui->pane_add_section(mod_ctx,pane,"D-pad and Arrows");
    add_num(pane,"D-Pad X Offset",g_dpadX,-1000,1000,10,"/10 px","Moves the D-Pad texture, shadows and orange arrows together.");
    add_num(pane,"D-Pad Y Offset",g_dpadY,-1000,1000,10,"/10 px","Moves the D-Pad texture, shadows and orange arrows together.");
    add_num(pane,"D-Pad Scale",g_dpadScale,30,250,1,"%","Scales the D-Pad texture, shadows and orange arrows as one group.");
    add_toggle(pane,"Orange Arrows",g_dpadArrowsEnabled,"Shows or hides the orange directional indicators.");
    add_toggle(pane,"Map Rise Animation",g_dpadMapAnimation,"Keeps the original Items/Map group rise animation.");
    add_toggle(pane,"D-Pad Shadows",g_dpadShadowsEnabled,"Shows or hides the original D-Pad shadow/ring layers.");
    svc_ui->pane_add_section(mod_ctx,pane,"Items Label");
    add_num(pane,"ITEM Text X",g_itemTextX,-1000,1000,10,"/10 px","Horizontal offset of the ITEM label.");
    add_num(pane,"ITEM Text Y",g_itemTextY,-1000,1000,10,"/10 px","Vertical offset of the ITEM label.");
    add_num(pane,"ITEM Text Scale",g_itemTextScale,30,250,1,"%","Independent scale of the ITEM label.");
    svc_ui->pane_add_section(mod_ctx,pane,"Map Label");
    add_num(pane,"MAP Text X",g_mapTextX,-1000,1000,10,"/10 px","Horizontal offset of the MAP label.");
    add_num(pane,"MAP Text Y",g_mapTextY,-1000,1000,10,"/10 px","Vertical offset of the MAP label.");
    add_num(pane,"MAP Text Scale",g_mapTextScale,30,250,1,"%","Independent scale of the MAP label.");
    svc_ui->pane_add_section(mod_ctx,pane,"Ornament");
    add_num(pane,"HUD Ornament - X",g_hudOrnamentX,-3000,3000,10,"/10 px","Horizontal offset of the HUD ornament.");
    add_num(pane,"HUD Ornament - Y",g_hudOrnamentY,-3000,3000,10,"/10 px","Vertical offset of the HUD ornament.");
    add_num(pane,"HUD Ornament - Scale",g_hudOrnamentScale,25,250,1,"%","Scale of the decorative HUD ornament.");
    add_toggle(pane,"HUD Ornament - Enabled",g_hudOrnamentEnabled,"Shows or hides the decorative HUD ornament.");
    return MOD_OK;
}

ModResult build_settings_1_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Shared item, sword and Midna controls used by both X/Y presets.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Shared Item Position");
    add_num(pane,"Items Group X",g_itemsAnchorX,-3000,3000,10,"/10 px","Moves both normalized item anchors horizontally.");
    add_num(pane,"Items Group Y",g_itemsAnchorY,-3000,3000,10,"/10 px","Moves both normalized item anchors vertically.");
    svc_ui->pane_add_section(mod_ctx,pane,"Sword");
    add_num(pane,"Sword X Offset",g_swordX,-300,300,1," px","Horizontal offset of the sword.");
    add_num(pane,"Sword Y Offset",g_swordY,-300,300,1," px","Vertical offset of the sword.");
    add_num(pane,"Sword Scale",g_swordScale,50,300,1,"%","Scale relative to the sword original size.");
    add_toggle(pane,"Sword Flip Horizontal",g_swordFlipH,"Flips the sword texture horizontally.");
    add_toggle(pane,"Sword Flip Vertical",g_swordFlipV,"Flips the sword texture vertically.");
    svc_ui->pane_add_section(mod_ctx,pane,"Midna Portrait");
    add_num(pane,"Midna X Offset",g_midnaX,-3000,3000,10,"/10 px","Horizontal offset of the Midna icon.");
    add_num(pane,"Midna Y Offset",g_midnaY,-3000,3000,10,"/10 px","Vertical offset of the Midna icon.");
    add_num(pane,"Midna Scale",g_midnaScale,50,200,1,"%","Scale of the Midna icon.");
    return MOD_OK;
}

ModResult build_settings_2_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"General action and back labels and their glows. Dialogue, shops and howling have separate controls.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Action Text");
    add_num(pane,"A (GC) / Cross (PS) / A (XB) Action Text - X",g_actionTextX,-3000,3000,10,"/10 px","Horizontal offset of the original A contextual text.");
    add_num(pane,"A (GC) / Cross (PS) / A (XB) Action Text - Y",g_actionTextY,-3000,3000,10,"/10 px","Vertical offset of the original A contextual text.");
    add_num(pane,"A (GC) / Cross (PS) / A (XB) Action Text - Scale",g_actionTextScale,50,200,1,"%","Visual scale of the contextual text.");
    svc_ui->pane_add_section(mod_ctx,pane,"Back Text");
    add_num(pane,"B (GC) / Circle (PS) / B (XB) Back Text - X",g_backTextX,-3000,3000,10,"/10 px","Horizontal offset of the B / Back contextual text.");
    add_num(pane,"B (GC) / Circle (PS) / B (XB) Back Text - Y",g_backTextY,-3000,3000,10,"/10 px","Vertical offset of the B / Back contextual text.");
    add_num(pane,"B (GC) / Circle (PS) / B (XB) Back Text - Scale",g_backTextScale,50,200,1,"%","Visual scale of the contextual text.");
    svc_ui->pane_add_section(mod_ctx,pane,"A (GC) / Cross (PS) / A (XB) Glow");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) Glow - X",g_actionGlowX,-3000,3000,10,"/10 px","Horizontal adjustment relative to the original glow position.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) Glow - Y",g_actionGlowY,-3000,3000,10,"/10 px","Vertical adjustment relative to the original glow position.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) Glow - Scale",g_actionGlowScale,25,300,1,"%","Multiplies the original animated Pikari scale.");
    add_toggle(pane,"A (GC) -> Cross (PS) / A (XB) Glow Enabled",g_actionGlowEnabled,"Enables or disables the original Pikari glow for Cross / Action.");
    svc_ui->pane_add_section(mod_ctx,pane,"B (GC) / Circle (PS) / B (XB) Glow");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) Glow - X",g_backGlowX,-3000,3000,10,"/10 px","Horizontal adjustment relative to the original glow position.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) Glow - Y",g_backGlowY,-3000,3000,10,"/10 px","Vertical adjustment relative to the original glow position.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) Glow - Scale",g_backGlowScale,25,300,1,"%","Multiplies the original animated Pikari scale.");
    add_toggle(pane,"B (GC) -> Circle (PS) / B (XB) Glow Enabled",g_backGlowEnabled,"Enables or disables the original Pikari glow for Circle / Back.");
    svc_ui->pane_add_section(mod_ctx,pane,"Glow Preview");
    add_toggle(pane,"Glow Adjustment Preview",g_glowPreview,"Keeps enabled Action/Back glows visible while adjusting them. When disabled, normal in-game triggers and animations are restored.");
    return MOD_OK;
}

ModResult build_settings_3_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,
        "Wolf Senses/Dig, their glows, and Howling text are preset-specific. Use the active preset tab to calibrate them.",
        nullptr);
    return MOD_OK;
}

ModResult build_settings_4_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,
        "Shop Confirm/Exit calibration is preset-specific. Use the active preset tab to adjust these labels.",
        nullptr);
    return MOD_OK;
}

ModResult build_settings_5_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Independent Check / Verificar label during canoe fishing.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Check / Verificar Text");
    add_num(pane,"Fishing Check Text - X",g_fishingCheckX,-3000,3000,10,"/10 px","Horizontal offset of Check / Verificar during canoe fishing.");
    add_num(pane,"Fishing Check Text - Y",g_fishingCheckY,-3000,3000,10,"/10 px","Vertical offset of Check / Verificar during canoe fishing.");
    add_num(pane,"Fishing Check Text - Scale",g_fishingCheckScale,25,250,1,"%","Independent scale of the fishing R1 label, including its outline.");
    return MOD_OK;
}

ModResult build_settings_6_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Shared Item Wheel navigation controls. X/Y assignments are calibrated in the preset tabs.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Select Analog");
    add_num(pane,"Select Analog - X Offset",g_wheelSelectAnalogX,-3000,3000,10,"/10 px","Horizontal offset of the modern analog icon beside Select.");
    add_num(pane,"Select Analog - Y Offset",g_wheelSelectAnalogY,-3000,3000,10,"/10 px","Vertical offset of the modern analog icon beside Select.");
    add_num(pane,"Select Analog - Scale",g_wheelSelectAnalogScale,30,250,1,"%","Scale of the modern analog icon beside Select.");
    svc_ui->pane_add_section(mod_ctx,pane,"Direct Select - L (GC) / L2 (PS) / LT (XB)");
    add_num(pane,"L (GC) / L2 (PS) / LT (XB) - X Offset",g_wheelL2X,-3000,3000,10,"/10 px","Horizontal offset of L2 beside Direct Select.");
    add_num(pane,"L (GC) / L2 (PS) / LT (XB) - Y Offset",g_wheelL2Y,-3000,3000,10,"/10 px","Vertical offset of L2 beside Direct Select.");
    add_num(pane,"L (GC) / L2 (PS) / LT (XB) - Scale",g_wheelL2Scale,30,250,1,"%","Scale of L2 beside Direct Select.");
    svc_ui->pane_add_section(mod_ctx,pane,"Direct Select Analog");
    add_num(pane,"Direct Select Analog - X Offset",g_wheelDirectAnalogX,-3000,3000,10,"/10 px","Horizontal offset of the analog icon beside Direct Select.");
    add_num(pane,"Direct Select Analog - Y Offset",g_wheelDirectAnalogY,-3000,3000,10,"/10 px","Vertical offset of the analog icon beside Direct Select.");
    add_num(pane,"Direct Select Analog - Scale",g_wheelDirectAnalogScale,30,250,1,"%","Scale of the analog icon beside Direct Select.");
    svc_ui->pane_add_section(mod_ctx,pane,"Bow Combination - R (GC) / R2 (PS) / RT (XB)");
    add_num(pane,"R (GC) / R2 (PS) / RT (XB) - X Offset",g_wheelR2X,-3000,3000,10,"/10 px","Horizontal offset of R2.");
    add_num(pane,"R (GC) / R2 (PS) / RT (XB) - Y Offset",g_wheelR2Y,-3000,3000,10,"/10 px","Vertical offset of R2.");
    add_num(pane,"R (GC) / R2 (PS) / RT (XB) - Scale",g_wheelR2Scale,30,250,1,"%","Scale of R2.");
    return MOD_OK;
}

ModResult build_settings_7_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Shared prompts in Collection-style menus.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"A (GC) / Cross (PS) / A (XB) Icon");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - X",g_menuCrossX,-3000,3000,10,"/10 px","Horizontal offset of the shared menu Cross icon.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Y",g_menuCrossY,-3000,3000,10,"/10 px","Vertical offset of the shared menu Cross icon.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Scale",g_menuCrossScale,50,250,1,"%","Menu Cross scale. Uses the saved default preset.");
    svc_ui->pane_add_section(mod_ctx,pane,"Confirm Text");
    add_num(pane,"Confirm Text - X",g_menuConfirmTextX,-3000,3000,10,"/10 px","Horizontal offset of the text paired with A / Cross.");
    add_num(pane,"Confirm Text - Y",g_menuConfirmTextY,-3000,3000,10,"/10 px","Vertical offset of the text paired with A / Cross.");
    add_num(pane,"Confirm Text - Scale",g_menuConfirmTextScale,50,200,1,"%","Scale of the A / Confirm text container.");
    svc_ui->pane_add_section(mod_ctx,pane,"B (GC) / Circle (PS) / B (XB) Icon");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - X",g_menuCircleX,-3000,3000,10,"/10 px","Horizontal offset of the shared menu Circle icon.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Y",g_menuCircleY,-3000,3000,10,"/10 px","Vertical offset of the shared menu Circle icon.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Scale",g_menuCircleScale,50,250,1,"%","Menu Circle scale. Uses the saved default preset.");
    svc_ui->pane_add_section(mod_ctx,pane,"Back Text");
    add_num(pane,"Back Text - X",g_menuBackTextX,-3000,3000,10,"/10 px","Horizontal offset of the text paired with B / Circle (for example Back/Voltar).");
    add_num(pane,"Back Text - Y",g_menuBackTextY,-3000,3000,10,"/10 px","Vertical offset of the text paired with B / Circle.");
    add_num(pane,"Back Text - Scale",g_menuBackTextScale,50,200,1,"%","Scale of the B / Back text container.");
    svc_ui->pane_add_section(mod_ctx,pane,"Shared Ornament");
    add_num(pane,"Shared Ornament - X",g_menuOrnamentX,-3000,3000,10,"/10 px","Horizontal offset of the ornament on shared menus only.");
    add_num(pane,"Shared Ornament - Y",g_menuOrnamentY,-3000,3000,10,"/10 px","Vertical offset of the ornament on shared menus only.");
    add_num(pane,"Shared Ornament - Scale",g_menuOrnamentScale,25,250,1,"%","Scale of the ornament on shared menus only.");
    add_toggle(pane,"Shared Menu Ornament - Enabled",g_menuPromptOrnament,"Shows or hides the custom ornament on Collection-style shared menus. Map ornament settings are independent.");
    return MOD_OK;
}

ModResult build_settings_8_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"World-map icons and labels. The map ornament section is shared with the dungeon map.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Map Ornament (World and Dungeon)");
    add_num(pane,"Map Ornament - X",g_mapOrnamentX,-3000,3000,10,"/10 px","Horizontal offset of the ornament on map screens only.");
    add_num(pane,"Map Ornament - Y",g_mapOrnamentY,-3000,3000,10,"/10 px","Vertical offset of the ornament on map screens only.");
    add_num(pane,"Map Ornament - Scale",g_mapOrnamentScale,25,250,1,"%","Scale of the ornament on map screens only.");
    add_toggle(pane,"Map Ornament - Enabled",g_mapOrnamentEnabled,"Shows or hides the custom ornament on the Field/Dungeon Map prompt layout. R3 and map text are not affected.");
    svc_ui->pane_add_section(mod_ctx,pane,"A (GC) / Cross (PS) / A (XB) Icon");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - X",g_mapCrossX,-3000,3000,10,"/10 px","Horizontal offset of the Cross icon on World Map screens only.");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - Y",g_mapCrossY,-3000,3000,10,"/10 px","Vertical offset of the Cross icon on World Map screens only.");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - Scale",g_mapCrossScale,50,250,1,"%","Cross scale on World Map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"Confirm Text");
    add_num(pane,"Map Confirm Text - X",g_mapConfirmTextX,-3000,3000,10,"/10 px","Horizontal offset of the Confirm text on world map screens only.");
    add_num(pane,"Map Confirm Text - Y",g_mapConfirmTextY,-3000,3000,10,"/10 px","Vertical offset of the Confirm text on world map screens only.");
    add_num(pane,"Map Confirm Text - Scale",g_mapConfirmTextScale,50,200,1,"%","Confirm text scale on world map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"B (GC) / Circle (PS) / B (XB) Icon");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - X",g_mapCircleX,-3000,3000,10,"/10 px","Horizontal offset of the Circle icon on World Map screens only.");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - Y",g_mapCircleY,-3000,3000,10,"/10 px","Vertical offset of the Circle icon on World Map screens only.");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - Scale",g_mapCircleScale,50,250,1,"%","Circle scale on World Map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"Back Text");
    add_num(pane,"D-pad Back Text - X",g_WorldReturnTextX,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"D-pad Back Text - Y",g_WorldReturnTextY,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"D-pad Back Text - Scale",g_WorldReturnTextScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    add_num(pane,"Map Back Text - X",g_mapBackTextX,-3000,3000,10,"/10 px","Horizontal offset of the Back text on world map screens only.");
    add_num(pane,"Map Back Text - Y",g_mapBackTextY,-3000,3000,10,"/10 px","Vertical offset of the Back text on world map screens only.");
    add_num(pane,"Map Back Text - Scale",g_mapBackTextScale,50,200,1,"%","Back text scale on world map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"Orange Arrows");
    add_num(pane,"Orange Arrows - X Offset",g_worldArrowX,-3000,3000,10,"/10 px","Additional horizontal offset of both orange arrows relative to the D-pad.");
    add_num(pane,"Orange Arrows - Y Offset",g_worldArrowY,-3000,3000,10,"/10 px","Additional vertical offset of both orange arrows relative to the D-pad.");
    add_num(pane,"Orange Arrows - Scale",g_worldArrowScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    add_toggle(pane,"D-pad Orange Arrows - Enabled",g_worldArrows,"Show the native animated arrows. Disable to hide only the World Map arrows.");
    svc_ui->pane_add_section(mod_ctx,pane,"Back - D-pad");
    add_num(pane,"D-pad / Back Icon - X",g_worldDpadX,-3000,3000,10,"/10 px","Moves the D-pad and its orange arrows together.");
    add_num(pane,"D-pad / Back Icon - Y",g_worldDpadY,-3000,3000,10,"/10 px","Moves the D-pad and its orange arrows together.");
    add_num(pane,"D-pad / Back Icon - Scale",g_worldDpadScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    svc_ui->pane_add_section(mod_ctx,pane,"Portals - Z (GC) / R1 (PS) / RB (XB)");
    add_num(pane,"Z (GC) / R1 (PS) / RB (XB) / Portals Icon - X",g_worldR1X,-3000,3000,10,"/10 px","Moves only the World Map R1 icon.");
    add_num(pane,"Z (GC) / R1 (PS) / RB (XB) / Portals Icon - Y",g_worldR1Y,-3000,3000,10,"/10 px","Moves only the World Map R1 icon.");
    add_num(pane,"Z (GC) / R1 (PS) / RB (XB) / Portals Icon - Scale",g_worldR1Scale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    svc_ui->pane_add_section(mod_ctx,pane,"Move - Control Stick (GC) / L3 (PS) / LS (XB)");
    add_num(pane,"Control Stick (GC) / L3 (PS) / LS (XB) / Move Icon - X",g_worldAnalogX,-3000,3000,10,"/10 px","Moves only the World Map analog icon.");
    add_num(pane,"Control Stick (GC) / L3 (PS) / LS (XB) / Move Icon - Y",g_worldAnalogY,-3000,3000,10,"/10 px","Moves only the World Map analog icon.");
    add_num(pane,"Control Stick (GC) / L3 (PS) / LS (XB) / Move Icon - Scale",g_worldAnalogScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    svc_ui->pane_add_section(mod_ctx,pane,"Move Text");
    add_num(pane,"Move Text - X",g_WorldMoveTextX,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"Move Text - Y",g_WorldMoveTextY,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"Move Text - Scale",g_WorldMoveTextScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    svc_ui->pane_add_section(mod_ctx,pane,"Portals Text");
    add_num(pane,"Portals Text - X",g_WorldPortalTextX,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"Portals Text - Y",g_WorldPortalTextY,-3000,3000,10,"/10 px","Moves only this World Map label.");
    add_num(pane,"Portals Text - Scale",g_WorldPortalTextScale,25,250,1,"%","Independent scale. 100% preserves the accepted size.");
    return MOD_OK;
}

ModResult build_settings_9_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Independent dungeon-map icons and labels. Shared map ornament controls are in World Map.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"A (GC) / Cross (PS) / A (XB) Icon");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - X",g_dungeonMapCrossX,-3000,3000,10,"/10 px","Horizontal offset of the Cross icon on Dungeon Map screens only.");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - Y",g_dungeonMapCrossY,-3000,3000,10,"/10 px","Vertical offset of the Cross icon on Dungeon Map screens only.");
    add_num(pane,"Map A (GC) -> Cross (PS) / A (XB) - Scale",g_dungeonMapCrossScale,50,250,1,"%","Cross scale on Dungeon Map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"Confirm Text");
    add_num(pane,"Map Confirm Text - X",g_dungeonMapConfirmTextX,-3000,3000,10,"/10 px","Horizontal offset of the Confirm text on dungeon map screens only.");
    add_num(pane,"Map Confirm Text - Y",g_dungeonMapConfirmTextY,-3000,3000,10,"/10 px","Vertical offset of the Confirm text on dungeon map screens only.");
    add_num(pane,"Map Confirm Text - Scale",g_dungeonMapConfirmTextScale,50,200,1,"%","Confirm text scale on dungeon map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"B (GC) / Circle (PS) / B (XB) Icon");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - X",g_dungeonMapCircleX,-3000,3000,10,"/10 px","Horizontal offset of the Circle icon on Dungeon Map screens only.");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - Y",g_dungeonMapCircleY,-3000,3000,10,"/10 px","Vertical offset of the Circle icon on Dungeon Map screens only.");
    add_num(pane,"Map B (GC) -> Circle (PS) / B (XB) - Scale",g_dungeonMapCircleScale,50,250,1,"%","Circle scale on Dungeon Map screens only.");
    svc_ui->pane_add_section(mod_ctx,pane,"Back Text");
    add_num(pane,"Map Back Text - X",g_dungeonMapBackTextX,-3000,3000,10,"/10 px","Horizontal offset of the Back text on dungeon map screens only.");
    add_num(pane,"Map Back Text - Y",g_dungeonMapBackTextY,-3000,3000,10,"/10 px","Vertical offset of the Back text on dungeon map screens only.");
    add_num(pane,"Map Back Text - Scale",g_dungeonMapBackTextScale,50,200,1,"%","Back text scale on dungeon map screens only.");
    return MOD_OK;
}

ModResult build_settings_10_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Button positions for file selection and in-game saving.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"File Selection");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - X",g_fileCircleX,-1000,1000,10,"/10 px","Moves only the Circle icon on the initial file-select screen.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Y",g_fileCircleY,-1000,1000,10,"/10 px","Moves only the Circle icon on the initial file-select screen.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - X",g_fileCrossX,-1000,1000,10,"/10 px","Moves only the Cross icon on the initial file-select screen.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Y",g_fileCrossY,-1000,1000,10,"/10 px","Moves only the Cross icon on the initial file-select screen.");
    svc_ui->pane_add_section(mod_ctx,pane,"In-game Save");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - X",g_saveCircleX,-1000,1000,10,"/10 px","Moves only the Circle icon on the in-game save screen.");
    add_num(pane,"B (GC) -> Circle (PS) / B (XB) - Y",g_saveCircleY,-1000,1000,10,"/10 px","Moves only the Circle icon on the in-game save screen.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - X",g_saveCrossX,-1000,1000,10,"/10 px","Moves only the Cross icon on the in-game save screen.");
    add_num(pane,"A (GC) -> Cross (PS) / A (XB) - Y",g_saveCrossY,-1000,1000,10,"/10 px","Moves only the Cross icon on the in-game save screen.");
    return MOD_OK;
}

ModResult build_settings_11_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Calibration and restoring the saved default preset.",nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Button Layout Presets");
    svc_ui->pane_add_text(mod_ctx,pane,
        "Preset selection is visual-only. The public CONTROLLER UI contains the explicit input-sync button; calibration controls remain developer-only.",
        nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Calibration Export");
    svc_ui->pane_add_text(mod_ctx,pane,
        "Writes Base, Swap X/Y and shared calibration values to calibration_export.json in this mod's persistent Dusklight data folder.",
        nullptr);
    add_button(pane,"Export Calibration JSON",export_calibration,
        "Export the current calibration so it can be shared without screenshots.");
    svc_ui->pane_add_section(mod_ctx,pane,"Midna Diagnostic");
    svc_ui->pane_add_text(mod_ctx,pane,
        "After reproducing the oversized Midna after a cutscene/skip, export the captured HUD geometry and parent transforms.",
        nullptr);
    add_button(pane,"Export Midna Diagnostic",export_midna_diagnostic,
        "Writes midna_diagnostic.txt to the mod data folder. Send that file for analysis.");
    svc_ui->pane_add_section(mod_ctx,pane,"Visual HUD Editor");
    add_toggle(pane,"Enable Visual HUD Editor",g_visualHudEditorEnabled,
        "Developer-only visual editing mode. The editor is restricted to a whitelist of TP Classic elements with known X/Y/Scale config handles.");
    svc_ui->pane_add_text(mod_ctx,pane,
        "Foundation stage: this switch is wired to the dev configuration and whitelist. On-screen selection/dragging will be connected next without touching unknown game or Twilit Essentials panes.",
        nullptr);
    svc_ui->pane_add_section(mod_ctx,pane,"Calibration Guide");
    add_num(pane,"Guide X",g_guideX,-1000,3000,10,"/10 px","Horizontal position of the calibration guide.");
    add_num(pane,"Guide Y",g_guideY,-1000,3000,10,"/10 px","Vertical position of the calibration guide.");
    add_num(pane,"Guide Scale",g_guideScale,50,200,1,"%","Scale of the calibration guide.");
    add_toggle(pane,"Show Calibration Guide",g_showGuide,"Shows or hides the calibration guide.");
    svc_ui->pane_add_section(mod_ctx,pane,"Saved Default Preset");
    svc_ui->pane_add_text(mod_ctx,pane,"Restore the complete layout captured on 2026-09-26, including all contexts and effect settings.",nullptr);
    add_button(pane,"Restore Saved Defaults",reset_layout,"Replaces all layout editor values with the saved preset bundled with this version.");
    return MOD_OK;
}

ModResult build_dialogue_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,
        "Dialogue offsets and Back-prompt animation settings are preset-specific. Use the active preset tab to adjust them.",
        nullptr);
    return MOD_OK;
}

bool playstation_selected(ModContext*,void*) { return raw_controller_style()==0; }
bool xbox_selected(ModContext*,void*) { return raw_controller_style()==1; }
bool switch_selected(ModContext*,void*) { return raw_controller_style()==2; }
void select_playstation(ModContext*,void*) {
    svc_config->set_int(mod_ctx,g_controllerStyle,0);
}
void select_xbox(ModContext*,void*) {
    svc_config->set_int(mod_ctx,g_controllerStyle,1);
}
void select_switch(ModContext*,void*) {
    svc_config->set_int(mod_ctx,g_controllerStyle,2);
}


static int layout_preset_index(ButtonLayoutPreset preset) {
    switch (preset) {
    case ButtonLayoutPreset::SwapXY: return 1;
    case ButtonLayoutPreset::SwapYB: return 2;
    case ButtonLayoutPreset::SwapXYXB: return 3;
    default: return 0;
    }
}

static void input_preset_sources(int preset,int out[4]) {
    out[0]=0; out[1]=1; out[2]=2; out[3]=3;
    switch (preset) {
    case 1: out[2]=3; out[3]=2; break;
    case 2: out[1]=3; out[3]=1; break;
    case 3: out[1]=3; out[2]=1; out[3]=2; break;
    default: break;
    }
}

static bool sync_dusklight_face_inputs(ButtonLayoutPreset targetPreset,bool switchNativeLayout) {
    const s32 controllerIndex=PADGetIndexForPort(PAD_CHAN0);
    if (controllerIndex < 0)
        return false;

    u32 count=0;
    PADButtonMapping* mappings=PADGetButtonMappings(PAD_CHAN0,&count);
    if (mappings==nullptr || count==0)
        return false;

    PADButtonMapping* faces[4]={nullptr,nullptr,nullptr,nullptr};
    for (u32 i=0;i<count;++i) {
        switch (mappings[i].padButton) {
        case PAD_BUTTON_A: faces[0]=&mappings[i]; break;
        case PAD_BUTTON_B: faces[1]=&mappings[i]; break;
        case PAD_BUTTON_X: faces[2]=&mappings[i]; break;
        case PAD_BUTTON_Y: faces[3]=&mappings[i]; break;
        default: break;
        }
    }
    for (int i=0;i<4;++i)
        if (faces[i]==nullptr) return false;

    // Dusklight stores gamepad face buttons using SDL's positional native-button
    // indices: South=0, East=1, West=2, North=3. Apply an absolute mapping
    // instead of permuting whatever happens to be configured already. This makes
    // Sync Controller Inputs authoritative even after A/B/X/Y were edited by hand.
    constexpr u32 kNativeSouth=0;
    constexpr u32 kNativeEast=1;
    constexpr u32 kNativeWest=2;
    constexpr u32 kNativeNorth=3;

    const int target=switchNativeLayout ? 0 : layout_preset_index(targetPreset);

    // PlayStation/Xbox Base follows Cross/A, Circle/B, Triangle/Y, Square/X.
    // Switch Base follows its native physical labels: A=right, B=bottom,
    // X=top, Y=left.
    u32 base[4]={
        switchNativeLayout ? kNativeEast : kNativeSouth, // GC A
        switchNativeLayout ? kNativeSouth : kNativeEast, // GC B
        kNativeNorth, // GC X
        kNativeWest,  // GC Y
    };

    int newSources[4];
    input_preset_sources(target,newSources);
    for (int targetSlot=0;targetSlot<4;++targetSlot)
        faces[targetSlot]->nativeButton=base[newSources[targetSlot]];

    PADSerializeMappings();

    if (g_inputSyncLastPreset!=0)
        svc_config->set_int(mod_ctx,g_inputSyncLastPreset,target);
    if (g_inputSyncControllerIndex!=0)
        svc_config->set_int(mod_ctx,g_inputSyncControllerIndex,controllerIndex);

    if (svc_log!=nullptr) {
        const char* name="Base";
        if (!switchNativeLayout) {
            if (target==1) name="Swap X/Y";
            else if (target==2) name="Swap Y/B";
            else if (target==3) name="Swap X/Y + X/B";
        }
        std::string msg="Controller face-button mapping synced through Dusklight: ";
        if (switchNativeLayout) msg+="Switch ";
        msg+=name;
        svc_log->info(mod_ctx,msg.c_str());
    }
    return true;
}

bool base_xy_preset_selected(ModContext*,void*) { return current_layout_preset()==ButtonLayoutPreset::Base; }
bool switch_base_preset_selected(ModContext*,void*) { return switch_controller_selected_now(); }
void select_switch_base_preset(ModContext*,void*) {
    // Switch currently exposes only its native Base layout. Keep the persisted
    // PlayStation/Xbox preset untouched so it is restored when changing design.
}
bool swap_xy_preset_selected(ModContext*,void*) { return current_layout_preset()==ButtonLayoutPreset::SwapXY; }
bool swap_yb_preset_selected(ModContext*,void*) { return current_layout_preset()==ButtonLayoutPreset::SwapYB; }
bool swap_xyxb_preset_selected(ModContext*,void*) { return current_layout_preset()==ButtonLayoutPreset::SwapXYXB; }
void select_base_xy_preset(ModContext*,void*) {
    if(g_buttonLayoutPreset!=0) svc_config->set_int(mod_ctx,g_buttonLayoutPreset,0);
}
void select_swap_xy_preset(ModContext*,void*) {
    if(g_buttonLayoutPreset!=0) svc_config->set_int(mod_ctx,g_buttonLayoutPreset,1);
}
void select_swap_yb_preset(ModContext*,void*) {
    if(g_buttonLayoutPreset!=0) svc_config->set_int(mod_ctx,g_buttonLayoutPreset,2);
}
void select_swap_xyxb_preset(ModContext*,void*) {
    if(g_buttonLayoutPreset!=0) svc_config->set_int(mod_ctx,g_buttonLayoutPreset,3);
}

void sync_selected_layout_preset(ModContext*,void*) {
    const bool switchNativeLayout=switch_controller_selected_now();
    const ButtonLayoutPreset preset=switchNativeLayout ? ButtonLayoutPreset::Base : current_layout_preset();
    if (!sync_dusklight_face_inputs(preset,switchNativeLayout) && svc_log!=nullptr) {
        svc_log->warn(mod_ctx,
            "Controller input sync skipped: no gamepad mapping is active on Port 1. Keyboard bindings were not changed.");
    }
}

bool public_back_animation_selected(ModContext*,void*) {
    return cfg_bool(layout_handle4(g_backButtonAnim,g_swapBackButtonAnim,g_ybBackButtonAnim,g_xyxbBackButtonAnim),false) &&
           cfg_bool(layout_handle4(g_backTextAnim,g_swapBackTextAnim,g_ybBackTextAnim,g_xyxbBackTextAnim),false);
}
void toggle_public_back_animation(ModContext*,void*) {
    const bool next = !public_back_animation_selected(nullptr,nullptr);
    svc_config->set_bool(mod_ctx,layout_handle4(g_backButtonAnim,g_swapBackButtonAnim,g_ybBackButtonAnim,g_xyxbBackButtonAnim),next);
    svc_config->set_bool(mod_ctx,layout_handle4(g_backTextAnim,g_swapBackTextAnim,g_ybBackTextAnim,g_xyxbBackTextAnim),next);
}

bool public_hud_glows_selected(ModContext*,void*) {
    const ConfigVarHandle actionEnabled = switch_variant(g_actionGlowEnabled);
    const ConfigVarHandle backEnabled =
        layout_handle4(g_backGlowEnabled,g_backGlowEnabled,g_ybBackGlowEnabled,g_xyxbBackGlowEnabled);
    const ConfigVarHandle wolfXEnabled =
        layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled);
    const ConfigVarHandle wolfYEnabled =
        layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled);
    return cfg_bool(actionEnabled,true) &&
           cfg_bool(backEnabled,true) &&
           cfg_bool(wolfXEnabled,true) &&
           cfg_bool(wolfYEnabled,true);
}
void toggle_public_hud_glows(ModContext*,void*) {
    const bool next = !public_hud_glows_selected(nullptr,nullptr);
    svc_config->set_bool(mod_ctx,switch_variant(g_actionGlowEnabled),next);
    svc_config->set_bool(mod_ctx,
        layout_handle4(g_backGlowEnabled,g_backGlowEnabled,g_ybBackGlowEnabled,g_xyxbBackGlowEnabled),next);
    svc_config->set_bool(mod_ctx,
        layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled),next);
    svc_config->set_bool(mod_ctx,
        layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled),next);
}

ModResult build_layout_panel(ModContext*,UiElementHandle pane,void*,ModError*) {
    svc_ui->pane_add_section(mod_ctx,pane,"TP Classic Modern Controller UI");
    svc_ui->pane_add_section(mod_ctx,pane,"Controller Design");
    svc_ui->pane_add_text(mod_ctx,pane,"Choose a design for the next launch. Restart Dusklight after changing it during play.",nullptr);
    UiControlDesc style=UI_CONTROL_DESC_INIT;
    style.kind=UI_CONTROL_BUTTON;
    style.label="PlayStation"; style.on_pressed=select_playstation; style.is_selected=playstation_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&style,nullptr);
    style.label="Xbox"; style.on_pressed=select_xbox; style.is_selected=xbox_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&style,nullptr);
    style.label="Switch"; style.on_pressed=select_switch; style.is_selected=switch_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&style,nullptr);

    svc_ui->pane_add_text(mod_ctx,pane,"Open the CONTROLLER UI tab in-game for visual options.",nullptr);
    if (kDeveloperOptions)
        svc_ui->pane_add_text(mod_ctx,pane,"Developer build: CONTROLLER DEV exposes the full internal calibration editor.",nullptr);
    return MOD_OK;
}

ModResult build_public_general_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Simple presentation options. Button positions and scales use the calibrated layout included with the mod.",nullptr);

    svc_ui->pane_add_section(mod_ctx,pane,"Button Layout Preset");
    svc_ui->pane_add_text(mod_ctx,pane,
        "Choose the visual layout first. This does not change controller inputs until you press the sync button below.",
        nullptr);

    UiControlDesc preset=UI_CONTROL_DESC_INIT;
    preset.kind=UI_CONTROL_BUTTON;
    preset.label="Base";
    if (switch_controller_selected_now()) {
        preset.help_rml="Native Switch B/A/Y/X layout. Additional Switch presets can be added in future versions.";
        preset.on_pressed=select_switch_base_preset;
        preset.is_selected=switch_base_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&preset,nullptr);

        svc_ui->pane_add_text(mod_ctx,pane,
            "Switch currently provides the Base preset only. Its calibration remains separate from PlayStation/Xbox presets.",
            nullptr);
    } else {
        preset.help_rml="Original TP Classic face-button arrangement.";
        preset.on_pressed=select_base_xy_preset;
        preset.is_selected=base_xy_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&preset,nullptr);

        preset.label="Swap X/Y";
        preset.help_rml="Exchange the original GC X and GC Y face assignments.";
        preset.on_pressed=select_swap_xy_preset;
        preset.is_selected=swap_xy_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&preset,nullptr);

        preset.label="Swap Y/B";
        preset.help_rml="Exchange the original GC Y and GC B face assignments.";
        preset.on_pressed=select_swap_yb_preset;
        preset.is_selected=swap_yb_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&preset,nullptr);

        preset.label="Swap X/Y + X/B";
        preset.help_rml="Use the combined X/Y and X/B face assignment preset.";
        preset.on_pressed=select_swap_xyxb_preset;
        preset.is_selected=swap_xyxb_preset_selected;
        svc_ui->pane_add_control(mod_ctx,pane,&preset,nullptr);
    }

    svc_ui->pane_add_section(mod_ctx,pane,"Controller Input Sync");
    svc_ui->pane_add_text(mod_ctx,pane,
        "After choosing a preset, sync only Port 1 gamepad A/B/X/Y through Dusklight's own Controller mapping. Keyboard, triggers, sticks, D-Pad and other bindings are untouched.",
        nullptr);
    add_button(pane,"Sync Controller Inputs with Preset",sync_selected_layout_preset,
        "Apply the selected visual preset to Dusklight's current Port 1 gamepad face-button bindings.");
    svc_ui->pane_add_section(mod_ctx,pane,"Decorations");
    add_toggle(pane,"HUD Ornament",g_hudOrnamentEnabled,"Show or hide the decorative ornament on the gameplay HUD.");
    add_toggle(pane,"Menu Prompt Ornament",g_menuPromptOrnament,"Show or hide the ornament used with shared menu prompts.");
    add_toggle(pane,"Map Ornament",g_mapOrnamentEnabled,"Show or hide the decorative ornament used on map prompts.");

    svc_ui->pane_add_section(mod_ctx,pane,"D-Pad");
    add_toggle(pane,"Orange Direction Arrows",g_dpadArrowsEnabled,"Show or hide the orange D-Pad direction indicators.");
    add_toggle(pane,"D-Pad Shadows",g_dpadShadowsEnabled,"Show or hide the original D-Pad shadow/ring layers.");

    svc_ui->pane_add_section(mod_ctx,pane,"World Map");
    add_toggle(pane,"World Map Arrows",g_worldArrows,"Show or hide the World Map directional arrows.");
    return MOD_OK;
}

ModResult build_public_effects_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Optional prompt effects and animations.",nullptr);

    svc_ui->pane_add_section(mod_ctx,pane,"HUD Effects");
    UiControlDesc hudGlows=UI_CONTROL_DESC_INIT;
    hudGlows.kind=UI_CONTROL_BUTTON;
    hudGlows.label="HUD Glows";
    hudGlows.help_rml="Enable or disable TP Classic prompt glows together. Individual glow controls remain available in developer builds.";
    hudGlows.on_pressed=toggle_public_hud_glows;
    hudGlows.is_selected=public_hud_glows_selected;
    svc_ui->pane_add_control(mod_ctx,pane,&hudGlows,nullptr);

    return MOD_OK;
}

ModResult build_public_tools_panel(ModContext*,UiWindowHandle,UiElementHandle pane,UiElementHandle,void*,ModError*) {
    svc_ui->pane_add_text(mod_ctx,pane,"Reset all controller UI options to the calibrated defaults included with this version.",nullptr);
    add_button(pane,"Restore Default UI",reset_layout,"Restore every TP Classic UI option, including internal layout values, to the current release defaults.");
    return MOD_OK;
}

void on_public_window_closed(ModContext*,UiWindowHandle,void*) {
    g_publicWindow=0;
}
void open_public_window(ModContext*,void*) {
    if(g_publicWindow!=0) return;
    static UiTabDesc tabs[3];
    const char* titles[3] = {"General", "Effects", "Restore"};
    decltype(tabs[0].build) builders[3] = {
        build_public_general_panel,
        build_public_effects_panel,
        build_public_tools_panel,
    };
    for (int i=0;i<3;i++) {
        tabs[i]=UI_TAB_DESC_INIT;
        tabs[i].title=titles[i];
        tabs[i].build=builders[i];
    }
    UiWindowDesc d=UI_WINDOW_DESC_INIT;
    d.tabs=tabs; d.tab_count=3; d.on_closed=on_public_window_closed;
    svc_ui->window_push(mod_ctx,&d,&g_publicWindow);
}

void on_layout_window_closed(ModContext*,UiWindowHandle,void*) {
    g_layoutWindow=0;
}
void open_layout_window(ModContext*,void*) {
    if(g_layoutWindow!=0) return;
    static UiTabDesc tabs[17];
    const bool sw=switch_controller_selected_now();
    const char* normalTitles[17] = {
        "BASE PRESET", "SWAP X/Y PRESET", "SWAP Y/B PRESET", "SWAP X/Y + X/B",
        "Shared HUD", "Shared Items", "HUD Text", "Dialogue", "Shared Wolf",
        "Shops", "Fishing", "Shared Item Wheel", "Menus", "World Map",
        "Dungeon Map", "Save", "Tools"
    };
    decltype(tabs[0].build) normalBuilders[17] = {
        build_base_xy_preset_panel, build_swap_xy_preset_panel,
        build_swap_yb_preset_panel, build_swap_xyxb_preset_panel,
        build_settings_0_panel, build_settings_1_panel, build_settings_2_panel,
        build_dialogue_panel, build_settings_3_panel, build_settings_4_panel,
        build_settings_5_panel, build_settings_6_panel, build_settings_7_panel,
        build_settings_8_panel, build_settings_9_panel, build_settings_10_panel,
        build_settings_11_panel
    };
    const char* switchTitles[14] = {
        "SWITCH PROFILE", "Switch HUD", "Switch Items", "Switch HUD Text",
        "Switch Dialogue", "Switch Wolf", "Switch Shops", "Switch Fishing",
        "Switch Item Wheel", "Switch Menus", "Switch World Map",
        "Switch Dungeon Map", "Switch Save", "Tools"
    };
    decltype(tabs[0].build) switchBuilders[14] = {
        build_base_xy_preset_panel,
        build_settings_0_panel, build_settings_1_panel, build_settings_2_panel,
        build_dialogue_panel, build_settings_3_panel, build_settings_4_panel,
        build_settings_5_panel, build_settings_6_panel, build_settings_7_panel,
        build_settings_8_panel, build_settings_9_panel, build_settings_10_panel,
        build_settings_11_panel
    };
    const int count=sw ? 14 : 17;
    for (int i=0;i<count;i++) {
        tabs[i]=UI_TAB_DESC_INIT;
        tabs[i].title=sw ? switchTitles[i] : normalTitles[i];
        tabs[i].build=sw ? switchBuilders[i] : normalBuilders[i];
    }
    UiWindowDesc d=UI_WINDOW_DESC_INIT;
    d.tabs=tabs; d.tab_count=count; d.on_closed=on_layout_window_closed;
    svc_ui->window_push(mod_ctx,&d,&g_layoutWindow);
}


DEFINE_HOOK(&dMeter2Draw_c::draw, MeterDrawHook);
DEFINE_HOOK(&dMeterButton_c::draw, MeterButtonDrawHook);
DEFINE_HOOK(&dMeterButton_c::screenInitButton, MeterButtonScreenInitHook);
DEFINE_HOOK(&dMeterButton_c::_execute, MeterButtonExecuteHook);
DEFINE_HOOK(&dMeter2Draw_c::drawButtonXY, ButtonXYDrawHook);
DEFINE_HOOK(&dMeter2Draw_c::drawButtonCross, ButtonCrossDrawHook);
DEFINE_HOOK(&CPaneMgr::paneTrans, PaneTransHook);
DEFINE_HOOK(&J2DScreen::draw, ScreenDrawHook);
DEFINE_HOOK(&dMenu_Ring_c::_draw, RingControllerOverlayHook);
DEFINE_HOOK(&dMenu_Collect2D_c::_draw, CollectCompatDrawHook);
DEFINE_HOOK(&dDlst_FileSel_c::draw, FileSelDrawHook);
DEFINE_HOOK(&COutFont_c::createPane, OutFontCreatePaneHook);
DEFINE_HOOK(&COutFont_c::drawFont, OutFontDrawFontHook);

bool s_drawHookInstalled = false;
bool s_drawPreInstalled = false;
bool s_paneTransHookInstalled = false;
dMeter2Draw_c* s_activeMeter = nullptr;
// Ammo digits are rendered outside mpItemXY, using mItemParams[].num_scale.
// Save the vanilla number scale before dMeter2Draw_c::draw(), apply the same
// user scale factor as the corresponding item slot, then restore it in post.
dMeter2Draw_c* s_ammoScaleMeter = nullptr;
float s_ammoNumScaleBase[2] = {1.0f, 1.0f};
float s_ammoNumPosXBase[2] = {0.0f, 0.0f};
float s_ammoNumPosYBase[2] = {0.0f, 0.0f};
bool s_ammoScaleApplied = false;
// Keep the last live meter instance so emphasized-button Pikari draws can be
// identified even though dMeterButton_c is a separate draw-list object.
dMeter2Draw_c* s_meterInstance = nullptr;
dMeterButton_c* s_activeMeterButton = nullptr;
struct MeterButtonGlowState {
    bool valid=false;
    f32 frame[2] = {0.0f, 0.0f};
    u8 button[2] = {dMeterButton_c::BUTTON_NONE_e, dMeterButton_c::BUTTON_NONE_e};
};
MeterButtonGlowState s_meterButtonGlowState;
bool s_logged = false;

ResourceBuffer s_cross = RESOURCE_BUFFER_INIT;
ResourceBuffer s_circle = RESOURCE_BUFFER_INIT;
ResourceBuffer s_square = RESOURCE_BUFFER_INIT;
ResourceBuffer s_triangle = RESOURCE_BUFFER_INIT;

ResourceBuffer s_guide = RESOURCE_BUFFER_INIT;
ResourceBuffer s_r1 = RESOURCE_BUFFER_INIT;
ResourceBuffer s_r1_hud = RESOURCE_BUFFER_INIT;
ResourceBuffer s_analog = RESOURCE_BUFFER_INIT;
ResourceBuffer s_animated_analog_base = RESOURCE_BUFFER_INIT;
ResourceBuffer s_skill_l3 = RESOURCE_BUFFER_INIT;
ResourceBuffer s_shop_l3_right = RESOURCE_BUFFER_INIT;
ResourceBuffer s_r3 = RESOURCE_BUFFER_INIT;
ResourceBuffer s_l2 = RESOURCE_BUFFER_INIT;
ResourceBuffer s_r2 = RESOURCE_BUFFER_INIT;
ResourceBuffer s_options = RESOURCE_BUFFER_INIT;
ResourceBuffer s_menu_ornament = RESOURCE_BUFFER_INIT;
ResourceBuffer s_hud_ornament = RESOURCE_BUFFER_INIT;
ResourceBuffer s_dpad = RESOURCE_BUFFER_INIT;
static bool s_optionBaseDiagLogged = false;

J2DPane* child_at(J2DPane* root, int wantedIndex) {
    if (root == nullptr || wantedIndex < 0) return nullptr;
    int index = 0;
    for (J2DPane* child = root->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane(), ++index) {
        if (index == wantedIndex) return child;
    }
    return nullptr;
}

J2DPicture* as_picture(J2DPane* pane) {
    if (pane == nullptr || pane->getTypeID() != 18) return nullptr;
    return static_cast<J2DPicture*>(pane);
}

struct PictureTexCoordAccess : J2DPicture {
    using Member = JGeometry::TVec2<s16> (J2DPicture::*)[4];
    using TextureMember = JUTTexture* (J2DPicture::*)[2];
    using TextureNumMember = u8 J2DPicture::*;
    static Member member() { return &PictureTexCoordAccess::field_0x10a; }
    static TextureMember texture_member() { return &PictureTexCoordAccess::mTexture; }
    static TextureNumMember texture_num_member() { return &PictureTexCoordAccess::mTextureNum; }
};

JUTTexture*& picture_texture_slot(J2DPicture* pic, int index) {
    const auto member = PictureTexCoordAccess::texture_member();
    return (pic->*member)[index];
}

u8& picture_texture_count(J2DPicture* pic) {
    const auto member = PictureTexCoordAccess::texture_num_member();
    return pic->*member;
}

void copy_picture_texcoords(J2DPicture* pic, JGeometry::TVec2<s16> out[4]) {
    if (pic == nullptr) return;
    const auto member = PictureTexCoordAccess::member();
    for (int i = 0; i < 4; ++i) out[i] = (pic->*member)[i];
}

void restore_picture_texcoords(J2DPicture* pic, const JGeometry::TVec2<s16> in[4]) {
    if (pic == nullptr) return;
    const auto member = PictureTexCoordAccess::member();
    for (int i = 0; i < 4; ++i) (pic->*member)[i] = in[i];
}

J2DPicture* picture_child(J2DPane* root, int index) {
    return as_picture(child_at(root, index));
}

void set_bounds(J2DPane* pane, float x, float y, float w, float h) {
    if (pane == nullptr) return;
    pane->move(x, y);
    pane->resize(w, h);
}

// Internal resource slots retain their historical PlayStation-oriented names.
// Switch resources are semantic instead: A stays A, B stays B, X stays X and
// Y stays Y. The switch_* calibration owns the Nintendo physical placement;
// no PS/Xbox face swap is used to reinterpret the Switch artwork.
struct ControllerTexture {
    ResourceBuffer* playstation;
    const char* xboxPath;
    ResourceBuffer xbox;
    const char* switchPath;
    ResourceBuffer switchTexture;
};
ControllerTexture s_controllerTextures[]={
    {&s_cross, "xbox/cross.bti", RESOURCE_BUFFER_INIT, "switch/a.bti", RESOURCE_BUFFER_INIT},
    {&s_circle, "xbox/circle.bti", RESOURCE_BUFFER_INIT, "switch/b.bti", RESOURCE_BUFFER_INIT},
    {&s_square, "xbox/square.bti", RESOURCE_BUFFER_INIT, "switch/y.bti", RESOURCE_BUFFER_INIT},
    {&s_triangle, "xbox/triangle.bti", RESOURCE_BUFFER_INIT, "switch/x.bti", RESOURCE_BUFFER_INIT},
    {&s_r1, "xbox/r1.bti", RESOURCE_BUFFER_INIT, "switch/r.bti", RESOURCE_BUFFER_INIT},
    {&s_r1_hud, "xbox/r1_hud.bti", RESOURCE_BUFFER_INIT, "switch/r.bti", RESOURCE_BUFFER_INIT},
    {&s_analog, "xbox/l3.bti", RESOURCE_BUFFER_INIT, "switch/l_stick.bti", RESOURCE_BUFFER_INIT},
    {&s_animated_analog_base, "xbox/animated_analog_base.bti", RESOURCE_BUFFER_INIT, "xbox/animated_analog_base.bti", RESOURCE_BUFFER_INIT},
    {&s_skill_l3, "xbox/skill_l3.bti", RESOURCE_BUFFER_INIT, "xbox/skill_l3.bti", RESOURCE_BUFFER_INIT},
    {&s_shop_l3_right, "xbox/shop_l3_right.bti", RESOURCE_BUFFER_INIT, "xbox/shop_l3_right.bti", RESOURCE_BUFFER_INIT},
    {&s_r3, "xbox/r3.bti", RESOURCE_BUFFER_INIT, "switch/r_stick.bti", RESOURCE_BUFFER_INIT},
    {&s_l2, "xbox/l2.bti", RESOURCE_BUFFER_INIT, "switch/zl.bti", RESOURCE_BUFFER_INIT},
    {&s_r2, "xbox/r2.bti", RESOURCE_BUFFER_INIT, "switch/zr.bti", RESOURCE_BUFFER_INIT},
    {&s_options, "xbox/options.bti", RESOURCE_BUFFER_INIT, "switch/plus.bti", RESOURCE_BUFFER_INIT},
    {&s_dpad, "xbox/dpad.bti", RESOURCE_BUFFER_INIT, "switch/dpad.bti", RESOURCE_BUFFER_INIT}
};

const ResTIMG* resource_timg(const ResourceBuffer& requested) {
    // Latch once, before any replacement texture can be attached to a game pane.
    // All controller packs remain allocated; no live reload or pointer invalidation occurs.
    if (!s_controllerStyleLocked) {
        const int64_t style=raw_controller_style();
        s_useXbox=style==1;
        s_useSwitch=style==2;
        s_controllerStyleLocked=true;
        if(svc_log) {
            const char* name=s_useSwitch ? "Switch" : (s_useXbox ? "Xbox" : "PlayStation");
            std::string msg="Controller design locked: ";
            msg+=name;
            svc_log->info(mod_ctx,msg.c_str());
        }
    }
    const ResourceBuffer* selected=&requested;
    if (s_useSwitch) {
        for(auto& texture:s_controllerTextures)
            if(texture.playstation==&requested) { selected=&texture.switchTexture; break; }
    } else if (s_useXbox) {
        for(auto& texture:s_controllerTextures)
            if(texture.playstation==&requested) { selected=&texture.xbox; break; }
    }
    const ResourceBuffer& buffer=*selected;
    if (buffer.data == nullptr || buffer.size < 0x20) return nullptr;
    return reinterpret_cast<const ResTIMG*>(buffer.data);
}

// Faces are resolved by original GameCube function slot. The game input/function
// stays attached to GC B/X/Y; presets only choose which modern physical face is
// drawn for that function. This keeps sword/item/dialogue behavior untouched.
const ResTIMG* gc_b_face_texture() {
    switch(current_layout_preset()) {
    case ButtonLayoutPreset::SwapYB:
    case ButtonLayoutPreset::SwapXYXB:
        return resource_timg(s_square);
    default:
        return resource_timg(s_circle);
    }
}
const ResTIMG* gc_x_face_texture() {
    switch(current_layout_preset()) {
    case ButtonLayoutPreset::SwapXY:   return resource_timg(s_square);
    // Swap X/Y starts with Square on GC X, then X/B exchanges that Square
    // with the original GC B face. GC X therefore becomes Circle.
    case ButtonLayoutPreset::SwapXYXB: return resource_timg(s_circle);
    default:                            return resource_timg(s_triangle);
    }
}
const ResTIMG* gc_y_face_texture() {
    switch(current_layout_preset()) {
    case ButtonLayoutPreset::SwapXY:
    case ButtonLayoutPreset::SwapXYXB:
        return resource_timg(s_triangle);
    // Base has Square on GC Y. Y/B exchanges that Square with GC B,
    // therefore GC Y becomes Circle while GC B becomes Square.
    case ButtonLayoutPreset::SwapYB:
        return resource_timg(s_circle);
    default:
        return resource_timg(s_square);
    }
}

// Twilit Essentials does not currently expose a public "is mod enabled" API/service.
// Detect ownership from the live HUD instead. screenInitButton gives us the clean
// vanilla Z texture before either mod changes the contextual prompt; Essentials then
// updates zbtn/z_btnl from dMeterButton_c::_execute.
J2DScreen* s_midnaPromptScreen = nullptr;
const ResTIMG* s_midnaPromptOriginalTexture = nullptr;
bool s_externalMidnaPromptOwner = false;

bool twilit_midna_layout_active(dMeter2Draw_c* meter) {
    if (meter == nullptr || meter->getMainScreenPtr() == nullptr) return false;

    // Contextual Z/Midna prompt ownership is independent from the main HUD
    // portrait. Only suppress TP Classic's portrait transform when Essentials
    // actually reparents the main midona_n root under juji_n.
    J2DScreen* screen = meter->getMainScreenPtr();
    J2DPane* midna = screen->search(MULTI_CHAR('midona_n'));
    J2DPane* juji = screen->search(MULTI_CHAR('juji_n'));
    return midna != nullptr && juji != nullptr && midna->getParentPane() == juji;
}

bool load_button_texture(const char* path, ResourceBuffer* out) {
    if (svc_resource == nullptr || out == nullptr) return false;
    return svc_resource->load(mod_ctx, path, out) == MOD_OK &&
           out->data != nullptr && out->size >= 0x20;
}

void replace_picture_texture(J2DPicture* picture, const ResTIMG* texture) {
    if (picture == nullptr || texture == nullptr) return;
    const u8 count = picture->getTextureCount();
    for (u8 i = 0; i < count; ++i) picture->changeTexture(texture, i);
    if (picture->getTextureCount() != 0 && picture->getTexture(0) != nullptr)
        picture->setTexCoord(picture->getTexture(0), BIND15, MIRROR0, false);
}

void apply_picture_flip(J2DPicture* picture, bool flipH, bool flipV) {
    if (picture == nullptr) return;

    // The item J2DPicture children are created with J2DBasePosition_4 (center).
    // Mirror the child itself around that center. This avoids changing the
    // mpItemB/mpItemXY parent used by the solved X/Y/Scale layout code.
    const float sx = picture->getScaleX();
    const float sy = picture->getScaleY();
    const float magX = sx < 0.0f ? -sx : sx;
    const float magY = sy < 0.0f ? -sy : sy;
    picture->scale(flipH ? -magX : magX, flipV ? -magY : magY);
}

// Usa somente a primeira picture de cada grupo para desenhar o botao completo.
// Isso evita depender das estruturas internas diferentes de A/B/X/Y.
void apply_full_button(J2DPane* root, const ResTIMG* texture) {
    if (root == nullptr || texture == nullptr) return;

    J2DPicture* face = picture_child(root, 0);
    if (face == nullptr) return;

    replace_picture_texture(face, texture);

    // IMPORTANTE: A/B/X/Y vanilla usam modulacoes de cor diferentes.
    // Se apenas trocarmos a textura, o Cross azul herda o verde do A,
    // o Circle herda o vermelho forte do B etc. Neutralizamos a TEV
    // black/white e os quatro corner colors para exibir a BTI sem tint.
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    face->setBlackWhite(neutralBlack, neutralWhite);
    face->setCornerColor(neutralWhite);

    face->show();
    set_bounds(face, 0.0f, 0.0f, 24.0f, 24.0f);
    face->rotate(12.0f, 12.0f, ROTATE_Z, 0.0f);

    // Desliga todas as camadas vanilla restantes: letra, brilho e overlays.
    for (int i = 1; i < 6; ++i) {
        if (J2DPane* extra = child_at(root, i)) extra->hide();
    }

    // XY0 possui uma picture aninhada dentro da primeira picture.
    // Ela tambem carrega material/transform vanilla e deve permanecer oculta.
    if (J2DPane* nested = child_at(face, 0)) nested->hide();
}


struct DpadPaneState {
    J2DPane* pane = nullptr;
    JGeometry::TBox2<f32> base{};
    float baseScaleX = 1.0f, baseScaleY = 1.0f;
    float lastDx = 0.0f, lastDy = 0.0f, lastScale = 1.0f;
    bool captured = false;
};
DpadPaneState s_dpadRings[4]{};
DpadPaneState s_dpadArrows[4]{};
DpadPaneState s_dpadTexts[2]{};
DpadPaneState s_dpadAnimRoot{};

static void transform_group_pane(J2DPane* pane, DpadPaneState& st,
                                 float dx, float dy, float sc,
                                 float pivotX, float pivotY) {
    if (pane == nullptr) return;
    const auto& cur = pane->getBounds();
    if (!st.captured || st.pane != pane) {
        st.pane = pane; st.base = cur; st.lastDx = st.lastDy = 0.0f;
        st.lastScale = 1.0f; st.captured = true;
    }
    const float bx = st.base.i.x, by = st.base.i.y;
    const float bw = st.base.getWidth(), bh = st.base.getHeight();
    pane->move(pivotX + (bx - pivotX) * sc + dx,
               pivotY + (by - pivotY) * sc + dy);
    pane->resize(bw * sc, bh * sc);
    st.lastDx = dx; st.lastDy = dy; st.lastScale = sc;
}

static void transform_text_pane(J2DPane* pane, DpadPaneState& st,
                                float dx, float dy, float sc) {
    if (pane == nullptr) return;
    const auto& cur = pane->getBounds();
    if (!st.captured || st.pane != pane) {
        st.pane = pane;
        st.base = cur;
        st.baseScaleX = pane->getScaleX();
        st.baseScaleY = pane->getScaleY();
        st.lastDx = st.lastDy = 0.0f;
        st.lastScale = 1.0f;
        st.captured = true;
    } else {
        // The game may animate the label position itself.  Remove only our previous
        // offset when deciding whether vanilla moved it; never treat our scale as a
        // bounds resize.  J2DPane::scale changes the render transform, not its box.
        const float expectedX = st.base.i.x + st.lastDx;
        const float expectedY = st.base.i.y + st.lastDy;
        if (std::fabs(cur.i.x - expectedX) > 0.25f ||
            std::fabs(cur.i.y - expectedY) > 0.25f) {
            st.base = cur;
            st.base.i.x -= st.lastDx;
            st.base.i.y -= st.lastDy;
            st.base.f.x -= st.lastDx;
            st.base.f.y -= st.lastDy;
        }
        const float expectedSX = st.baseScaleX * st.lastScale;
        const float expectedSY = st.baseScaleY * st.lastScale;
        const float csx = pane->getScaleX(), csy = pane->getScaleY();
        if (std::fabs(csx - expectedSX) > 0.01f || std::fabs(csy - expectedSY) > 0.01f) {
            // Preserve a vanilla scale change if one occurs.
            st.baseScaleX = csx / (st.lastScale == 0.0f ? 1.0f : st.lastScale);
            st.baseScaleY = csy / (st.lastScale == 0.0f ? 1.0f : st.lastScale);
        }
    }
    pane->move(st.base.i.x + dx, st.base.i.y + dy);
    pane->scale(st.baseScaleX * sc, st.baseScaleY * sc);
    st.lastDx = dx; st.lastDy = dy; st.lastScale = sc;
}


struct TextChildScaleState {
    J2DPane* pane = nullptr;
    float sx = 1.0f, sy = 1.0f;
};
static TextChildScaleState s_itemTextChildren[24]{};
static TextChildScaleState s_mapTextChildren[24]{};

static void scale_text_descendants(J2DPane* root, TextChildScaleState* states, int& used, float factor) {
    if (root == nullptr || used >= 24) return;
    for (J2DPane* c = root->getFirstChildPane(); c != nullptr && used < 24; c = c->getNextChildPane()) {
        // The visible ITEM/MAP glyphs live in descendants (cont_ju*), not in the
        // i_text_n/m_text_n containers. Scale each actual child pane directly.
        TextChildScaleState& st = states[used++];
        if (st.pane != c) {
            st.pane = c;
            st.sx = c->getScaleX();
            st.sy = c->getScaleY();
        }
        c->scale(st.sx * factor, st.sy * factor);
        scale_text_descendants(c, states, used, factor);
    }
}

static void apply_map_rise_toggle(J2DPane* root, bool enabled) {
    if (root == nullptr) return;
    auto& st=s_dpadAnimRoot;
    if (!st.captured || st.pane != root) {
        st.pane=root; st.base=root->getBounds(); st.captured=true;
    }
    if (!enabled) {
        root->move(st.base.i.x, st.base.i.y);
        root->resize(st.base.getWidth(), st.base.getHeight());
    }
}

struct DpadDiagState {
    J2DPane* pane = nullptr;
    float x=0,y=0,w=0,h=0,sx=0,sy=0;
    bool valid=false;
};
DpadDiagState s_dpadDiag[8]{};
static void diagnose_dpad_map_motion(J2DScreen* screen) {
    if (screen == nullptr || svc_log == nullptr) return;
    static const u64 tags[8] = {
        MULTI_CHAR('..juji_n'), MULTI_CHAR('..ju_p_n'), MULTI_CHAR('i_text_n'), MULTI_CHAR('m_text_n'),
        MULTI_CHAR('ju_ring1'), MULTI_CHAR('yaji_00'), MULTI_CHAR('juji_001'), MULTI_CHAR('cont_ju0')
    };
    static const char* names[8] = {"..juji_n","..ju_p_n","i_text_n","m_text_n","ju_ring1","yaji_00","juji_001","cont_ju0"};
    for (int i=0;i<8;++i) {
        J2DPane* p=screen->search(tags[i]);
        if (!p) continue;
        const auto& b=p->getBounds();
        const float x=b.i.x,y=b.i.y,w=b.getWidth(),h=b.getHeight(),sx=p->getScaleX(),sy=p->getScaleY();
        auto& d=s_dpadDiag[i];
        if (!d.valid || d.pane!=p) {
            d={p,x,y,w,h,sx,sy,true};
            char line[256]; std::snprintf(line,sizeof(line),"[DPadAnimDiag] BASE %-9s x=%.2f y=%.2f w=%.2f h=%.2f sx=%.3f sy=%.3f",names[i],x,y,w,h,sx,sy);
            svc_log->info(mod_ctx,line);
        } else if (std::fabs(x-d.x)>0.20f || std::fabs(y-d.y)>0.20f || std::fabs(w-d.w)>0.20f || std::fabs(h-d.h)>0.20f || std::fabs(sx-d.sx)>0.01f || std::fabs(sy-d.sy)>0.01f) {
            char line[320]; std::snprintf(line,sizeof(line),"[DPadAnimDiag] CHANGE %-9s x %.2f->%.2f y %.2f->%.2f w %.2f->%.2f h %.2f->%.2f sx %.3f->%.3f sy %.3f->%.3f",names[i],d.x,x,d.y,y,d.w,w,d.h,h,d.sx,sx,d.sy,sy);
            svc_log->info(mod_ctx,line);
            d.x=x;d.y=y;d.w=w;d.h=h;d.sx=sx;d.sy=sy;
        }
    }
}

HookAction before_button_cross_draw(ModContext*, void* args, void*, void*) {
    if (args != nullptr && !cfg_bool(g_dpadMapAnimation, true)) {
        // drawButtonCross() is the single point where moveButtonCross() applies the
        // animated map position to the common D-Pad parent.  Override only its
        // position arguments; do not touch child bounds, user scale, rings or arrows.
        mods::arg_ref<f32>(args, 1) = g_drawHIO.mButtonCrossOFFPosX;
        mods::arg_ref<f32>(args, 2) = g_drawHIO.mButtonCrossOFFPosY;
    }
    return HOOK_CONTINUE;
}

// Draw-local transform: sliders update live without feeding modified values
// back into the game's event-driven Z label placement.
J2DPane* s_fishingCheckPane = nullptr;
float s_fishingCheckBaseX=0, s_fishingCheckBaseY=0;
float s_fishingCheckBaseSX=1, s_fishingCheckBaseSY=1;
HookAction before_meter_draw(ModContext*, void* args, void*, void*) {
    s_activeMeter = args != nullptr ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;
    if (s_activeMeter != nullptr) s_meterInstance = s_activeMeter;
    s_fishingCheckPane = nullptr;
    if (s_activeMeter != nullptr && s_activeMeter->getCanoeFishing() &&
        s_activeMeter->mpTextXY[2] != nullptr) {
        J2DPane* pane = s_activeMeter->mpTextXY[2]->getPanePtr();
        if (pane != nullptr) {
            s_fishingCheckPane = pane;
            s_fishingCheckBaseX = pane->getTranslateX();
            s_fishingCheckBaseY = pane->getTranslateY();
            s_fishingCheckBaseSX = pane->getScaleX();
            s_fishingCheckBaseSY = pane->getScaleY();
            const float scale = cfg_scale(g_fishingCheckScale, 1.0f);
            pane->translate(s_fishingCheckBaseX + cfg_pos(g_fishingCheckX, 0.0f),
                            s_fishingCheckBaseY + cfg_pos(g_fishingCheckY, 0.0f));
            pane->scale(s_fishingCheckBaseSX * scale, s_fishingCheckBaseSY * scale);
        }
    }

    // v0.11.00: replace the four-piece vanilla white D-Pad with one supplied
    // PlayStation-style texture. Rings/shadows and orange arrows stay independent.
    if (s_activeMeter != nullptr && s_activeMeter->mpScreen != nullptr) {
        J2DPane* screen = s_activeMeter->mpScreen;
        const bool shadows = cfg_bool(g_dpadShadowsEnabled, true);
        const bool arrows = cfg_bool(g_dpadArrowsEnabled, true);
        static const u64 ringTags[] = {
            MULTI_CHAR('ju_ring1'), MULTI_CHAR('ju_ring2'),
            MULTI_CHAR('ju_ring3'), MULTI_CHAR('ju_ring4')
        };
        static const u64 arrowTags[] = {
            MULTI_CHAR('yaji_00'), MULTI_CHAR('yaji_01'),
            MULTI_CHAR('yaji_02'), MULTI_CHAR('yaji_03')
        };
        const float dpadDx = cfg_pos(g_dpadX, 0.0f);
        const float dpadDy = cfg_pos(g_dpadY, 0.0f);
        const float dpadScale = cfg_scale(g_dpadScale, 1.0f);

        // Rings are children of ..juji_n. The visual D-Pad center in that coordinate
        // space is (31.9,24.5): ..ju_p_n origin (8.5,12.0) + face center (23.4,12.5).
        for (int i=0;i<4;++i) {
            if (J2DPane* pane = screen->search(ringTags[i])) {
                transform_group_pane(pane,s_dpadRings[i],dpadDx,dpadDy,dpadScale,31.9f,24.5f);
                if (shadows) pane->show(); else pane->hide();
            }
        }
        // Arrows share the same parent as the replacement face, so use its local center.
        for (int i=0;i<4;++i) {
            if (J2DPane* pane = screen->search(arrowTags[i])) {
                transform_group_pane(pane,s_dpadArrows[i],dpadDx,dpadDy,dpadScale,23.4f,12.5f);
                if (arrows) pane->show(); else pane->hide();
            }
        }

        // ITEM and MAP labels get independent offsets/scales while retaining vanilla motion.
        J2DPane* itemTextRoot = screen->search(MULTI_CHAR('i_text_n'));
        J2DPane* mapTextRoot  = screen->search(MULTI_CHAR('m_text_n'));
        transform_text_pane(itemTextRoot,s_dpadTexts[0],
                            cfg_pos(g_itemTextX,0.0f),cfg_pos(g_itemTextY,0.0f),1.0f);
        transform_text_pane(mapTextRoot,s_dpadTexts[1],
                            cfg_pos(g_mapTextX,0.0f),cfg_pos(g_mapTextY,0.0f),1.0f);
        int itemUsed=0, mapUsed=0;
        scale_text_descendants(itemTextRoot,s_itemTextChildren,itemUsed,cfg_scale(g_itemTextScale,1.0f));
        scale_text_descendants(mapTextRoot,s_mapTextChildren,mapUsed,cfg_scale(g_mapTextScale,1.0f));

        J2DPicture* dpadFace = as_picture(screen->search(MULTI_CHAR('juji_001')));
        if (dpadFace != nullptr) {
            replace_picture_texture(dpadFace, resource_timg(s_dpad));
            const JUtility::TColor neutralBlack(0,0,0,0);
            const JUtility::TColor neutralWhite(255,255,255,255);
            dpadFace->setBlackWhite(neutralBlack, neutralWhite);
            dpadFace->setCornerColor(neutralWhite);
            dpadFace->show();
            // juji_001 is the top-left quarter of the original 22x22 white base.
            // Expand this one host over the full original white D-Pad footprint.
            const float baseW = 22.8f, baseH = 22.0f;
            const float w = baseW * dpadScale, h = baseH * dpadScale;
            const float x = 12.0f + dpadDx + (baseW - w) * 0.5f;
            const float y = 1.5f + dpadDy + (baseH - h) * 0.5f;
            set_bounds(dpadFace, x, y, w, h);
        }
        static const u64 oldWhiteTags[] = {
            MULTI_CHAR('juji_002'), MULTI_CHAR('juji_003'), MULTI_CHAR('juji_004')
        };
        for (u64 tag : oldWhiteTags) if (J2DPane* pane = screen->search(tag)) pane->hide();
    }

    // Item quantity/ammo is not a child of mpItemXY. Vanilla draws the digits
    // manually from mItemParams[i].num_scale, so scaling the item pane alone
    // only changes where the number appears. Scale num_scale explicitly for
    // the duration of this draw call.
    s_ammoScaleApplied = false;
    s_ammoScaleMeter = nullptr;
    if (s_activeMeter != nullptr) {
        s_ammoScaleMeter = s_activeMeter;
        const float itemScale[2] = {
            cfg_scale(g_itemSquareScale, 1.0f),
            cfg_scale(g_itemTriangleScale, 1.0f),
        };
        for (int i = 0; i < 2; ++i) {
            s_ammoNumScaleBase[i] = s_activeMeter->mItemParams[i].num_scale;
            s_ammoNumPosXBase[i] = s_activeMeter->mItemParams[i].num_pos_x;
            s_ammoNumPosYBase[i] = s_activeMeter->mItemParams[i].num_pos_y;

            // Vanilla draws ammo manually around the item's center, but its Y
            // anchor uses the unscaled pane height. Scale the complete local
            // offset from the item center so the digits shrink toward/away from
            // the item coherently instead of sliding as Item Scale changes.
            s_activeMeter->mItemParams[i].num_scale = s_ammoNumScaleBase[i] * itemScale[i];
            s_activeMeter->mItemParams[i].num_pos_x = s_ammoNumPosXBase[i] * itemScale[i];
            if (s_activeMeter->mpItemXY[i] != nullptr) {
                const float itemHeight = s_activeMeter->mpItemXY[i]->getSizeY();
                s_activeMeter->mItemParams[i].num_pos_y =
                    (itemHeight + s_ammoNumPosYBase[i]) * itemScale[i] - itemHeight;
            } else {
                s_activeMeter->mItemParams[i].num_pos_y = s_ammoNumPosYBase[i] * itemScale[i];
            }
        }
        s_ammoScaleApplied = true;
    }

    if (s_activeMeter != nullptr) {
        const bool preview = cfg_bool(g_glowPreview,false);

        const ConfigVarHandle actionGlowEnabled = switch_variant(g_actionGlowEnabled);
        if (!cfg_bool(actionGlowEnabled,true)) {
            s_activeMeter->field_0x608 = 0.0f;
        } else if (preview) {
            s_activeMeter->field_0x608 = 18.0f;
        }

        const ConfigVarHandle backGlowEnabled =
            layout_handle4(g_backGlowEnabled,g_backGlowEnabled,g_ybBackGlowEnabled,g_xyxbBackGlowEnabled);
        if (!cfg_bool(backGlowEnabled,true)) {
            s_activeMeter->field_0x60c = 0.0f;
        } else if (preview) {
            s_activeMeter->field_0x60c = 18.0f;
        }

        // Wolf contextual X/Y glows use dMeter2Draw_c::field_0x620[] and are
        // rendered by drawPikari(mpBTextXY[i], ...). This is separate from
        // x_light/y_light and from dMeterButton_c's emphasis Pikari.
        const bool wolfPreview = cfg_bool(g_wolfGlowPreview,false);
        const ConfigVarHandle wolfXEnabled = layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled);
        const ConfigVarHandle wolfYEnabled = layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled);
        if (!cfg_bool(wolfXEnabled,true)) {
            s_activeMeter->field_0x620[0] = 0.0f;
        } else if (wolfPreview) {
            s_activeMeter->field_0x620[0] = 18.0f;
        }
        if (!cfg_bool(wolfYEnabled,true)) {
            s_activeMeter->field_0x620[1] = 0.0f;
        } else if (wolfPreview) {
            s_activeMeter->field_0x620[1] = 18.0f;
        }
    }

    return HOOK_CONTINUE;
}

// The game's Action/Back Pikari is anchored to the center of the hidden
// b_text_a / b_text_b panes. Instead of hooking the overloaded drawPikari
// function directly (which leaves an unresolved MSVC symbol in this SDK),
// adjust those anchor panes here. This preserves the vanilla renderer,
// animation timing and colors, while still giving us independent X/Y control.
//
// Scale is handled by changing the anchor pane scale. The Pikari renderer
// itself uses the HIO scale, so this does not multiply the Pikari geometry;
// for this test build, X/Y and enable/disable are the verified controls.
// Scale remains in the UI but is applied conservatively through the anchor.

static void tag_to_text(u64 tag, char out[9]);

HookAction before_pane_trans(ModContext*, void* args, void*, void*) {
    if (args == nullptr || s_activeMeter == nullptr) return HOOK_CONTINUE;
    auto* mgr = mods::arg<CPaneMgr*>(args, 0);
    if (mgr == nullptr) return HOOK_CONTINUE;

    // Sword: restore the verified v0.10.1/v0.9.25 mpItemB path.
    // drawButtonB() refreshes the vanilla transform immediately before paneTrans(),
    // so these offsets remain relative to the current vanilla sword position.
    if (mgr == s_activeMeter->mpItemB) {
        mods::arg_ref<f32>(args, 1) += (float)cfg_int(layout_handle4(g_swordX,g_swordX,g_ybSwordX,g_xyxbSwordX),0);
        mods::arg_ref<f32>(args, 2) += (float)cfg_int(layout_handle4(g_swordY,g_swordY,g_ybSwordY,g_xyxbSwordY),0);

        J2DPane* pane = mgr->getPanePtr();
        if (pane != nullptr) {
            const float factor=(float)cfg_int(layout_handle4(g_swordScale,g_swordScale,g_ybSwordScale,g_xyxbSwordScale),100)/100.0f;
            const bool flipH=cfg_bool(layout_handle4(g_swordFlipH,g_swordFlipH,g_ybSwordFlipH,g_xyxbSwordFlipH),false);
            const bool flipV=cfg_bool(layout_handle4(g_swordFlipV,g_swordFlipV,g_ybSwordFlipV,g_xyxbSwordFlipV),false);
            const float flipX=flipH ? -1.0f : 1.0f;
            const float flipY=flipV ? -1.0f : 1.0f;

            const float vanillaSX=pane->getScaleX();
            const float vanillaSY=pane->getScaleY();
            const float vanillaRot=g_drawHIO.mButtonBItemRotation[1];

            pane->scale(vanillaSX*factor*flipX,
                        vanillaSY*factor*flipY);

            const float targetRot=(flipH != flipV) ? -vanillaRot : vanillaRot;
            pane->rotate(mgr->getSizeX()*0.5f, mgr->getSizeY()*0.5f,
                         ROTATE_Z, targetRot);
        }
        return HOOK_CONTINUE;
    }

    // Grass whistle / hawk-call HUD owns its own preset-local text layout.
    // Use the same player state checks as the vanilla meter instead of matching
    // localized strings such as "Assoprar" / "Voltar".
    {
        daPy_py_c* player=daPy_getPlayerActorClass();
        const bool whistleContext =
            player != nullptr && (player->checkGrassWhistle() || player->checkHawkWait());
        if (whistleContext && (mgr == s_activeMeter->mpTextA || mgr == s_activeMeter->mpTextB)) {
            const bool action = mgr == s_activeMeter->mpTextA;
            const ConfigVarHandle xh = action
                ? layout_handle4(g_whistleActionX,g_swapWhistleActionX,g_ybWhistleActionX,g_xyxbWhistleActionX)
                : layout_handle4(g_whistleBackX,g_swapWhistleBackX,g_ybWhistleBackX,g_xyxbWhistleBackX);
            const ConfigVarHandle yh = action
                ? layout_handle4(g_whistleActionY,g_swapWhistleActionY,g_ybWhistleActionY,g_xyxbWhistleActionY)
                : layout_handle4(g_whistleBackY,g_swapWhistleBackY,g_ybWhistleBackY,g_xyxbWhistleBackY);
            const ConfigVarHandle sh = action
                ? layout_handle4(g_whistleActionScale,g_swapWhistleActionScale,g_ybWhistleActionScale,g_xyxbWhistleActionScale)
                : layout_handle4(g_whistleBackScale,g_swapWhistleBackScale,g_ybWhistleBackScale,g_xyxbWhistleBackScale);
            mods::arg_ref<f32>(args,1) += cfg_pos(xh,0.0f);
            mods::arg_ref<f32>(args,2) += cfg_pos(yh,0.0f);
            if (J2DPane* pane=mgr->getPanePtr()) {
                const float factor=cfg_scale(sh,1.0f);
                pane->scale(pane->getScaleX()*factor,pane->getScaleY()*factor);
            }
            return HOOK_CONTINUE;
        }
    }

    // Howling owns separate settings: bypass general action/dialogue/back
    // offsets and the Back position lock instead of stacking on top of them.
    if (dMsgObject_getMsgObjectClass() != nullptr &&
        dMsgObject_getMsgObjectClass()->isHowlMessage() &&
        (mgr == s_activeMeter->mpTextA || mgr == s_activeMeter->mpTextB)) {
        const bool action = mgr == s_activeMeter->mpTextA;
        const ConfigVarHandle xh = action
            ? layout_handle4(g_howlActionX,g_swapHowlActionX,g_ybHowlActionX,g_xyxbHowlActionX)
            : layout_handle4(g_howlBackX,g_swapHowlBackX,g_ybHowlBackX,g_xyxbHowlBackX);
        const ConfigVarHandle yh = action
            ? layout_handle4(g_howlActionY,g_swapHowlActionY,g_ybHowlActionY,g_xyxbHowlActionY)
            : layout_handle4(g_howlBackY,g_swapHowlBackY,g_ybHowlBackY,g_xyxbHowlBackY);
        const ConfigVarHandle sh = action
            ? layout_handle4(g_howlActionScale,g_swapHowlActionScale,g_ybHowlActionScale,g_xyxbHowlActionScale)
            : layout_handle4(g_howlBackScale,g_swapHowlBackScale,g_ybHowlBackScale,g_xyxbHowlBackScale);
        mods::arg_ref<f32>(args,1) += cfg_pos(xh,0.0f);
        mods::arg_ref<f32>(args,2) += cfg_pos(yh,0.0f);
        if (J2DPane* pane = mgr->getPanePtr()) {
            const float scale = cfg_scale(sh,1.0f);
            pane->scale(pane->getScaleX()*scale,pane->getScaleY()*scale);
        }
        return HOOK_CONTINUE;
    }
    // Shops own separate settings: bypass general action/dialogue/back
    // offsets and the Back position lock instead of stacking on top of them.
    if (dMeter2Info_isShopTalkFlag() &&
        (mgr == s_activeMeter->mpTextA || mgr == s_activeMeter->mpTextB)) {
        const bool action = mgr == s_activeMeter->mpTextA;
        const ConfigVarHandle xh = action
            ? layout_handle4(g_shopActionX,g_swapShopActionX,g_ybShopActionX,g_xyxbShopActionX)
            : layout_handle4(g_shopBackX,g_swapShopBackX,g_ybShopBackX,g_xyxbShopBackX);
        const ConfigVarHandle yh = action
            ? layout_handle4(g_shopActionY,g_swapShopActionY,g_ybShopActionY,g_xyxbShopActionY)
            : layout_handle4(g_shopBackY,g_swapShopBackY,g_ybShopBackY,g_xyxbShopBackY);
        const ConfigVarHandle sh = action
            ? layout_handle4(g_shopActionScale,g_swapShopActionScale,g_ybShopActionScale,g_xyxbShopActionScale)
            : layout_handle4(g_shopBackScale,g_swapShopBackScale,g_ybShopBackScale,g_xyxbShopBackScale);
        mods::arg_ref<f32>(args,1) += cfg_pos(xh,0.0f);
        mods::arg_ref<f32>(args,2) += cfg_pos(yh,0.0f);
        if (J2DPane* pane = mgr->getPanePtr()) {
            const float scale = cfg_scale(sh,1.0f);
            pane->scale(pane->getScaleX()*scale,pane->getScaleY()*scale);
        }
        return HOOK_CONTINUE;
    }
    if (mgr == s_activeMeter->mpTextA) {
        // O HUD vanilla reposiciona o texto de acao durante draw().
        // Ajustamos os argumentos da propria paneTrans para que o offset seja
        // aplicado no momento correto e continue relativo a qualquer texto/estado vanilla.
        mods::arg_ref<f32>(args, 1) += cfg_pos(g_actionTextX,0.0f);
        mods::arg_ref<f32>(args, 2) += cfg_pos(g_actionTextY,0.0f);

        // Use the same talk-state test used by dMeter2_c::moveButtonA() itself.
        // This distinguishes the dialogue presentation of mpTextA (e.g. "Seguinte")
        // from its normal HUD actions without moving the Cross button.
        if (dMsgObject_isTalkNowCheck()) {
            mods::arg_ref<f32>(args, 1) += cfg_pos(
                layout_handle4(g_dialogActionTextX,g_swapDialogActionTextX,g_ybDialogActionTextX,g_xyxbDialogActionTextX),0.0f);
            mods::arg_ref<f32>(args, 2) += cfg_pos(
                layout_handle4(g_dialogActionTextY,g_swapDialogActionTextY,g_ybDialogActionTextY,g_xyxbDialogActionTextY),0.0f);
        }
        if (J2DPane* pane=mgr->getPanePtr()) {
            const float factor=cfg_scale(g_actionTextScale,1.0f);
            pane->scale(pane->getScaleX()*factor,pane->getScaleY()*factor);
        }
        return HOOK_CONTINUE;
    }

    if (mgr == s_activeMeter->mpTextB) {
        // b_text_n: texto contextual do B, usado como "Voltar" na tela de itens.
        // The lock is optional and leaves vanilla behavior untouched when OFF.
        static bool baseValid=false;
        static float baseX=0.0f, baseY=0.0f;
        static bool lastLock=false;
        const ConfigVarHandle backTextAnim =
            layout_handle4(g_backTextAnim,g_swapBackTextAnim,g_ybBackTextAnim,g_xyxbBackTextAnim);
        const bool lock=!cfg_bool(backTextAnim,true);
        float& tx=mods::arg_ref<f32>(args, 1);
        float& ty=mods::arg_ref<f32>(args, 2);
        if (lock!=lastLock) { baseValid=false; lastLock=lock; }
        if (lock) {
            if (!baseValid) { baseX=tx; baseY=ty; baseValid=true; }
            tx=baseX; ty=baseY;
        }
        const ConfigVarHandle backTextX=layout_handle4(g_backTextX,g_backTextX,g_ybBackTextX,g_xyxbBackTextX);
        const ConfigVarHandle backTextY=layout_handle4(g_backTextY,g_backTextY,g_ybBackTextY,g_xyxbBackTextY);
        const ConfigVarHandle backTextScale=layout_handle4(g_backTextScale,g_backTextScale,g_ybBackTextScale,g_xyxbBackTextScale);
        tx += cfg_pos(backTextX,0.0f);
        ty += cfg_pos(backTextY,0.0f);
        if (J2DPane* pane=mgr->getPanePtr()) {
            const float factor=cfg_scale(backTextScale,1.0f);
            pane->scale(pane->getScaleX()*factor,pane->getScaleY()*factor);
        }
        return HOOK_CONTINUE;
    }

    if (mgr == s_activeMeter->mpButtonA) {
        mods::arg_ref<f32>(args, 1) = cfg_pos(g_crossX,118.0f) - 99.0f;
        mods::arg_ref<f32>(args, 2) = cfg_pos(g_crossY,65.7f) - 41.5f;
        const float sc=cfg_scale(g_crossScale,1.45f);
        if (mgr->getPanePtr()!=nullptr) mgr->getPanePtr()->scale(sc,sc);
    } else if (mgr == s_activeMeter->mpButtonB) {
        // Current behavior stays the default. When enabled, re-add only the
        // displacement that vanilla applies around its known idle paneTrans.
        const float vanillaX=mods::arg_ref<f32>(args, 1);
        const float vanillaY=mods::arg_ref<f32>(args, 2);
        float animX=0.0f, animY=0.0f;
        const ConfigVarHandle backButtonAnim =
            layout_handle4(g_backButtonAnim,g_swapBackButtonAnim,g_ybBackButtonAnim,g_xyxbBackButtonAnim);
        if (cfg_bool(backButtonAnim,false)) {
            animX=vanillaX+2.2f;
            animY=vanillaY+1.3f;
        }
        mods::arg_ref<f32>(args, 1) = cfg_pos(layout_handle4(g_circleX,g_circleX,g_ybCircleX,g_xyxbCircleX),151.5f) - 81.5f + animX;
        mods::arg_ref<f32>(args, 2) = cfg_pos(layout_handle4(g_circleY,g_circleY,g_ybCircleY,g_xyxbCircleY),39.4f) - 76.0f + animY;
        const float sc=cfg_scale(layout_handle4(g_circleScale,g_circleScale,g_ybCircleScale,g_xyxbCircleScale),1.45f);
        if (mgr->getPanePtr()!=nullptr) mgr->getPanePtr()->scale(sc,sc);
    } else if (mgr == s_activeMeter->mpButtonXY[2]) {
        // Z/R1 is also repositioned by vanilla each frame. Use the editor values
        // directly as paneTrans offsets; full-draw normalization below fixes bounds.
        mods::arg_ref<f32>(args,1)=cfg_pos(g_r1X,170.0f);
        mods::arg_ref<f32>(args,2)=cfg_pos(g_r1Y,-10.0f);
        const float sc=cfg_scale(g_r1Scale,1.45f);
        if (mgr->getPanePtr()!=nullptr) mgr->getPanePtr()->scale(sc,sc);
    }
    return HOOK_CONTINUE;
}


struct SwordPictureState {
    J2DPicture* pane = nullptr;
    JGeometry::TBox2<f32> base{};
    float lastDx = 0.0f, lastDy = 0.0f, lastScale = 1.0f;
    bool captured = false;
};
SwordPictureState s_swordPicture{};

static void adjust_sword_picture(J2DPicture* pane,float dx,float dy,float sc) {
    if(pane==nullptr) return;

    auto& st=s_swordPicture;
    const auto& cur=pane->getBounds();

    if(!st.captured || st.pane!=pane) {
        st.pane=pane;
        st.base=cur;
        st.lastDx=st.lastDy=0.0f;
        st.lastScale=1.0f;
        st.captured=true;
    } else {
        const float bw=st.base.getWidth(), bh=st.base.getHeight();
        const float pw=bw*st.lastScale, ph=bh*st.lastScale;
        const float px=st.base.i.x+st.lastDx-(pw-bw)*0.5f;
        const float py=st.base.i.y+st.lastDy-(ph-bh)*0.5f;

        // If vanilla recreated/reset the picture (area/load/item refresh),
        // accept its current geometry as the new clean baseline.
        if(((cur.i.x>px?cur.i.x-px:px-cur.i.x)>0.25f) ||
           ((cur.i.y>py?cur.i.y-py:py-cur.i.y)>0.25f) ||
           ((cur.getWidth()>pw?cur.getWidth()-pw:pw-cur.getWidth())>0.25f) ||
           ((cur.getHeight()>ph?cur.getHeight()-ph:ph-cur.getHeight())>0.25f)) {
            st.base=cur;
        }
    }

    const float bw=st.base.getWidth(), bh=st.base.getHeight();
    const float nw=bw*sc, nh=bh*sc;
    pane->move(st.base.i.x+dx-(nw-bw)*0.5f,
               st.base.i.y+dy-(nh-bh)*0.5f);
    pane->resize(nw,nh);
    st.lastDx=dx;
    st.lastDy=dy;
    st.lastScale=sc;
}

struct MidnaPictureState {
    J2DPane* pane = nullptr;
    JGeometry::TBox2<f32> base{};
    float baseParentScaleX = 1.0f;
    float baseParentScaleY = 1.0f;
    float lastDx = 0.0f, lastDy = 0.0f, lastScale = 1.0f;
    bool captured = false;
};
MidnaPictureState s_midnaPictures[3]{};

static bool nearf(float a,float b,float eps=0.25f) {
    return (a>b ? a-b : b-a) <= eps;
}

static bool is_midna_picture(J2DPane* pane) {
    if (pane==nullptr || pane->getTypeID()!=18) return false;
    return pane->mInfoTag == MULTI_CHAR('midona_s') ||
           pane->mInfoTag == MULTI_CHAR('midona') ||
           pane->mInfoTag == MULTI_CHAR('j_light1');
}

static void midna_parent_scale(J2DPane* pane,float& sx,float& sy) {
    sx=1.0f; sy=1.0f;
    if (pane==nullptr) return;
    for (J2DPane* p=pane->getParentPane(); p!=nullptr; p=p->getParentPane()) {
        sx*=p->getScaleX();
        sy*=p->getScaleY();
    }
    if (fabsf(sx)<0.0001f) sx=1.0f;
    if (fabsf(sy)<0.0001f) sy=1.0f;
}

static int midna_slot_for(J2DPane* pane) {
    if (pane==nullptr) return -1;
    if (pane->mInfoTag == MULTI_CHAR('midona_s')) return 0;
    if (pane->mInfoTag == MULTI_CHAR('midona')) return 1;
    if (pane->mInfoTag == MULTI_CHAR('j_light1')) return 2;
    return -1;
}

static void capture_midna_baseline(J2DPane* pane) {
    const int slot=midna_slot_for(pane);
    if (slot<0 || pane==nullptr) return;

    MidnaPictureState& st=s_midnaPictures[slot];
    st.pane=pane;
    st.base=pane->getBounds();
    midna_parent_scale(pane,st.baseParentScaleX,st.baseParentScaleY);
    st.lastDx=0.0f;
    st.lastDy=0.0f;
    st.lastScale=1.0f;
    st.captured=true;
}

static void capture_midna_baselines_from_screen(J2DScreen* screen) {
    if (screen==nullptr) return;

    // screenInitButton is our clean construction point. Capture local geometry
    // here before TP Classic ever scales the portrait and before Essentials
    // later takes ownership of its contextual Midna/Z layout.
    const u64 tags[3] = {
        MULTI_CHAR('midona_s'),
        MULTI_CHAR('midona'),
        MULTI_CHAR('j_light1'),
    };
    for (u64 tag : tags) {
        J2DPane* pane=screen->search(tag);
        if (is_midna_picture(pane))
            capture_midna_baseline(pane);
    }
}

static MidnaPictureState* midna_state_for(J2DPane* pane) {
    const int slot=midna_slot_for(pane);
    if (slot<0) return nullptr;

    MidnaPictureState& st=s_midnaPictures[slot];
    if (st.captured) {
        st.pane=pane;
        return &st;
    }

    // Fallback only for the MAIN HUD portrait tree. A contextual Z prompt
    // contains a different 60x60 picture with the same "midona" tag.
    bool underMainMidnaRoot=false;
    for (J2DPane* p=pane->getParentPane();p!=nullptr;p=p->getParentPane()) {
        if (p->mInfoTag==MULTI_CHAR('midona_n')) {
            underMainMidnaRoot=true;
            break;
        }
    }
    if (!underMainMidnaRoot || dComIfGp_event_runCheck())
        return nullptr;

    capture_midna_baseline(pane);
    return &st;
}

static void adjust_midna_pictures(J2DPane* pane,float dx,float dy,float sc) {
    if(pane==nullptr) return;
    if(is_midna_picture(pane)) {
        MidnaPictureState* st=midna_state_for(pane);
        if (st==nullptr) {
            for(J2DPane* child=pane->getFirstChildPane();child!=nullptr;child=child->getNextChildPane())
                adjust_midna_pictures(child,dx,dy,sc);
            return;
        }

        const float bw=st->base.getWidth();
        const float bh=st->base.getHeight();

        // Parent/root transforms can change across area transitions or events.
        // Compensate relative to the clean parent scale captured at screen init,
        // but never recapture the portrait from a live gameplay/cutscene frame.
        float parentScaleX=1.0f,parentScaleY=1.0f;
        midna_parent_scale(pane,parentScaleX,parentScaleY);
        float compensateX=st->baseParentScaleX/parentScaleX;
        float compensateY=st->baseParentScaleY/parentScaleY;
        if (!std::isfinite(compensateX) || fabsf(compensateX)<0.0001f) compensateX=1.0f;
        if (!std::isfinite(compensateY) || fabsf(compensateY)<0.0001f) compensateY=1.0f;

        const float nw=bw*sc*compensateX;
        const float nh=bh*sc*compensateY;
        pane->move(st->base.i.x+dx-(nw-bw)*0.5f,
                   st->base.i.y+dy-(nh-bh)*0.5f);
        pane->resize(nw,nh);
        st->lastDx=dx;
        st->lastDy=dy;
        st->lastScale=sc;
    }

    for(J2DPane* child=pane->getFirstChildPane();child!=nullptr;child=child->getNextChildPane())
        adjust_midna_pictures(child,dx,dy,sc);
}

static void tag_to_text(u64 tag,char out[9]) {
    for(int i=0;i<8;i++) {
        const unsigned shift=(unsigned)((7-i)*8);
        unsigned char c=(unsigned char)((tag>>shift)&0xFF);
        out[i]=(c>=32 && c<=126)?(char)c:'.';
    }
    out[8]='\0';
}

static void append_midna_pane_diag(std::string& out,const char* name,J2DPane* pane) {
    char line[1024];
    if (pane==nullptr) {
        std::snprintf(line,sizeof(line),"  %s: <null>\n",name);
        out+=line;
        return;
    }

    const auto& b=pane->getBounds();
    const auto& gb=pane->getGlbBounds();
    char tag[9],user[9];
    tag_to_text(pane->mInfoTag,tag);
    tag_to_text(pane->getUserInfo(),user);

    float parentSX=1.0f,parentSY=1.0f;
    midna_parent_scale(pane,parentSX,parentSY);

    std::snprintf(line,sizeof(line),
        "  %s: ptr=%p tag='%s' user='%s' type=%u vis=%d alpha=%u "
        "local=(%.3f,%.3f %.3fx%.3f) global=(%.3f,%.3f %.3fx%.3f) "
        "scale=(%.4f,%.4f) parentAccumScale=(%.4f,%.4f)\n",
        name,(void*)pane,tag,user,(unsigned)pane->getTypeID(),
        pane->isVisible()?1:0,(unsigned)pane->getAlpha(),
        (double)b.i.x,(double)b.i.y,(double)b.getWidth(),(double)b.getHeight(),
        (double)gb.i.x,(double)gb.i.y,(double)gb.getWidth(),(double)gb.getHeight(),
        (double)pane->getScaleX(),(double)pane->getScaleY(),
        (double)parentSX,(double)parentSY);
    out+=line;

    int depth=0;
    for(J2DPane* p=pane->getParentPane();p!=nullptr && depth<10;p=p->getParentPane(),++depth) {
        const auto& pb=p->getBounds();
        const auto& pgb=p->getGlbBounds();
        char ptag[9],puser[9];
        tag_to_text(p->mInfoTag,ptag);
        tag_to_text(p->getUserInfo(),puser);
        std::snprintf(line,sizeof(line),
            "    parent[%d]: ptr=%p tag='%s' user='%s' type=%u vis=%d alpha=%u "
            "local=(%.3f,%.3f %.3fx%.3f) global=(%.3f,%.3f %.3fx%.3f) scale=(%.4f,%.4f)\n",
            depth,(void*)p,ptag,puser,(unsigned)p->getTypeID(),
            p->isVisible()?1:0,(unsigned)p->getAlpha(),
            (double)pb.i.x,(double)pb.i.y,(double)pb.getWidth(),(double)pb.getHeight(),
            (double)pgb.i.x,(double)pgb.i.y,(double)pgb.getWidth(),(double)pgb.getHeight(),
            (double)p->getScaleX(),(double)p->getScaleY());
        out+=line;
    }
}

static void capture_midna_diagnostic_snapshot(const char* reason,dMeter2Draw_c* meter,J2DScreen* explicitScreen=nullptr) {
    if (!kDeveloperOptions) return;
    if (s_midnaDiagnostic.size()>120000) return;

    char header[512];
    std::snprintf(header,sizeof(header),
        "\n=== SNAPSHOT: %s ===\n"
        "eventRunning=%d externalPromptOwner=%d configuredMidna=(x=%lld y=%lld scale=%lld)\n",
        reason,
        dComIfGp_event_runCheck()?1:0,
        s_externalMidnaPromptOwner?1:0,
        (long long)cfg_int(g_midnaX,80),
        (long long)cfg_int(g_midnaY,-90),
        (long long)cfg_int(g_midnaScale,65));
    s_midnaDiagnostic+=header;

    J2DScreen* screen=explicitScreen;
    if (screen==nullptr && meter!=nullptr) screen=meter->getMainScreenPtr();

    if (screen!=nullptr) {
        append_midna_pane_diag(s_midnaDiagnostic,"screen.midona_n",screen->search(MULTI_CHAR('midona_n')));
        append_midna_pane_diag(s_midnaDiagnostic,"screen.midona_s",screen->search(MULTI_CHAR('midona_s')));
        append_midna_pane_diag(s_midnaDiagnostic,"screen.midona",screen->search(MULTI_CHAR('midona')));
        append_midna_pane_diag(s_midnaDiagnostic,"screen.j_light1",screen->search(MULTI_CHAR('j_light1')));
        append_midna_pane_diag(s_midnaDiagnostic,"screen.juji_n",screen->search(MULTI_CHAR('juji_n')));
    } else {
        s_midnaDiagnostic+="  main screen: <null>\n";
    }

    if (meter!=nullptr && meter->mpButtonMidona!=nullptr)
        append_midna_pane_diag(s_midnaDiagnostic,"meter.mpButtonMidona",meter->mpButtonMidona->getPanePtr());

    for(int i=0;i<3;++i) {
        char label[64];
        std::snprintf(label,sizeof(label),"storedBaseline[%d]",i);
        char line[512];
        const auto& st=s_midnaPictures[i];
        if (!st.captured) {
            std::snprintf(line,sizeof(line),"  %s: <not captured>\n",label);
        } else {
            std::snprintf(line,sizeof(line),
                "  %s: pane=%p base=(%.3f,%.3f %.3fx%.3f) baseParentScale=(%.4f,%.4f) last=(dx=%.3f dy=%.3f scale=%.4f)\n",
                label,(void*)st.pane,
                (double)st.base.i.x,(double)st.base.i.y,
                (double)st.base.getWidth(),(double)st.base.getHeight(),
                (double)st.baseParentScaleX,(double)st.baseParentScaleY,
                (double)st.lastDx,(double)st.lastDy,(double)st.lastScale);
        }
        s_midnaDiagnostic+=line;
    }
}


static void log_picture_tree(J2DPane* pane,int depth,int& count) {
    if(pane==nullptr || svc_log==nullptr || count>=240 || depth>10) return;
    if(pane->getTypeID()==18) {
        const JGeometry::TBox2<f32>& b=pane->getBounds();
        const JGeometry::TBox2<f32>& gb=pane->getGlbBounds();
        char tag[9],user[9],line[640];
        tag_to_text(pane->mInfoTag,tag);
        tag_to_text(pane->getUserInfo(),user);
        snprintf(line,sizeof(line),
            "[HudPic] n=%d depth=%d tag='%s' user='%s' vis=%d alpha=%u "
            "local=(%.1f,%.1f %.1fx%.1f) global=(%.1f,%.1f %.1fx%.1f) scale=(%.2f,%.2f)",
            count,depth,tag,user,pane->isVisible()?1:0,(unsigned)pane->getAlpha(),
            (double)b.i.x,(double)b.i.y,(double)b.getWidth(),(double)b.getHeight(),
            (double)gb.i.x,(double)gb.i.y,(double)gb.getWidth(),(double)gb.getHeight(),
            (double)pane->getScaleX(),(double)pane->getScaleY());
        svc_log->info(mod_ctx,line);
        ++count;
    }
    for(J2DPane* c=pane->getFirstChildPane();c!=nullptr;c=c->getNextChildPane())
        log_picture_tree(c,depth+1,count);
}

static void log_button_layers(const char* name,J2DPane* root) {
    if(root==nullptr || svc_log==nullptr) return;
    int i=0;
    for(J2DPane* c=root->getFirstChildPane();c!=nullptr && i<12;c=c->getNextChildPane(),++i) {
        char tag[9],user[9],line[420];
        tag_to_text(c->mInfoTag,tag); tag_to_text(c->getUserInfo(),user);
        const auto& b=c->getBounds();
        snprintf(line,sizeof(line),
            "[GlowLayer] button=%s child=%d type=%u tag='%s' user='%s' vis=%d alpha=%u local=(%.1f,%.1f %.1fx%.1f) scale=(%.2f,%.2f)",
            name,i,(unsigned)c->getTypeID(),tag,user,c->isVisible()?1:0,(unsigned)c->getAlpha(),
            (double)b.i.x,(double)b.i.y,(double)b.getWidth(),(double)b.getHeight(),
            (double)c->getScaleX(),(double)c->getScaleY());
        svc_log->info(mod_ctx,line);
    }
}


struct WolfTextBase {
    bool valid;
    float x;
    float y;
    float sx;
    float sy;
};
WolfTextBase s_wolfTextBase[2] = {};

void after_button_xy_draw(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    const int i_no = mods::arg<int>(args, 1);
    if (meter == nullptr || i_no < 0 || i_no > 1) return;

    CPaneMgr* text = meter->mpTextXY[i_no];
    if (text == nullptr || text->getPanePtr() == nullptr) return;

    // drawButtonXY is event-driven: it is not called every frame.  Capture only
    // the vanilla transform here.  The live config is applied from MeterDrawHook
    // every frame so slider changes are visible immediately without accumulating.
    J2DPane* pane = text->getPanePtr();
    s_wolfTextBase[i_no].valid = true;
    s_wolfTextBase[i_no].x = pane->getTranslateX();
    s_wolfTextBase[i_no].y = pane->getTranslateY();
    s_wolfTextBase[i_no].sx = pane->getScaleX();
    s_wolfTextBase[i_no].sy = pane->getScaleY();
}

void apply_wolf_text_config(dMeter2Draw_c* meter) {
    if (meter == nullptr) return;
    for (int i_no = 0; i_no < 2; ++i_no) {
        if (!s_wolfTextBase[i_no].valid) continue;
        CPaneMgr* text = meter->mpTextXY[i_no];
        if (text == nullptr || text->getPanePtr() == nullptr) continue;

        ConfigVarHandle xh = (i_no == 0)
            ? layout_handle4(g_wolfSenseX,g_swapWolfSenseX,g_ybWolfSenseX,g_xyxbWolfSenseX)
            : layout_handle4(g_wolfDigX,g_swapWolfDigX,g_ybWolfDigX,g_xyxbWolfDigX);
        ConfigVarHandle yh = (i_no == 0)
            ? layout_handle4(g_wolfSenseY,g_swapWolfSenseY,g_ybWolfSenseY,g_xyxbWolfSenseY)
            : layout_handle4(g_wolfDigY,g_swapWolfDigY,g_ybWolfDigY,g_xyxbWolfDigY);
        ConfigVarHandle sh = (i_no == 0)
            ? layout_handle4(g_wolfSenseScale,g_swapWolfSenseScale,g_ybWolfSenseScale,g_xyxbWolfSenseScale)
            : layout_handle4(g_wolfDigScale,g_swapWolfDigScale,g_ybWolfDigScale,g_xyxbWolfDigScale);
        const WolfTextBase& base = s_wolfTextBase[i_no];
        const float factor = cfg_scale(sh, 1.0f);

        J2DPane* pane = text->getPanePtr();
        pane->scale(base.sx * factor, base.sy * factor);
        text->paneTrans(base.x + cfg_pos(xh, 0.0f), base.y + cfg_pos(yh, 0.0f));
    }
}

void after_meter_draw(ModContext*, void* args, void*, void*) {
    auto* midnaDiagMeter = args != nullptr ? mods::arg<dMeter2Draw_c*>(args,0) : nullptr;
    if (kDeveloperOptions) {
        const bool eventNow=dComIfGp_event_runCheck();
        if (!s_midnaDiagLastEvent && eventNow) {
            capture_midna_diagnostic_snapshot("event-start",midnaDiagMeter);
            s_midnaDiagPostEventFrame=-1;
        } else if (s_midnaDiagLastEvent && !eventNow) {
            capture_midna_diagnostic_snapshot("event-end-frame0",midnaDiagMeter);
            s_midnaDiagPostEventFrame=0;
        }
        s_midnaDiagLastEvent=eventNow;

        if (!eventNow && s_midnaDiagPostEventFrame>=0) {
            ++s_midnaDiagPostEventFrame;
            if (s_midnaDiagPostEventFrame==1)
                capture_midna_diagnostic_snapshot("post-event-frame1",midnaDiagMeter);
            else if (s_midnaDiagPostEventFrame==3)
                capture_midna_diagnostic_snapshot("post-event-frame3",midnaDiagMeter);
            else if (s_midnaDiagPostEventFrame==10) {
                capture_midna_diagnostic_snapshot("post-event-frame10",midnaDiagMeter);
                s_midnaDiagPostEventFrame=-1;
            }
        }
    }

    if (s_fishingCheckPane != nullptr) {
        s_fishingCheckPane->translate(s_fishingCheckBaseX, s_fishingCheckBaseY);
        s_fishingCheckPane->scale(s_fishingCheckBaseSX, s_fishingCheckBaseSY);
        s_fishingCheckPane = nullptr;
    }
    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);

    // draw() has already consumed the temporary ammo scale. Restore vanilla
    // state immediately so no later game logic inherits the modded value.
    if (s_ammoScaleApplied && s_ammoScaleMeter != nullptr) {
        for (int i = 0; i < 2; ++i) {
            s_ammoScaleMeter->mItemParams[i].num_scale = s_ammoNumScaleBase[i];
            s_ammoScaleMeter->mItemParams[i].num_pos_x = s_ammoNumPosXBase[i];
            s_ammoScaleMeter->mItemParams[i].num_pos_y = s_ammoNumPosYBase[i];
        }
    }
    s_ammoScaleApplied = false;
    s_ammoScaleMeter = nullptr;

    apply_wolf_text_config(meter);
    if (meter == nullptr || meter->mpButtonParent == nullptr || meter->mpButtonA == nullptr ||
        meter->mpButtonB == nullptr || meter->mpButtonXY[0] == nullptr || meter->mpButtonXY[1] == nullptr) {
        s_activeMeter = nullptr;
        return;
    }

    J2DPane* parent = meter->mpButtonParent->getPanePtr();
    J2DPane* a = meter->mpButtonA->getPanePtr();
    J2DPane* b = meter->mpButtonB->getPanePtr();
    J2DPane* x = meter->mpButtonXY[0]->getPanePtr();
    J2DPane* y = meter->mpButtonXY[1]->getPanePtr();
    if (parent == nullptr || a == nullptr || b == nullptr || x == nullptr || y == nullptr) {
        s_activeMeter = nullptr;
        return;
    }

    // Capture only the actual main HUD Midna subtree. The contextual prompt
    // screen has another pane named "midona" and is intentionally excluded.
    if (J2DScreen* mainScreen=meter->getMainScreenPtr()) {
        J2DPane* mainRoot=mainScreen->search(MULTI_CHAR('midona_n'));
        if (mainRoot!=nullptr) {
            const u64 tags[3] = {
                MULTI_CHAR('midona_s'),
                MULTI_CHAR('midona'),
                MULTI_CHAR('j_light1'),
            };
            for (u64 tag : tags) {
                J2DPane* pane=mainScreen->search(tag);
                const int slot=midna_slot_for(pane);
                if (slot>=0 && !s_midnaPictures[slot].captured)
                    capture_midna_baseline(pane);
            }
        }
    }

    // Calibration guide + optional decorative HUD ornament. Both reuse the original
    // GameCube ornament container, but remain independently visible/configurable.
    // The first picture remains the proven calibration guide path. The second vanilla
    // ornament layer is reused only as a host for the new decorative texture.
    if (J2DPane* ornament = child_at(parent, 0)) {
        const bool showGuide = cfg_bool(g_showGuide, true);
        const bool showHudOrnament = cfg_bool(g_hudOrnamentEnabled, true);
        if (showGuide || showHudOrnament) {
            ornament->show();
            const float gx=cfg_pos(g_guideX,84.0f);
            const float gy=cfg_pos(g_guideY,2.0f);
            const float gs=cfg_scale(g_guideScale,1.0f);
            set_bounds(ornament,gx,gy,96.0f,96.0f);
            ornament->scale(gs,gs);

            const JUtility::TColor neutralBlack(0,0,0,0);
            const JUtility::TColor neutralWhite(255,255,255,255);
            if (J2DPicture* guide = picture_child(ornament, 0)) {
                if (showGuide) {
                    replace_picture_texture(guide, resource_timg(s_guide));
                    guide->setBlackWhite(neutralBlack,neutralWhite);
                    guide->setCornerColor(neutralWhite);
                    guide->setAlpha(255);
                    guide->show();
                    set_bounds(guide,0.0f,0.0f,96.0f,96.0f);
                    guide->rotate(48.0f,48.0f,ROTATE_Z,0.0f);
                } else {
                    guide->hide();
                }
            }

            if (J2DPicture* hudOrnament = picture_child(ornament, 1)) {
                if (showHudOrnament) {
                    replace_picture_texture(hudOrnament, resource_timg(s_hud_ornament));
                    hudOrnament->setBlackWhite(neutralBlack,neutralWhite);
                    hudOrnament->setCornerColor(neutralWhite);
                    const float hx=cfg_pos(g_hudOrnamentX,0.0f);
                    const float hy=cfg_pos(g_hudOrnamentY,0.0f);
                    const float hs=cfg_scale(g_hudOrnamentScale,1.0f);
                    // The ornament still uses the proven vanilla host, but cancel the
                    // Calibration Guide container translation/scale so guide adjustments
                    // cannot move or resize it. 84/2 preserve the v0.10.66 visual baseline.
                    const float safeGs = (gs > 0.001f ? gs : 1.0f);
                    const float ornamentBaseX = 84.0f;
                    const float ornamentBaseY = 2.0f;
                    set_bounds(hudOrnament,(ornamentBaseX + hx - gx)/safeGs,
                               (ornamentBaseY + hy - gy)/safeGs,96.0f,96.0f);
                    hudOrnament->scale(hs/safeGs,hs/safeGs);
                    // Keep the natural translucency from the source texture/HUD material.
                    hudOrnament->setAlpha(255);
                    hudOrnament->rotate(48.0f,48.0f,ROTATE_Z,0.0f);
                    hudOrnament->show();
                } else {
                    hudOrnament->hide();
                }
            }
        } else {
            ornament->hide();
        }
    }

    // Mapeamento PlayStation:
    // A -> Cross, B -> Circle, X -> Triangle, Y -> Square.
    apply_full_button(a, resource_timg(s_cross));
    apply_full_button(b, gc_b_face_texture());
    apply_full_button(x, gc_x_face_texture());
    apply_full_button(y, gc_y_face_texture());

    // Z do GameCube -> R1 do PlayStation. Mantemos mpItemR e textos separados:
    // esta etapa troca apenas a superficie visual do botao Z/XY2.
    J2DPane* z = meter->mpButtonXY[2] != nullptr ? meter->mpButtonXY[2]->getPanePtr() : nullptr;
    if (z != nullptr) {
        // v0.10.38: HUD and Options intentionally use separate R1 resources.
        // r1_hud.bti is the pre-Options 2:1 resource whose size/position were
        // already calibrated in the normal HUD. Restore that exact path here.
        apply_full_button(z, resource_timg(s_r1_hud));
        const float zx=cfg_pos(g_r1X,170.0f);
        const float zy=cfg_pos(g_r1Y,-10.0f);
        // v0.10.93: the new R1 artwork is much wider than the original Z face.
        // Give it a real 48x24 base instead of trying to enlarge the old 32x16 box.
        // Keep the old visual center so existing X/Y calibration remains coherent.
        constexpr float oldW = 32.0f, oldH = 16.0f;
        constexpr float newW = 48.0f, newH = 24.0f;
        const float baseX = zx + (oldW - newW) * 0.5f;
        const float baseY = zy + (oldH - newH) * 0.5f;
        set_bounds(z,baseX,baseY,newW,newH);
        if (J2DPicture* face=picture_child(z,0)) {
            set_bounds(face,0.0f,0.0f,newW,newH);
            face->rotate(newW * 0.5f,newH * 0.5f,ROTATE_Z,0.0f);
        }
        const float zsc=cfg_scale(g_r1Scale,1.45f);
        meter->mpButtonXY[2]->scale(zsc,zsc);
    }

    // v0.9.6 targeted HUD picture inspector.
    // Dump the complete picture tree once while the HUD is visible.
    if(!g_swordDiagLogged && meter->mpScreen!=nullptr && svc_log!=nullptr) {
        svc_log->info(mod_ctx,"[HudPic] BEGIN full zelda_game_image picture tree");
        int picCount=0;
        log_picture_tree(meter->mpScreen,0,picCount);
        char line[128];
        snprintf(line,sizeof(line),"[HudPic] END count=%d",picCount);
        svc_log->info(mod_ctx,line);
        g_swordDiagLogged=true;
    }

    // v0.9.2 item editor:
    // v0.9.1 somava o offset e multiplicava a escala sobre o valor do frame anterior.
    // Como estes panes nao sao necessariamente restaurados pelo jogo a cada frame,
    // isso causava deriva exponencial (o item "andava" ou "crescia" sozinho).
    //
    // Agora removemos PRIMEIRO a transformacao aplicada no frame anterior e so
    // depois aplicamos os valores atuais. Assim o ajuste e absoluto/reversivel
    // sem destruir a posicao vanilla dinamica.
    struct ItemAdjustState {
        J2DPane* pane;
        float dx;
        float dy;
        float scale;
        float sxFactor;
        float syFactor;
        // Item X/Y use per-item vanilla offsets (bow, bottle, bombs, etc.).
        // Keep a normalized base with those offsets removed so swapping items
        // does not move the visual anchor inside our compact button layout.
        float normalizedBaseX;
        float normalizedBaseY;
        bool normalizedBaseValid;
    };
    static ItemAdjustState itemState[6] = {
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false},
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false},
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false},
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false},
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false},
        {nullptr,0.0f,0.0f,1.0f,1.0f,1.0f,0.0f,0.0f,false}
    };

    auto apply_item_adjust=[meter](int slot,CPaneMgr* mgr,ConfigVarHandle xh,
                              ConfigVarHandle yh,ConfigVarHandle sh,
                              ConfigVarHandle flipH,ConfigVarHandle flipV) {
        if(mgr==nullptr || mgr->getPanePtr()==nullptr) return;
        J2DPane* pane=mgr->getPanePtr();
        ItemAdjustState& st=itemState[slot];

        // Um pane novo significa tela/contexto novo: nao herdar estado antigo.
        if(st.pane!=pane) {
            st.pane=pane;
            st.dx=0.0f;
            st.dy=0.0f;
            st.scale=1.0f;
            st.sxFactor=1.0f;
            st.syFactor=1.0f;
            st.normalizedBaseX=0.0f;
            st.normalizedBaseY=0.0f;
            st.normalizedBaseValid=false;
        }

        // Detecta reset/rebuild vanilla (troca de area, load etc.).
        // Se o pane ainda contem nosso ajuste anterior, removemos esse ajuste.
        // Se o jogo ja restaurou outra geometria, a geometria atual vira a nova base.
        const float curX=pane->getBounds().i.x;
        const float curY=pane->getBounds().i.y;
        const float curSX=pane->getScaleX();
        const float curSY=pane->getScaleY();

        static float lastOutX[6]={}, lastOutY[6]={}, lastOutSX[6]={}, lastOutSY[6]={};
        static bool haveOut[6]={false,false,false,false,false,false};

        const bool stillOurs=haveOut[slot] &&
            nearf(curX,lastOutX[slot]) && nearf(curY,lastOutY[slot]) &&
            nearf(curSX,lastOutSX[slot],0.01f) && nearf(curSY,lastOutSY[slot],0.01f);

        float baseX=stillOurs ? curX-st.dx : curX;
        float baseY=stillOurs ? curY-st.dy : curY;
        const float baseScaleX=(stillOurs && st.sxFactor!=0.0f) ? curSX/st.sxFactor : curSX;
        const float baseScaleY=(stillOurs && st.syFactor!=0.0f) ? curSY/st.syFactor : curSY;

        // v0.10.73: normalize the item POSITION anchor. setItemParamX/Y gives
        // every item its own pos_x/pos_y because the original X and Y HUD slots
        // lived on opposite sides of the screen. In the compact layout that
        // makes a bottle, bow, bombs, etc. jump when the equipped item changes.
        // Remove only that per-item vanilla position. Scale and rotation remain
        // vanilla-per-item, and the v0.10.70 ammo behavior remains untouched.
        if (slot < 2) {
            if (!stillOurs) {
                st.normalizedBaseX = baseX - meter->mItemParams[slot].pos_x;
                st.normalizedBaseY = baseY - meter->mItemParams[slot].pos_y;
                st.normalizedBaseValid = true;
            }
            if (st.normalizedBaseValid) {
                baseX = st.normalizedBaseX;
                baseY = st.normalizedBaseY;
            }
        }

        // The group offset moves both normalized slot anchors together. The
        // existing Square/Triangle X/Y values remain fine adjustments per slot.
        const float dx=cfg_pos(xh,0.0f) + cfg_pos(g_itemsAnchorX,0.0f);
        const float dy=cfg_pos(yh,0.0f) + cfg_pos(g_itemsAnchorY,0.0f);
        const float sc=cfg_scale(sh,1.0f);
        const float signedScaleX=sc*(cfg_bool(flipH,false) ? -1.0f : 1.0f);
        const float signedScaleY=sc*(cfg_bool(flipV,false) ? -1.0f : 1.0f);

        pane->move(baseX+dx,baseY+dy);
        pane->scale(baseScaleX*signedScaleX,baseScaleY*signedScaleY);

        st.dx=dx;
        st.dy=dy;
        // st.scale is used to recover the vanilla scale. Horizontal and vertical
        // can differ in sign, so recovery below is handled from the last output.
        st.scale=sc;
        st.sxFactor=signedScaleX;
        st.syFactor=signedScaleY;
        lastOutX[slot]=pane->getBounds().i.x;
        lastOutY[slot]=pane->getBounds().i.y;
        lastOutSX[slot]=pane->getScaleX();
        lastOutSY[slot]=pane->getScaleY();
        haveOut[slot]=true;
    };

    apply_item_adjust(0,meter->mpItemXY[0],
                      layout_handle4(g_itemSquareX,g_swapItemSquareX,g_ybItemSquareX,g_xyxbItemSquareX),
                      layout_handle4(g_itemSquareY,g_swapItemSquareY,g_ybItemSquareY,g_xyxbItemSquareY),
                      layout_handle4(g_itemSquareScale,g_swapItemSquareScale,g_ybItemSquareScale,g_xyxbItemSquareScale),
                      layout_handle4(g_itemSquareFlipH,g_swapItemSquareFlipH,g_ybItemSquareFlipH,g_xyxbItemSquareFlipH),
                      layout_handle4(g_itemSquareFlipV,g_swapItemSquareFlipV,g_ybItemSquareFlipV,g_xyxbItemSquareFlipV));
    apply_item_adjust(1,meter->mpItemXY[1],
                      layout_handle4(g_itemTriangleX,g_swapItemTriangleX,g_ybItemTriangleX,g_xyxbItemTriangleX),
                      layout_handle4(g_itemTriangleY,g_swapItemTriangleY,g_ybItemTriangleY,g_xyxbItemTriangleY),
                      layout_handle4(g_itemTriangleScale,g_swapItemTriangleScale,g_ybItemTriangleScale,g_xyxbItemTriangleScale),
                      layout_handle4(g_itemTriangleFlipH,g_swapItemTriangleFlipH,g_ybItemTriangleFlipH,g_xyxbItemTriangleFlipH),
                      layout_handle4(g_itemTriangleFlipV,g_swapItemTriangleFlipV,g_ybItemTriangleFlipV,g_xyxbItemTriangleFlipV));
    // v0.9.13 sword test: target the dynamically appended visible J2DPicture
    // instead of its mpItemB container.
// Midna v0.9.8: leave the vanilla root alone and transform only its pictures.
    // When Twilit Essentials is active, it owns Midna's contextual layout and
    // positioning. Do not apply TP Classic offsets/scaling on top of it.
    if (!twilit_midna_layout_active(meter) && meter->mpButtonMidona != nullptr) {
        adjust_midna_pictures(meter->mpButtonMidona->getPanePtr(),
            cfg_pos(g_midnaX,7.0f), cfg_pos(g_midnaY,-18.0f),
            cfg_scale(g_midnaScale,1.0f));
    }

    // Espada: o HUD agrupa os elementos de D-pad/espada em mpButtonCrossParent.
    // Midna: mpButtonMidona e o pane dedicado do retrato/atalho.
    // Sword candidate transforms are intentionally disabled in this inspector build.

    // v0.8.5: os quatro botoes de calibracao vieram da mesma folha e foram
    // normalizados para canvases identicos. Nenhuma compensacao especial em X/Y.

    // Normaliza a escala para que A/B nao mantenham o 1.1 vanilla enquanto X/Y usam 1.0.
    // Isso deixa o diametro aparente dos quatro botoes consistente.
    { float sc=cfg_scale(g_crossScale,1.45f); meter->mpButtonA->scale(sc,sc); }
    { float sc=cfg_scale(g_circleScale,1.45f); meter->mpButtonB->scale(sc,sc); }
    { float sc=cfg_scale(layout_handle4(g_squareScale,g_swapSquareScale,g_ybSquareScale,g_xyxbSquareScale),1.45f); meter->mpButtonXY[0]->scale(sc,sc); }
    { float sc=cfg_scale(layout_handle4(g_triScale,g_swapTriScale,g_ybTriScale,g_xyxbTriScale),1.45f); meter->mpButtonXY[1]->scale(sc,sc); }

    // Losango base da v0.6.17. Each GC slot selects its normal or swapped profile.
    set_bounds(y,
               cfg_pos(layout_handle4(g_triX,g_swapTriX,g_ybTriX,g_xyxbTriX),124.0f),
               cfg_pos(layout_handle4(g_triY,g_swapTriY,g_ybTriY,g_xyxbTriY),7.2f),
               24.0f,24.0f);
    set_bounds(x,
               cfg_pos(layout_handle4(g_squareX,g_swapSquareX,g_ybSquareX,g_xyxbSquareX),89.6f),
               cfg_pos(layout_handle4(g_squareY,g_swapSquareY,g_ybSquareY,g_xyxbSquareY),40.6f),
               24.0f,24.0f);
    set_bounds(b, cfg_pos(layout_handle4(g_circleX,g_circleX,g_ybCircleX,g_xyxbCircleX),151.5f), cfg_pos(layout_handle4(g_circleY,g_circleY,g_ybCircleY,g_xyxbCircleY),39.4f), 24.0f, 24.0f); // Circle
    set_bounds(a, cfg_pos(g_crossX,118.0f), cfg_pos(g_crossY,65.7f), 24.0f, 24.0f); // Cross

    // Z/XY2 permanece totalmente vanilla nesta versao.

    if (!s_logged && svc_log != nullptr) {
        svc_log->info(mod_ctx,
            "TP Classic Modern Controller UI v1.0.0 - Item Wheel controller prompt replacements - by XandasLegend");
        s_logged = true;
    }

    s_activeMeter = nullptr;

    // A/B position is already handled in PaneTransHook using the measured
    // local-space offsets. Do not call paneTrans() again here: doing so uses
    // editor coordinates as raw pane coordinates and displaces Cross/Circle.
}

void free_resources() {
    if (svc_resource == nullptr) return;
    for(auto& texture:s_controllerTextures) {
        svc_resource->free(mod_ctx,&texture.xbox);
        svc_resource->free(mod_ctx,&texture.switchTexture);
    }
    svc_resource->free(mod_ctx, &s_cross);
    svc_resource->free(mod_ctx, &s_circle);
    svc_resource->free(mod_ctx, &s_square);
    svc_resource->free(mod_ctx, &s_triangle);
    svc_resource->free(mod_ctx, &s_guide);
    svc_resource->free(mod_ctx, &s_r1);
    svc_resource->free(mod_ctx, &s_r1_hud);
    svc_resource->free(mod_ctx, &s_analog);
    svc_resource->free(mod_ctx, &s_animated_analog_base);
    svc_resource->free(mod_ctx, &s_skill_l3);
    svc_resource->free(mod_ctx, &s_shop_l3_right);
    svc_resource->free(mod_ctx, &s_r3);
    svc_resource->free(mod_ctx, &s_l2);
    svc_resource->free(mod_ctx, &s_r2);
    svc_resource->free(mod_ctx, &s_options);
    svc_resource->free(mod_ctx, &s_menu_ornament);
    svc_resource->free(mod_ctx, &s_hud_ornament);
    svc_resource->free(mod_ctx, &s_dpad);
}

} // namespace

extern "C" {


MOD_EXPORT

J2DPicture* first_picture_recursive(J2DPane* root) {
    if (root == nullptr) return nullptr;
    if (J2DPicture* pic = as_picture(root)) return pic;
    for (J2DPane* child = root->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        if (J2DPicture* pic = first_picture_recursive(child)) return pic;
    }
    return nullptr;
}

void apply_menu_button_texture(J2DPane* root, const ResTIMG* texture) {
    if (root == nullptr || texture == nullptr) return;

    // The file-select A/B roots contain the vanilla button artwork as several
    // layered J2DPictures. Replacing only child #0 leaves the letter/base above
    // our texture. Use the same "single full button picture" strategy as HUD:
    // replace the first picture, neutralize it, and hide every other picture
    // in this button subtree.
    J2DPicture* face = first_picture_recursive(root);
    if (face == nullptr) return;

    replace_picture_texture(face, texture);
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    face->setBlackWhite(neutralBlack, neutralWhite);
    face->setCornerColor(neutralWhite);
    face->show();

    // Hide all other pictures recursively, preserving the root pane itself,
    // its position, alpha and menu animation.
    J2DPane* stack[64];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        J2DPane* node = stack[--top];
        for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (top < 64) stack[top++] = child;
            J2DPicture* pic = as_picture(child);
            if (pic != nullptr && pic != face) pic->hide();
        }
    }
}

// Shared Collection-family A/B prompts are also draw-local. Twilit Essentials
// extends the vanilla Collection J2DScreen, so leaving our hidden layers or
// replacement TIMGs attached after draw can affect its added pages on later frames.
struct SharedPromptTempPaneState {
    J2DPane* pane=nullptr;
    bool visible=false;
    u8 alpha=255;
    bool picture=false;
    JGeometry::TBox2<f32> bounds{};
    f32 tx=0.0f, ty=0.0f, sx=1.0f, sy=1.0f, rotation=0.0f;
    const ResTIMG* tex0=nullptr;
    const ResTIMG* tex1=nullptr;
    JUtility::TColor black{};
    JUtility::TColor white{};
    JUtility::TColor corners[4]{};
    JGeometry::TVec2<s16> texCoords[4]{};
};

SharedPromptTempPaneState s_sharedPromptTemp[96];
int s_sharedPromptTempCount=0;
J2DScreen* s_sharedPromptTempScreen=nullptr;

void capture_shared_prompt_pane(J2DPane* pane) {
    if(pane==nullptr) return;
    for(int i=0;i<s_sharedPromptTempCount;++i)
        if(s_sharedPromptTemp[i].pane==pane) return;
    if(s_sharedPromptTempCount >= (int)(sizeof(s_sharedPromptTemp)/sizeof(s_sharedPromptTemp[0]))) return;

    auto& st=s_sharedPromptTemp[s_sharedPromptTempCount++];
    st.pane=pane;
    st.visible=pane->isVisible();
    st.alpha=pane->getAlpha();
    st.tx=pane->getTranslateX(); st.ty=pane->getTranslateY();
    st.sx=pane->getScaleX(); st.sy=pane->getScaleY();
    st.rotation=pane->getRotateZ();

    if(J2DPicture* pic=as_picture(pane)) {
        st.picture=true;
        st.bounds=pic->mBounds;
        if(pic->getTexture(0)!=nullptr) st.tex0=pic->getTexture(0)->getTexInfo();
        if(pic->getTexture(1)!=nullptr) st.tex1=pic->getTexture(1)->getTexInfo();
        st.black=pic->getBlack(); st.white=pic->getWhite();
        for(int i=0;i<4;++i) st.corners[i]=pic->corner(i);
        copy_picture_texcoords(pic,st.texCoords);
    }
}

void capture_shared_prompt_tree(J2DPane* root) {
    if(root==nullptr) return;
    J2DPane* stack[64]; int top=0; stack[top++]=root;
    while(top>0) {
        J2DPane* node=stack[--top];
        capture_shared_prompt_pane(node);
        for(J2DPane* child=node->getFirstChildPane(); child!=nullptr; child=child->getNextChildPane())
            if(top<64) stack[top++]=child;
    }
}

void begin_shared_prompt_temp_state(J2DScreen* screen) {
    s_sharedPromptTempScreen=screen;
    s_sharedPromptTempCount=0;
    if(screen==nullptr) return;
    static const u64 roots[]={
        MULTI_CHAR('g_abtn_n'),MULTI_CHAR('abtn_n1'),MULTI_CHAR('abtn_n'),
        MULTI_CHAR('g_bbtn_n'),MULTI_CHAR('bbtn_n1'),MULTI_CHAR('bbtn_n'),
    };
    for(u64 tag:roots) capture_shared_prompt_tree(screen->search(tag));
}

void restore_shared_prompt_temp_state(J2DScreen* screen) {
    if(screen==nullptr || screen!=s_sharedPromptTempScreen) return;
    for(int i=s_sharedPromptTempCount-1;i>=0;--i) {
        auto& st=s_sharedPromptTemp[i];
        if(st.pane==nullptr) continue;
        if(st.picture) {
            J2DPicture* pic=static_cast<J2DPicture*>(st.pane);
            if(st.tex0!=nullptr && pic->getTextureCount()>0) pic->changeTexture(st.tex0,0);
            if(st.tex1!=nullptr && pic->getTextureCount()>1) pic->changeTexture(st.tex1,1);
            pic->mBounds=st.bounds;
            restore_picture_texcoords(pic,st.texCoords);
            pic->setBlackWhite(st.black,st.white);
            pic->setCornerColor(st.corners[0],st.corners[1],st.corners[2],st.corners[3]);
        }
        st.pane->translate(st.tx,st.ty);
        st.pane->scale(st.sx,st.sy);
        st.pane->rotate(st.rotation);
        st.pane->setAlpha(st.alpha);
        if(st.visible) st.pane->show(); else st.pane->hide();
    }
    s_sharedPromptTempCount=0;
    s_sharedPromptTempScreen=nullptr;
}

// Item Wheel changes must be draw-local. Twilit Essentials keeps/copies UI
// resources across its radial menus; leaving our texture/bounds/visibility edits
// on the vanilla Item Wheel after a draw can poison those later copies.
struct ItemWheelTempPaneState {
    J2DPane* pane = nullptr;
    bool visible = false;
    u8 alpha = 255;
    bool picture = false;
    JGeometry::TBox2<f32> bounds{};
    f32 tx = 0.0f, ty = 0.0f, sx = 1.0f, sy = 1.0f, rotation = 0.0f;
    const ResTIMG* tex0 = nullptr;
    const ResTIMG* tex1 = nullptr;
    JUTTexture* originalTexture[2]{};
    JUTTexture* privateTexture[2]{};
    u8 originalTextureCount = 0;
    JUtility::TColor black{};
    JUtility::TColor white{};
    JUtility::TColor corners[4]{};
    JGeometry::TVec2<s16> texCoords[4]{};
};

ItemWheelTempPaneState s_itemWheelTemp[128];
int s_itemWheelTempCount = 0;
J2DScreen* s_itemWheelTempScreen = nullptr;

void capture_item_wheel_temp_pane(J2DPane* pane) {
    if (pane == nullptr) return;
    for (int i = 0; i < s_itemWheelTempCount; ++i) {
        if (s_itemWheelTemp[i].pane == pane) return;
    }
    if (s_itemWheelTempCount >= (int)(sizeof(s_itemWheelTemp)/sizeof(s_itemWheelTemp[0]))) return;

    ItemWheelTempPaneState& st = s_itemWheelTemp[s_itemWheelTempCount++];
    st.pane = pane;
    st.visible = pane->isVisible();
    st.alpha = pane->getAlpha();
    st.tx = pane->getTranslateX();
    st.ty = pane->getTranslateY();
    st.sx = pane->getScaleX();
    st.sy = pane->getScaleY();
    st.rotation = pane->getRotateZ();

    if (J2DPicture* pic = as_picture(pane)) {
        st.picture = true;
        st.bounds = pic->mBounds;
        if (pic->getTexture(0) != nullptr) {
            st.tex0 = pic->getTexture(0)->getTexInfo();
            st.originalTexture[0] = pic->getTexture(0);
        }
        if (pic->getTexture(1) != nullptr) {
            st.tex1 = pic->getTexture(1)->getTexInfo();
            st.originalTexture[1] = pic->getTexture(1);
        }
        st.privateTexture[0] = nullptr;
        st.privateTexture[1] = nullptr;
        st.originalTextureCount = pic->getTextureCount();
        st.black = pic->getBlack();
        st.white = pic->getWhite();
        for (int i = 0; i < 4; ++i) st.corners[i] = pic->corner(i);
        copy_picture_texcoords(pic, st.texCoords);
    }
}

void capture_item_wheel_temp_tree(J2DPane* root) {
    if (root == nullptr) return;
    J2DPane* stack[128];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        J2DPane* node = stack[--top];
        capture_item_wheel_temp_pane(node);
        for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (top < 128) stack[top++] = child;
        }
    }
}

ItemWheelTempPaneState* item_wheel_temp_state_for(J2DPicture* pic) {
    if (pic == nullptr) return nullptr;
    for (int i = 0; i < s_itemWheelTempCount; ++i) {
        if (s_itemWheelTemp[i].pane == pic) return &s_itemWheelTemp[i];
    }
    return nullptr;
}

bool use_private_item_wheel_texture(J2DPicture* pic, const ResTIMG* texture) {
    if (pic == nullptr || texture == nullptr) return false;
    ItemWheelTempPaneState* st = item_wheel_temp_state_for(pic);
    if (st == nullptr || pic->getTextureCount() == 0) return false;

    // Use a single private texture stage. Several vanilla wheel glyphs have two
    // texture stages/blend ratios; feeding our art through those stages can leave
    // the vanilla-looking mask/letter visible even though our geometry changed.
    // A one-stage private picture gives us the exact PS/Xbox artwork while still
    // leaving every original J2DMaterial/JUTTexture object completely untouched.
    if (st->privateTexture[0] == nullptr) {
        st->privateTexture[0] = JKR_NEW JUTTexture(texture, 0);
        if (st->privateTexture[0] == nullptr) return false;
    }

    picture_texture_slot(pic, 0) = st->privateTexture[0];
    picture_texture_count(pic) = 1;

    if (pic->getTexture(0) != nullptr)
        pic->setTexCoord(pic->getTexture(0), BIND15, MIRROR0, false);
    return true;
}

void begin_item_wheel_temp_state(J2DScreen* screen) {
    s_itemWheelTempScreen = screen;
    s_itemWheelTempCount = 0;

    static const u64 roots[] = {
        MULTI_CHAR('x_btn_n'), MULTI_CHAR('y_btn_n'),
        MULTI_CHAR('l_btn_n'), MULTI_CHAR('gr_btn_n'), MULTI_CHAR('r_btn_n'),
    };
    for (u64 tag : roots) capture_item_wheel_temp_tree(screen->search(tag));

    static const u64 exactPics[] = {
        MULTI_CHAR('cbtn1'), MULTI_CHAR('cbtn3'), MULTI_CHAR('cbtn'), MULTI_CHAR('cbtn2'),
        MULTI_CHAR('cbtn4'), MULTI_CHAR('cbtn5'), MULTI_CHAR('cbtn6'), MULTI_CHAR('cbtn7'),
    };
    for (u64 tag : exactPics) capture_item_wheel_temp_pane(screen->search(tag));
}

void restore_item_wheel_temp_state(J2DScreen* screen) {
    if (screen == nullptr || screen != s_itemWheelTempScreen) return;

    for (int i = s_itemWheelTempCount - 1; i >= 0; --i) {
        ItemWheelTempPaneState& st = s_itemWheelTemp[i];
        if (st.pane == nullptr) continue;

        if (st.picture) {
            J2DPicture* pic = static_cast<J2DPicture*>(st.pane);

            // Restore the exact material texture objects first. The private
            // replacements are deleted only after the picture no longer points
            // at them, so no shared J2DMaterial/JUTTexture is ever modified.
            picture_texture_count(pic) = st.originalTextureCount;
            for (u8 t = 0; t < 2; ++t) {
                if (st.privateTexture[t] != nullptr) {
                    picture_texture_slot(pic, t) = st.originalTexture[t];
                    JKR_DELETE(st.privateTexture[t]);
                    st.privateTexture[t] = nullptr;
                }
            }

            // Restore the exact local geometry instead of calling move()/place().
            // Those helpers recalculate translation (and place() can move children),
            // which was able to accumulate drift across repeated menu opens.
            pic->mBounds = st.bounds;
            pic->translate(st.tx, st.ty);
            pic->scale(st.sx, st.sy);
            pic->rotate(st.rotation);
            restore_picture_texcoords(pic, st.texCoords);
            pic->setBlackWhite(st.black, st.white);
            pic->setCornerColor(st.corners[0], st.corners[1], st.corners[2], st.corners[3]);
        } else {
            st.pane->translate(st.tx, st.ty);
            st.pane->scale(st.sx, st.sy);
            st.pane->rotate(st.rotation);
        }

        st.pane->setAlpha(st.alpha);
        if (st.visible) st.pane->show();
        else st.pane->hide();
    }

    s_itemWheelTempCount = 0;
    s_itemWheelTempScreen = nullptr;
}

// Item Wheel controller icons are authored on square canvases. The original
// GameCube panes are not square (especially L/R), so simply swapping the BTI
// stretches the new artwork. Fit the visible replacement picture into a square
// centered on the vanilla picture and force neutral material/alpha.
struct ItemWheelIconBase {
    J2DPicture* picture = nullptr;
    JGeometry::TBox2<f32> base{};
    float lastDx = 0.0f, lastDy = 0.0f, lastScale = 1.0f;
    bool captured = false;
};
ItemWheelIconBase s_wheelSquareBase{}, s_wheelTriangleBase{};
ItemWheelIconBase s_wheelSelectAnalogBase{}, s_wheelDirectAnalogBase{}, s_wheelL2Base{};
ItemWheelIconBase s_wheelR2ComboBase{}, s_wheelR2AltBase{};

void apply_item_wheel_icon_texture(J2DPane* root, const ResTIMG* texture,
                                   ItemWheelIconBase& state,
                                   ConfigVarHandle xh, ConfigVarHandle yh, ConfigVarHandle sh) {
    if (root == nullptr || texture == nullptr) return;
    J2DPicture* face = first_picture_recursive(root);
    if (face == nullptr) return;

    // Rebuild from the live vanilla/local geometry on every draw. The wheel is
    // destroyed and recreated whenever the item menu closes; the allocator can
    // reuse the same J2DPicture address, so pointer-based cached baselines can
    // accidentally survive into a new wheel instance and compound each reopen.
    const JGeometry::TBox2<f32> base = face->mBounds;
    const float baseTx = face->getTranslateX();
    const float baseTy = face->getTranslateY();
    const float w = base.getWidth();
    const float h = base.getHeight();
    const float side = (w < h ? w : h);
    const float localCx = base.i.x + w * 0.5f;
    const float localCy = base.i.y + h * 0.5f;

    if (!use_private_item_wheel_texture(face, texture)) return;
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    face->setBlackWhite(neutralBlack, neutralWhite);
    face->setCornerColor(neutralWhite);
    face->setAlpha(255);

    const float dx = cfg_pos(xh, 0.0f);
    const float dy = cfg_pos(yh, 0.0f);
    const float sc = cfg_scale(sh, 1.0f);
    const float fitted = side * sc;

    // Never use move()/resize()/place() for the draw-local wheel replacement:
    // place() adjusts child translations, which is exactly the kind of state
    // that can accumulate across repeated wheel opens. Change only local bounds
    // and local translation, then restore the complete subtree after draw.
    face->mBounds.i.x = localCx - fitted * 0.5f;
    face->mBounds.i.y = localCy - fitted * 0.5f;
    face->mBounds.f.x = localCx + fitted * 0.5f;
    face->mBounds.f.y = localCy + fitted * 0.5f;
    face->translate(baseTx + dx, baseTy + dy);
    if (face->getTexture(0) != nullptr)
        face->setTexCoord(face->getTexture(0), BIND15, MIRROR0, false);
    face->show();

    // Keep the legacy state object only for ABI/source compatibility with the
    // existing call sites. It is intentionally not used as a persistent base.
    state.picture = face;
    state.base = base;
    state.captured = false;

    J2DPane* stack[64];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        J2DPane* node = stack[--top];
        for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (top < 64) stack[top++] = child;
            if (J2DPicture* pic = as_picture(child); pic != nullptr && pic != face) pic->hide();
        }
    }
}

void apply_item_wheel_shoulder_texture(J2DPane* root, const ResTIMG* texture,
                                       ItemWheelIconBase& state,
                                       ConfigVarHandle xh, ConfigVarHandle yh, ConfigVarHandle sh) {
    if (root == nullptr || texture == nullptr) return;
    J2DPicture* face = first_picture_recursive(root);
    if (face == nullptr) return;

    const JGeometry::TBox2<f32> base = face->mBounds;
    const float baseTx = face->getTranslateX();
    const float baseTy = face->getTranslateY();
    const float vanillaW = base.getWidth();
    const float vanillaH = base.getHeight();
    const float localCx = base.i.x + vanillaW * 0.5f;
    const float localCy = base.i.y + vanillaH * 0.5f;
    const float aspect = texture->height != 0
        ? ((float)texture->width / (float)texture->height) : 1.0f;

    if (!use_private_item_wheel_texture(face, texture)) return;
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    face->setBlackWhite(neutralBlack, neutralWhite);
    face->setCornerColor(neutralWhite);
    face->setAlpha(255);

    const float dx = cfg_pos(xh, 0.0f);
    const float dy = cfg_pos(yh, 0.0f);
    const float sc = cfg_scale(sh, 1.0f);
    const float fittedH = vanillaH * sc;
    const float fittedW = fittedH * aspect;

    face->mBounds.i.x = localCx - fittedW * 0.5f;
    face->mBounds.i.y = localCy - fittedH * 0.5f;
    face->mBounds.f.x = localCx + fittedW * 0.5f;
    face->mBounds.f.y = localCy + fittedH * 0.5f;
    face->translate(baseTx + dx, baseTy + dy);
    if (face->getTexture(0) != nullptr)
        face->setTexCoord(face->getTexture(0), BIND15, MIRROR0, false);
    face->show();

    state.picture = face;
    state.base = base;
    state.captured = false;

    J2DPane* stack[64];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        J2DPane* node = stack[--top];
        for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (top < 64) stack[top++] = child;
            if (J2DPicture* pic = as_picture(child); pic != nullptr && pic != face) pic->hide();
        }
    }
}

// Replace one exact Item Wheel picture and optionally hide the other layers that
// formed the original GameCube C-stick artwork. Unlike the root helper above,
// this never touches neighbouring text, plus signs, or shoulder-button groups.
void apply_item_wheel_exact_picture(J2DScreen* screen, u64 faceTag,
                                    const u64* hideTags, int hideCount,
                                    const ResTIMG* texture, ItemWheelIconBase& state,
                                    ConfigVarHandle xh, ConfigVarHandle yh, ConfigVarHandle sh) {
    if (screen == nullptr || texture == nullptr) return;
    J2DPane* pane = screen->search(faceTag);
    J2DPicture* face = as_picture(pane);
    if (face == nullptr) return;

    const JGeometry::TBox2<f32> base = face->mBounds;
    const float baseTx = face->getTranslateX();
    const float baseTy = face->getTranslateY();
    const float w = base.getWidth();
    const float h = base.getHeight();
    const float side = (w > h ? w : h);
    const float localCx = base.i.x + w * 0.5f;
    const float localCy = base.i.y + h * 0.5f;

    if (!use_private_item_wheel_texture(face, texture)) return;
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    face->setBlackWhite(neutralBlack, neutralWhite);
    face->setCornerColor(neutralWhite);
    face->setAlpha(255);
    face->show();

    const float dx = cfg_pos(xh, 0.0f);
    const float dy = cfg_pos(yh, 0.0f);
    const float sc = cfg_scale(sh, 1.0f);
    const float fitted = side * sc;
    face->mBounds.i.x = localCx - fitted * 0.5f;
    face->mBounds.i.y = localCy - fitted * 0.5f;
    face->mBounds.f.x = localCx + fitted * 0.5f;
    face->mBounds.f.y = localCy + fitted * 0.5f;
    face->translate(baseTx + dx, baseTy + dy);
    if (face->getTexture(0) != nullptr)
        face->setTexCoord(face->getTexture(0), BIND15, MIRROR0, false);

    state.picture = face;
    state.base = base;
    state.captured = false;

    for (int i = 0; i < hideCount; ++i) {
        J2DPane* other = screen->search(hideTags[i]);
        if (other != nullptr && other != face) other->hide();
    }
}

// Match only the visible replacement picture size, preserving the target
// root pane position/animation. Used for save-screen Circle vs Cross parity.
void match_menu_button_picture_size(J2DPane* targetRoot, J2DPane* referenceRoot) {
    J2DPicture* target = first_picture_recursive(targetRoot);
    J2DPicture* reference = first_picture_recursive(referenceRoot);
    if (target == nullptr || reference == nullptr) return;

    const auto& tb = target->getBounds();
    const auto& rb = reference->getBounds();
    const float cx = tb.i.x + tb.getWidth() * 0.5f;
    const float cy = tb.i.y + tb.getHeight() * 0.5f;
    const float w = rb.getWidth();
    const float h = rb.getHeight();
    set_bounds(target, cx - w * 0.5f, cy - h * 0.5f, w, h);
}

struct MenuButtonBase {
    J2DPicture* picture = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};
MenuButtonBase s_fileCrossBase, s_fileCircleBase, s_saveCrossBase, s_saveCircleBase;

void apply_menu_button_offset(J2DPane* root, MenuButtonBase& base,
                              ConfigVarHandle xh, ConfigVarHandle yh) {
    J2DPicture* picture = first_picture_recursive(root);
    if (picture == nullptr) return;
    if (base.picture != picture) {
        const auto& b = picture->getBounds();
        base.picture = picture;
        base.x = b.i.x;
        base.y = b.i.y;
        base.w = b.getWidth();
        base.h = b.getHeight();
    }
    set_bounds(picture,
               base.x + cfg_pos(xh, 0.0f),
               base.y + cfg_pos(yh, 0.0f),
               base.w, base.h);
}

// File-select button icons use their own layout panes. Reuse the exact same
// Cross/Circle ResourceBuffers already loaded for the gameplay HUD.
// No message/font resources are touched.
HookAction before_file_select_draw(ModContext* ctx, void* args, void* retval, void* userdata) {
    dDlst_FileSel_c* dlst = mods::arg<dDlst_FileSel_c*>(args, 0);
    if (dlst == nullptr || dlst->Scr == nullptr) return HOOK_CONTINUE;

    // These are the exact A/B button roots used by dFile_select_c::_create().
    J2DPane* aRoot = dlst->Scr->search(MULTI_CHAR('w_n_abtn'));
    J2DPane* bRoot = dlst->Scr->search(MULTI_CHAR('w_n_bbtn'));

    apply_menu_button_texture(aRoot, resource_timg(s_cross));   // A -> Cross
    apply_menu_button_texture(bRoot, gc_b_face_texture());  // B -> Circle
    // Initial file-select screen: make Circle visually the same size as Cross.
    match_menu_button_picture_size(bRoot, aRoot);
    // Position controls affect only the replacement pictures, not Confirmar/Voltar.
    apply_menu_button_offset(aRoot, s_fileCrossBase, g_fileCrossX, g_fileCrossY);
    apply_menu_button_offset(bRoot, s_fileCircleBase, g_fileCircleX, g_fileCircleY);
    return HOOK_CONTINUE;
}



void after_outfont_create_pane(ModContext* ctx, void* args, void* retval, void* userdata) {
    COutFont_c* outFont = mods::arg<COutFont_c*>(args, 0);
    if (outFont == nullptr) return;

    const ResTIMG* cross = resource_timg(s_cross);
    const ResTIMG* circle = gc_b_face_texture();
    if (cross == nullptr || circle == nullptr) return;

    // COutFont icon 0 = A (font_00.bti), icon 1 = B (font_01.bti).
    // Icon 2 is font_09.bti: the concentric-stick glyph used by the
    // Options help legend (stick + arrows).  Replace that glyph only;
    // the directional arrows remain the game's own artwork.
    // These are the inline controller glyphs embedded in translated gameplay
    // descriptions and labels such as the TV setup "Pronto A".
    // Replace only their pictures; message strings/fonts are left untouched.
    if (outFont->mpPane[0] != nullptr) {
        replace_picture_texture(outFont->mpPane[0], cross);
        const JUtility::TColor black(0, 0, 0, 0);
        const JUtility::TColor white(255, 255, 255, 255);
        outFont->mpPane[0]->setBlackWhite(black, white);
        outFont->mpPane[0]->setCornerColor(white);
    }
    if (outFont->mpPane[1] != nullptr) {
        replace_picture_texture(outFont->mpPane[1], circle);
        const JUtility::TColor black(0, 0, 0, 0);
        const JUtility::TColor white(255, 255, 255, 255);
        outFont->mpPane[1]->setBlackWhite(black, white);
        outFont->mpPane[1]->setCornerColor(white);
    }
    // Final Technique: COutFont type 9 is a native two-stage animation.
    // stage 0 = dedicated animation base; static s_analog remains unchanged
    // stage 1 = the user-supplied L3 artwork pointing upward
    // Keep the game's original setBlendAnime(), timing and mirror sequence.
    // Type 18 is the vertical up/down animation used by Double Clawshots.
    for (int type : {9, 14, 15, 18}) if (outFont->mpPane[type] != nullptr) {
        J2DPicture* pic = outFont->mpPane[type];
        const ResTIMG* baseAnalog = resource_timg(s_animated_analog_base);
        // Single-direction vertical glyphs rotate their input 90 degrees in draw().
        // Supply horizontal artwork so types 14/15 tilt vertically after that rotation.
        const ResTIMG* skillL3 = resource_timg((type == 14 || type == 15) ? s_shop_l3_right : s_skill_l3);
        if (baseAnalog != nullptr && skillL3 != nullptr && pic->getTextureCount() >= 2) {
            pic->changeTexture(baseAnalog, 0);
            pic->changeTexture(skillL3, 1);
            if (pic->getTexture(0) != nullptr)
                pic->setTexCoord(pic->getTexture(0), BIND15, MIRROR0, false);
            const JUtility::TColor black(0, 0, 0, 0);
            const JUtility::TColor white(255, 255, 255, 255);
            pic->setBlackWhite(black, white);
            pic->setCornerColor(white);
            pic->setAlpha(255);
        }
    }

    // Shop dialogue: COutFont type 17 uses the same native two-stage
    // analog animation pattern as the final Technique. Reuse the proven
    // dedicated base + directional pair and leave the adjacent type 11 arrow intact.
    for (int type : {16, 17, 19}) if (outFont->mpPane[type] != nullptr) {
        J2DPicture* pic = outFont->mpPane[type];
        const ResTIMG* baseAnalog = resource_timg(s_animated_analog_base);
        const ResTIMG* skillL3 = resource_timg(s_shop_l3_right);
        if (baseAnalog != nullptr && skillL3 != nullptr && pic->getTextureCount() >= 2) {
            pic->changeTexture(baseAnalog, 0);
            pic->changeTexture(skillL3, 1);
            if (pic->getTexture(0) != nullptr)
                pic->setTexCoord(pic->getTexture(0), BIND15, MIRROR0, false);
            const JUtility::TColor black(0, 0, 0, 0);
            const JUtility::TColor white(255, 255, 255, 255);
            pic->setBlackWhite(black, white);
            pic->setCornerColor(white);
            pic->setAlpha(255);
        }
    }

    // font_09.bti is the concentric analog-stick glyph used by inline help text,
    // including the Item Wheel Select / Direct Select instructions.
    if (outFont->mpPane[2] != nullptr) {
        if (const ResTIMG* analog = resource_timg(s_analog)) {
            replace_picture_texture(outFont->mpPane[2], analog);
            const JUtility::TColor black(0, 0, 0, 0);
            const JUtility::TColor white(255, 255, 255, 255);
            outFont->mpPane[2]->setBlackWhite(black, white);
            outFont->mpPane[2]->setCornerColor(white);
        }
    }
    for (int type = 3; type <= 4; ++type) {
        const ResTIMG* tex = type == 3 ? resource_timg(s_l2) : resource_timg(s_r2);
        if (outFont->mpPane[type] != nullptr && tex != nullptr) {
            replace_picture_texture(outFont->mpPane[type], tex);
            const JUtility::TColor black(0, 0, 0, 0);
            const JUtility::TColor white(255, 255, 255, 255);
            outFont->mpPane[type]->setBlackWhite(black, white);
            outFont->mpPane[type]->setCornerColor(white);
            outFont->mpPane[type]->setAlpha(255);
        }
    }
}

// Keep modern inline controller glyphs current even for COutFont instances
// whose panes were supplied through setPane() instead of createPane().
// Type mapping from COutFont::getBtiName(): 2=font_09 (stick),
// 3=font_04 (L), 4=font_05 (R).
HookAction before_outfont_draw_font(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    COutFont_c* outFont = mods::arg<COutFont_c*>(args, 0);
    if (outFont == nullptr) return HOOK_CONTINUE;

    const u8 type = mods::arg<u8>(args, 2);

    // Directional inline sticks: preserve native blend/mirror timing, but
    // compensate textbox scaling so the authored square canvas stays square.
    if (type == 9 || (type >= 14 && type <= 19)) {
        float ax = 1.0f, ay = 1.0f;
        for (J2DPane* p = mods::arg<J2DTextBox*>(args, 1); p != nullptr; p = p->getParentPane()) {
            ax *= p->getScaleX(); ay *= p->getScaleY();
        }
        if (ax > 0.0001f && ay > 0.0001f) {
            float& x = mods::arg_ref<f32>(args, 3);
            float& y = mods::arg_ref<f32>(args, 4);
            float& w = mods::arg_ref<f32>(args, 5);
            float& h = mods::arg_ref<f32>(args, 6);
            const float side = w * ax > h * ay ? w * ax : h * ay;
            x -= (side / ax - w) * 0.5f; y -= (side / ay - h) * 0.5f;
            w = side / ax; h = side / ay;
        }
    }
    // Type 9: preserve its native two-texture blend. Do not use
    // replace_picture_texture() here because that intentionally replaces every
    // stage and would make both animation stages identical.
    if ((type == 9 || type == 14 || type == 15 || type == 18) && outFont->mpPane[type] != nullptr) {
        J2DPicture* pic = outFont->mpPane[type];
        const ResTIMG* baseAnalog = resource_timg(s_animated_analog_base);
        // Single-direction vertical glyphs rotate their input 90 degrees in draw().
        // Supply horizontal artwork so types 14/15 tilt vertically after that rotation.
        const ResTIMG* skillL3 = resource_timg((type == 14 || type == 15) ? s_shop_l3_right : s_skill_l3);
        if (baseAnalog != nullptr && skillL3 != nullptr && pic->getTextureCount() >= 2) {
            pic->changeTexture(baseAnalog, 0);
            pic->changeTexture(skillL3, 1);
            if (pic->getTexture(0) != nullptr)
                pic->setTexCoord(pic->getTexture(0), BIND15, MIRROR0, false);
            const JUtility::TColor neutralBlack(0, 0, 0, 0);
            const JUtility::TColor neutralWhite(255, 255, 255, 255);
            pic->setBlackWhite(neutralBlack, neutralWhite);
            pic->setCornerColor(neutralWhite);
            pic->setAlpha(255);
        }
        return HOOK_CONTINUE;
    }

    // Type 17: shop-dialogue analog prompt. Keep its native animation and
    // replace only its two texture stages. Type 11 (the arrow) is untouched.
    if ((type == 16 || type == 17 || type == 19) && outFont->mpPane[type] != nullptr) {
        J2DPicture* pic = outFont->mpPane[type];
        const ResTIMG* baseAnalog = resource_timg(s_animated_analog_base);
        const ResTIMG* skillL3 = resource_timg(s_shop_l3_right);
        if (baseAnalog != nullptr && skillL3 != nullptr && pic->getTextureCount() >= 2) {
            pic->changeTexture(baseAnalog, 0);
            pic->changeTexture(skillL3, 1);
            if (pic->getTexture(0) != nullptr)
                pic->setTexCoord(pic->getTexture(0), BIND15, MIRROR0, false);
            const JUtility::TColor neutralBlack(0, 0, 0, 0);
            const JUtility::TColor neutralWhite(255, 255, 255, 255);
            pic->setBlackWhite(neutralBlack, neutralWhite);
            pic->setCornerColor(neutralWhite);
            pic->setAlpha(255);
        }
        return HOOK_CONTINUE;
    }

    const ResTIMG* replacement = nullptr;
    if (type == 1) replacement = gc_b_face_texture(); // native GC B / Back slot
    else if (type == 2) replacement = resource_timg(s_analog);
    else if (type == 3) replacement = resource_timg(s_l2);
    else if (type == 4) replacement = resource_timg(s_r2);
    // Inline item descriptions: native X/Y glyphs use font_02/font_03.
    else if (type == 5) replacement = gc_x_face_texture(); // native GC X slot
    else if (type == 6) replacement = gc_y_face_texture(); // native GC Y slot
    else if (type == 7) replacement = resource_timg(s_r1); // Z -> R1
    else if (type == 8) replacement = resource_timg(s_dpad); // D-pad
    if (replacement == nullptr || outFont->mpPane[type] == nullptr) return HOOK_CONTINUE;

    J2DPicture* pic = outFont->mpPane[type];
    replace_picture_texture(pic, replacement);
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    pic->setBlackWhite(neutralBlack, neutralWhite);
    pic->setCornerColor(neutralWhite);
    pic->setAlpha(255);

    // All replacement BTIs use a square canvas. Make the draw box square while
    // preserving its center; L2/R2 artwork itself remains wide inside that canvas.
    float& px = mods::arg_ref<f32>(args, 3);
    float& py = mods::arg_ref<f32>(args, 4);
    float& sx = mods::arg_ref<f32>(args, 5);
    float& sy = mods::arg_ref<f32>(args, 6);
    if (type == 7 || type == 8) {
        float ax = 1.0f, ay = 1.0f;
        for (J2DPane* p = mods::arg<J2DTextBox*>(args, 1); p != nullptr; p = p->getParentPane()) {
            ax *= p->getScaleX(); ay *= p->getScaleY();
        }
        if (ax > 0.0001f && ay > 0.0001f && replacement->height != 0) {
            const float aspect = float(replacement->width) / float(replacement->height);
            // Fit inside the glyph slot so the wide R1 cannot overlap text.
            const float width = sx * ax < sy * ay * aspect ? sx * ax : sy * ay * aspect;
            const float w = width / ax, h = width / aspect / ay;
            px += (sx - w) * 0.5f; py += (sy - h) * 0.5f;
            sx = w; sy = h;
        }
        return HOOK_CONTINUE;
    }
    if (type==1) {
        // Native B/font_01 does not use the X/Y height trim.  Normalize in
        // final screen pixels only; sharing the X/Y compensation made B oval
        // on Technique/Battle tutorial text.
        float scaleX=1.0f,scaleY=1.0f;
        for (J2DPane* pane=mods::arg<J2DTextBox*>(args,1);pane!=nullptr;pane=pane->getParentPane()) {
            scaleX*=pane->getScaleX(); scaleY*=pane->getScaleY();
        }
        if (scaleX>0.0001f && scaleY>0.0001f) {
            const float width=sx*scaleX;
            const float height=sy*scaleY;
            const float side=width>height?width:height;
            px-=(side-width)*0.5f/scaleX;
            py-=(side-height)*0.5f/scaleY;
            sx=side/scaleX;
            sy=side/scaleY;
        }
        return HOOK_CONTINUE;
    }
    if (type==5 || type==6) {
        // COutFont::draw applies the textbox ancestry scale, then subtracts
        // 2px (JPN) or 3px from X/Y glyph height. Compensate in final pixels.
        float scaleX=1.0f,scaleY=1.0f;
        for (J2DPane* pane=mods::arg<J2DTextBox*>(args,1);pane!=nullptr;pane=pane->getParentPane()) {
            scaleX*=pane->getScaleX(); scaleY*=pane->getScaleY();
        }
        if (scaleX>0.0001f && scaleY>0.0001f) {
            const DVDDiskID* disk=DVDGetCurrentDiskID();
            const float trim=(disk!=nullptr && disk->gameName[3]=='J')?2.0f:3.0f;
            const float width=sx*scaleX;
            const float height=sy*scaleY-trim;
            const float side=width>height?width:height;
            px-=(side-width)*0.5f/scaleX;
            py-=(side-height)*0.5f/scaleY;
            sx=side/scaleX;
            sy=(side+trim)/scaleY;
        }
        return HOOK_CONTINUE;
    }
    const float side = (sx > sy ? sx : sy);
    px -= (side - sx) * 0.5f;
    py -= (side - sy) * 0.5f;
    sx = side;
    sy = side;
    return HOOK_CONTINUE;
}

struct MenuPromptDrawState {
    J2DPane* pane=nullptr;
    float x=0,y=0,sx=1,sy=1;
};
MenuPromptDrawState s_menuPromptDrawState[64];
int s_menuPromptDrawStateCount=0;
J2DScreen* s_menuPromptDrawScreen=nullptr;

void begin_menu_prompt_draw(J2DScreen* screen) {
    // Prompt transforms are draw-local. The game must always receive its vanilla
    // pane transforms back after J2DScreen::draw so Collection/Options/Map can
    // freely reuse or rebuild the same layout without inheriting our edits.
    s_menuPromptDrawScreen=screen;
    s_menuPromptDrawStateCount=0;
}

void capture_menu_prompt_pane(J2DPane* pane) {
    if (pane==nullptr) return;
    for (int i=0;i<s_menuPromptDrawStateCount;i++) {
        if (s_menuPromptDrawState[i].pane==pane) return;
    }
    if (s_menuPromptDrawStateCount >= (int)(sizeof(s_menuPromptDrawState)/sizeof(s_menuPromptDrawState[0]))) return;
    MenuPromptDrawState& st=s_menuPromptDrawState[s_menuPromptDrawStateCount++];
    st.pane=pane;
    st.x=pane->getTranslateX(); st.y=pane->getTranslateY();
    st.sx=pane->getScaleX(); st.sy=pane->getScaleY();
}

void apply_menu_pane_transform(J2DPane* pane, ConfigVarHandle xh, ConfigVarHandle yh, ConfigVarHandle sh) {
    if (pane==nullptr) return;
    capture_menu_prompt_pane(pane);
    MenuPromptDrawState* st=nullptr;
    for (int i=0;i<s_menuPromptDrawStateCount;i++) if (s_menuPromptDrawState[i].pane==pane) { st=&s_menuPromptDrawState[i]; break; }
    if (st==nullptr) return;
    const float f=cfg_scale(sh,1.0f);
    pane->translate(st->x+cfg_pos(xh,0.0f), st->y+cfg_pos(yh,0.0f));
    pane->scale(st->sx*f, st->sy*f);
}

void restore_menu_prompt_after_draw(J2DScreen* screen) {
    if (screen==nullptr || screen!=s_menuPromptDrawScreen) return;
    for (int i=s_menuPromptDrawStateCount-1;i>=0;i--) {
        MenuPromptDrawState& st=s_menuPromptDrawState[i];
        if (st.pane==nullptr) continue;
        st.pane->translate(st.x,st.y);
        st.pane->scale(st.sx,st.sy);
    }
    s_menuPromptDrawStateCount=0;
    s_menuPromptDrawScreen=nullptr;
}
enum class PromptMapKind { None, World, Dungeon };

PromptMapKind prompt_map_kind(J2DScreen* screen) {
    if (screen==nullptr) return PromptMapKind::None;
    // World-map title layout has the Z prompt; dungeon's dedicated prompt
    // layout has a C-stick and its move label. Text tags alone overlap.
    if (screen->search(MULTI_CHAR('zbtn_n1'))!=nullptr &&
        (screen->search(MULTI_CHAR('font_at1'))!=nullptr ||
         screen->search(MULTI_CHAR('cont_at'))!=nullptr)) return PromptMapKind::World;
    if (screen->search(MULTI_CHAR('c_btn'))!=nullptr &&
        (screen->search(MULTI_CHAR('c_text'))!=nullptr ||
         screen->search(MULTI_CHAR('f_text'))!=nullptr) &&
        (screen->search(MULTI_CHAR('font_at'))!=nullptr ||
         screen->search(MULTI_CHAR('cont_at'))!=nullptr)) return PromptMapKind::Dungeon;
    return PromptMapKind::None;
}

J2DPane* map_button_root(J2DScreen* screen, PromptMapKind kind, bool confirm) {
    if (screen==nullptr || kind==PromptMapKind::None) return nullptr;
    // Pick one root, never both an ancestor and its nested variant: applying
    // scale to both multiplies it twice and moves the same artwork twice.
    if (kind==PromptMapKind::World) {
        if (J2DPane* pane=screen->search(confirm ? MULTI_CHAR('abtn_n1') : MULTI_CHAR('bbtn_n1'))) return pane;
    }
    if (J2DPane* pane=screen->search(confirm ? MULTI_CHAR('g_abtn_n') : MULTI_CHAR('g_bbtn_n'))) return pane;
    if (J2DPane* pane=screen->search(confirm ? MULTI_CHAR('abtn_n1') : MULTI_CHAR('bbtn_n1'))) return pane;
    return screen->search(confirm ? MULTI_CHAR('abtn_n') : MULTI_CHAR('bbtn_n'));
}

void apply_shared_menu_prompt_layout(J2DScreen* screen) {
    if (screen==nullptr) return;
    if (prompt_map_kind(screen)!=PromptMapKind::None) return;
    // The Collection family reuses zelda_collect_soubi_do_icon_parts.blo.
    // Its five-layer A/B strings are a reliable signature and keep Save/File Select out of this path.
    const bool shared = screen->search(MULTI_CHAR('atext1_1'))!=nullptr || screen->search(MULTI_CHAR('btext1_1'))!=nullptr;
    if (!shared) return;

    static const u64 aIcons[]={MULTI_CHAR('g_abtn_n'),MULTI_CHAR('abtn_n1'),MULTI_CHAR('abtn_n')};
    static const u64 bIcons[]={MULTI_CHAR('g_bbtn_n'),MULTI_CHAR('bbtn_n1'),MULTI_CHAR('bbtn_n')};

    // Some layouts expose both an outer and an inner alias for the same button.
    // Transforming both multiplies the apparent offset/scale. Pick the first
    // canonical root exactly like map_button_root() does.
    J2DPane* aRoot=nullptr;
    J2DPane* bRoot=nullptr;
    for (u64 t:aIcons) if ((aRoot=screen->search(t))!=nullptr) break;
    for (u64 t:bIcons) if ((bRoot=screen->search(t))!=nullptr) break;
    apply_menu_pane_transform(aRoot,g_menuCrossX,g_menuCrossY,g_menuCrossScale);
    apply_menu_pane_transform(bRoot,g_menuCircleX,g_menuCircleY,g_menuCircleScale);

    if (J2DPane* p=screen->search(MULTI_CHAR('a_text_n'))) apply_menu_pane_transform(p,g_menuConfirmTextX,g_menuConfirmTextY,g_menuConfirmTextScale);
    if (J2DPane* p=screen->search(MULTI_CHAR('b_text_n'))) apply_menu_pane_transform(p,g_menuBackTextX,g_menuBackTextY,g_menuBackTextScale);

}

void apply_map_menu_prompt_layout(J2DScreen* screen) {
    if (screen==nullptr) return;

    const PromptMapKind kind=prompt_map_kind(screen);
    if (kind==PromptMapKind::None) return;
    const bool dungeonMap=kind==PromptMapKind::Dungeon;

    apply_menu_pane_transform(map_button_root(screen,kind,true),(dungeonMap ? g_dungeonMapCrossX : g_mapCrossX),(dungeonMap ? g_dungeonMapCrossY : g_mapCrossY),(dungeonMap ? g_dungeonMapCrossScale : g_mapCrossScale));
    apply_menu_pane_transform(map_button_root(screen,kind,false),(dungeonMap ? g_dungeonMapCircleX : g_mapCircleX),(dungeonMap ? g_dungeonMapCircleY : g_mapCircleY),(dungeonMap ? g_dungeonMapCircleScale : g_mapCircleScale));

    // Both map implementations use five layered text panes. PC builds may use
    // font_* or cont_* depending on the original regional layout. Transform
    // every existing layer so outline/shadow and translated text remain aligned.
    static const u64 aText[]={MULTI_CHAR('font_at'),MULTI_CHAR('font_at1'),MULTI_CHAR('font_at2'),MULTI_CHAR('font_at3'),MULTI_CHAR('font_at4'),MULTI_CHAR('font_at5'),MULTI_CHAR('cont_at'),MULTI_CHAR('cont_at1'),MULTI_CHAR('cont_at2'),MULTI_CHAR('cont_at3'),MULTI_CHAR('cont_at4')};
    static const u64 bText[]={MULTI_CHAR('font_bt'),MULTI_CHAR('font_bt1'),MULTI_CHAR('font_bt2'),MULTI_CHAR('font_bt3'),MULTI_CHAR('font_bt4'),MULTI_CHAR('font_bt5'),MULTI_CHAR('cont_bt'),MULTI_CHAR('cont_bt1'),MULTI_CHAR('cont_bt2'),MULTI_CHAR('cont_bt3'),MULTI_CHAR('cont_bt4'),MULTI_CHAR('cont_bt8')};
    for (u64 t:aText) if (J2DPane* pane=screen->search(t)) apply_menu_pane_transform(pane,(dungeonMap ? g_dungeonMapConfirmTextX : g_mapConfirmTextX),(dungeonMap ? g_dungeonMapConfirmTextY : g_mapConfirmTextY),(dungeonMap ? g_dungeonMapConfirmTextScale : g_mapConfirmTextScale));
    for (u64 t:bText) if (J2DPane* pane=screen->search(t)) apply_menu_pane_transform(pane,(dungeonMap ? g_dungeonMapBackTextX : g_mapBackTextX),(dungeonMap ? g_dungeonMapBackTextY : g_mapBackTextY),(dungeonMap ? g_dungeonMapBackTextScale : g_mapBackTextScale));
}

struct MenuOrnamentDrawState {
    J2DPicture* pane = nullptr;
    bool visible = false;
};
struct MenuOrnamentHostState {
    J2DPicture* pane = nullptr;
    JGeometry::TBox2<f32> bounds{};
    float x=0.0f, y=0.0f, sx=1.0f, sy=1.0f, rotation=0.0f;
    u8 alpha=255;
    const ResTIMG* tex0=nullptr;
    const ResTIMG* tex1=nullptr;
    JUtility::TColor black{};
    JUtility::TColor white{};
    JUtility::TColor corners[4]{};
    JGeometry::TVec2<s16> texCoords[4]{};
};
MenuOrnamentDrawState s_menuOrnamentState[128];
MenuOrnamentHostState s_menuOrnamentHost;
int s_menuOrnamentStateCount=0;
J2DScreen* s_menuOrnamentScreen=nullptr;
bool s_menuOrnamentIsMap=false;

struct OrnamentOrderState {
    J2DPane* pane;
    J2DPane* parent;
    J2DPane* next;
};
OrnamentOrderState s_ornamentOrder[32];
int s_ornamentOrderCount=0;

void move_ornament_behind_prompts(J2DPane* host, J2DScreen* screen) {
    if (host==nullptr || screen==nullptr || s_ornamentOrderCount!=0) return;
    // Validate the complete ancestry before changing any sibling order.
    J2DPane* ancestor=host;
    int depth=0;
    while (ancestor!=nullptr && ancestor!=screen && depth<32) {
        ancestor=ancestor->getParentPane();
        ++depth;
    }
    if (ancestor!=screen) return;
    // J2D draws siblings in list order. Keep each pane under its original
    // parent, preserving local transforms, clipping and inherited alpha.
    for (J2DPane* pane=host; pane!=screen; ) {
        J2DPane* parent=pane->getParentPane();
        J2DPane* first=parent->getFirstChildPane();
        if (first!=pane) {
            J2DPane* next=pane->getNextChildPane();
            if (parent->insertChild(first,pane)) {
                s_ornamentOrder[s_ornamentOrderCount++]={pane,parent,next};
            }
        }
        pane=parent;
    }
}

void restore_ornament_order() {
    while (s_ornamentOrderCount>0) {
        const auto& st=s_ornamentOrder[--s_ornamentOrderCount];
        st.parent->insertChild(st.next,st.pane);
    }
}

J2DPane* pane_common_ancestor(J2DPane* a, J2DPane* b, J2DScreen* screen) {
    if (a==nullptr || b==nullptr || screen==nullptr) return nullptr;
    for (J2DPane* pa=a; pa!=nullptr; pa=pa->getParentPane()) {
        for (J2DPane* pb=b; pb!=nullptr; pb=pb->getParentPane()) {
            if (pa==pb) return pa;
            if (pb==screen) break;
        }
        if (pa==screen) break;
    }
    return nullptr;
}

void capture_menu_ornament_host(J2DPicture* pic) {
    s_menuOrnamentHost.pane = nullptr;
    if (pic==nullptr) return;
    auto& st=s_menuOrnamentHost;
    st.pane=pic;
    st.bounds=pic->mBounds;
    st.x=pic->getTranslateX(); st.y=pic->getTranslateY();
    st.sx=pic->getScaleX(); st.sy=pic->getScaleY();
    st.rotation=pic->getRotateZ();
    st.alpha=pic->getAlpha();
    if (pic->getTexture(0)!=nullptr) st.tex0=pic->getTexture(0)->getTexInfo();
    if (pic->getTexture(1)!=nullptr) st.tex1=pic->getTexture(1)->getTexInfo();
    st.black=pic->getBlack(); st.white=pic->getWhite();
    for(int i=0;i<4;++i) st.corners[i]=pic->corner(i);
    copy_picture_texcoords(pic,st.texCoords);
}

void restore_menu_ornament_host() {
    auto& st=s_menuOrnamentHost;
    if(st.pane==nullptr) return;
    if(st.tex0!=nullptr && st.pane->getTextureCount()>0) st.pane->changeTexture(st.tex0,0);
    if(st.tex1!=nullptr && st.pane->getTextureCount()>1) st.pane->changeTexture(st.tex1,1);
    st.pane->mBounds=st.bounds;
    st.pane->translate(st.x,st.y);
    st.pane->scale(st.sx,st.sy);
    st.pane->rotate(st.rotation);
    restore_picture_texcoords(st.pane,st.texCoords);
    st.pane->setAlpha(st.alpha);
    st.pane->setBlackWhite(st.black,st.white);
    st.pane->setCornerColor(st.corners[0],st.corners[1],st.corners[2],st.corners[3]);
    s_menuOrnamentHost.pane=nullptr;
}

bool is_prompt_map_screen(J2DScreen* screen) {
    return prompt_map_kind(screen)!=PromptMapKind::None;
}

bool pane_is_within(J2DPane* pane, J2DPane* root) {
    if (root==nullptr) return false;
    for (; pane!=nullptr; pane=pane->getParentPane()) {
        if (pane==root) return true;
    }
    return false;
}

bool world_map_ornament_picture(J2DPane* pane, J2DPane* controls,
                                J2DPane* a, J2DPane* b, J2DPane* z) {
    // The world-map title screen also owns navigation glyphs and headers.
    // Only pictures inside cont_n, outside all button subtrees, are decoration.
    return pane_is_within(pane,controls) && !pane_is_within(pane,a) &&
           !pane_is_within(pane,b) && !pane_is_within(pane,z);
}

void prepare_menu_ornament_before_draw(J2DScreen* screen) {
    if (screen==nullptr) return;
    const bool shared = screen->search(MULTI_CHAR('atext1_1'))!=nullptr ||
                        screen->search(MULTI_CHAR('btext1_1'))!=nullptr;
    const bool map = is_prompt_map_screen(screen);
    if (!shared && !map) return;

    const bool worldMap=prompt_map_kind(screen)==PromptMapKind::World;
    J2DPane* worldControls=worldMap ? screen->search(MULTI_CHAR('cont_n')) : nullptr;
    J2DPane* worldZ=worldMap ? screen->search(MULTI_CHAR('zbtn_n1')) : nullptr;
    if (worldMap && worldControls==nullptr) return;

    J2DPane* aRoot=nullptr; J2DPane* bRoot=nullptr; J2DPane* cRoot=nullptr;
    static const u64 aIcons[]={MULTI_CHAR('g_abtn_n'),MULTI_CHAR('abtn_n1'),MULTI_CHAR('abtn_n')};
    static const u64 bIcons[]={MULTI_CHAR('g_bbtn_n'),MULTI_CHAR('bbtn_n1'),MULTI_CHAR('bbtn_n')};
    for (u64 t:aIcons) if ((aRoot=screen->search(t))!=nullptr) break;
    for (u64 t:bIcons) if ((bRoot=screen->search(t))!=nullptr) break;
    if (map) {
        aRoot=map_button_root(screen,prompt_map_kind(screen),true);
        bRoot=map_button_root(screen,prompt_map_kind(screen),false);
        cRoot=screen->search(MULTI_CHAR('c_btn'));
    }

    // The old implementation walked the entire Collection screen and treated
    // every visible picture as ornament artwork. Collection extensions (such as
    // Twilit Essentials pages/tabs) live inside that same vanilla J2DScreen, so
    // their icons were being hidden or even reused as the ornament texture host.
    // Restrict the shared-menu pass to the actual A/B prompt container only.
    J2DPane* scope = worldMap ? worldControls : pane_common_ancestor(aRoot,bRoot,screen);
    if (scope==nullptr || (!worldMap && scope==screen)) {
        // Unknown/shared layout: preserve all artwork rather than guessing.
        return;
    }

    J2DPicture* customOrnamentHost=nullptr;
    const ResTIMG* customOrnament=resource_timg(s_menu_ornament);
    const bool enabled = map ? cfg_bool(g_mapOrnamentEnabled,true)
                             : cfg_bool(g_menuPromptOrnament,true);
    const float ox = map ? cfg_pos(g_mapOrnamentX,0.0f) : cfg_pos(g_menuOrnamentX,0.0f);
    const float oy = map ? cfg_pos(g_mapOrnamentY,0.0f) : cfg_pos(g_menuOrnamentY,0.0f);
    const float os = map ? cfg_scale(g_mapOrnamentScale,1.0f) : cfg_scale(g_menuOrnamentScale,1.0f);

    s_menuOrnamentStateCount=0;
    s_menuOrnamentScreen=screen;
    s_menuOrnamentIsMap=map;
    s_menuOrnamentHost.pane=nullptr;

    J2DPane* stack[96]; int top=0; stack[top++]=scope;
    while(top>0) {
        J2DPane* node=stack[--top];
        for(J2DPane* child=node->getFirstChildPane(); child!=nullptr; child=child->getNextChildPane()) {
            if(top<96) stack[top++]=child;
            J2DPicture* pic=as_picture(child);
            if(pic==nullptr) continue;

            // Never let ornament handling touch controller-button subtrees.
            if(pane_is_within(pic,aRoot) || pane_is_within(pic,bRoot) ||
               pane_is_within(pic,cRoot) || pane_is_within(pic,worldZ))
                continue;
            if(worldMap && !world_map_ornament_picture(pic,worldControls,aRoot,bRoot,worldZ))
                continue;
            if(s_menuOrnamentStateCount>=128) continue;

            s_menuOrnamentState[s_menuOrnamentStateCount++]={pic,pic->isVisible()};
            if(!enabled) {
                if(pic->isVisible()) pic->hide();
                continue;
            }
            if(!pic->isVisible()) continue;

            if(customOrnamentHost==nullptr && customOrnament!=nullptr) {
                customOrnamentHost=pic;
                capture_menu_ornament_host(pic);
                replace_picture_texture(pic,customOrnament);
                const JUtility::TColor neutralBlack(0,0,0,0);
                const JUtility::TColor neutralWhite(255,255,255,255);
                pic->setBlackWhite(neutralBlack,neutralWhite);
                pic->setCornerColor(neutralWhite);
                pic->show();

                // Apply the configured draw-time transform from the exact
                // geometry captured above. It is restored immediately after draw.
                const auto& base=s_menuOrnamentHost;
                pic->translate(base.x+ox,base.y+oy);
                pic->resize(128.0f,128.0f);
                pic->scale(os,os);
            } else if(customOrnamentHost!=pic) {
                pic->hide();
            }
        }
    }

    // Keep the original pane order intact for compatibility with Collection extensions.
}

void restore_shared_menu_ornament_after_draw(J2DScreen* screen) {
    if (screen==nullptr || screen!=s_menuOrnamentScreen) return;
    restore_menu_ornament_host();

    // Always restore every visibility bit we touched. The previous code left
    // non-host pictures hidden whenever the shared ornament was enabled, which
    // permanently removed third-party Collection tabs/icons after the first draw.
    for(int i=s_menuOrnamentStateCount-1;i>=0;--i) {
        auto& st=s_menuOrnamentState[i];
        if(st.pane==nullptr) continue;
        if(st.visible) st.pane->show(); else st.pane->hide();
        st.pane=nullptr;
    }
    s_menuOrnamentStateCount=0;
    s_menuOrnamentScreen=nullptr;
    s_menuOrnamentIsMap=false;
}

bool world_map_arrow(J2DPane* pane) {
    if (pane==nullptr) return false;
    const u64 tag=pane->mInfoTag;
    return tag==MULTI_CHAR('yaji_04') || tag==MULTI_CHAR('yaji_05') ||
           tag==MULTI_CHAR('yaji_06') || tag==MULTI_CHAR('yaji_07');
}

struct WorldIconGeometry {
    J2DPicture* pane;
    JGeometry::TBox2<f32> bounds;
    float x,y,sx,sy,rotation;
};
WorldIconGeometry s_worldIconGeometry[3];
int s_worldIconGeometryCount=0;
J2DScreen* s_worldIconScreen=nullptr;
struct WorldArrowState { J2DPane* pane; bool visible; };
WorldArrowState s_worldArrows[4];
int s_worldArrowCount=0;

// Keep every world-map picture mutation strictly local to one screen draw.
// This intentionally snapshots only known map prompt subtrees instead of all
// J2D panes; broader snapshots caused invalid restores when menus rebuilt panes.
struct WorldMapPictureState {
    J2DPicture* pic = nullptr;
    bool visible = false;
    u8 alpha = 255;
    JGeometry::TBox2<f32> bounds{};
    float x = 0.0f, y = 0.0f, sx = 1.0f, sy = 1.0f, rotation = 0.0f;
    u8 textureCount = 0;
    const ResTIMG* textures[8]{};
    JUtility::TColor black{};
    JUtility::TColor white{};
    JUtility::TColor corners[4]{};
    JGeometry::TVec2<s16> texCoords[4]{};
};

WorldMapPictureState s_worldMapPictures[96];
int s_worldMapPictureCount = 0;
J2DScreen* s_worldMapTempScreen = nullptr;

void capture_world_map_picture(J2DPicture* pic) {
    if (pic == nullptr) return;
    for (int i = 0; i < s_worldMapPictureCount; ++i)
        if (s_worldMapPictures[i].pic == pic) return;
    if (s_worldMapPictureCount >= (int)(sizeof(s_worldMapPictures) / sizeof(s_worldMapPictures[0]))) return;

    auto& st = s_worldMapPictures[s_worldMapPictureCount++];
    st.pic = pic;
    st.visible = pic->isVisible();
    st.alpha = pic->getAlpha();
    st.bounds = pic->mBounds;
    st.x = pic->getTranslateX();
    st.y = pic->getTranslateY();
    st.sx = pic->getScaleX();
    st.sy = pic->getScaleY();
    st.rotation = pic->getRotateZ();
    st.textureCount = pic->getTextureCount();
    if (st.textureCount > 8) st.textureCount = 8;
    for (u8 i = 0; i < st.textureCount; ++i) {
        if (pic->getTexture(i) != nullptr)
            st.textures[i] = pic->getTexture(i)->getTexInfo();
    }
    st.black = pic->getBlack();
    st.white = pic->getWhite();
    for (int i = 0; i < 4; ++i) st.corners[i] = pic->corner(i);
    copy_picture_texcoords(pic, st.texCoords);
}

void capture_world_map_picture_tree(J2DPane* root) {
    if (root == nullptr) return;
    J2DPane* stack[64];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        J2DPane* pane = stack[--top];
        if (J2DPicture* pic = as_picture(pane)) capture_world_map_picture(pic);
        for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (top < 64) stack[top++] = child;
        }
    }
}

void begin_world_map_temp_state(J2DScreen* screen, PromptMapKind kind) {
    s_worldMapTempScreen = screen;
    s_worldMapPictureCount = 0;
    if (screen == nullptr || kind == PromptMapKind::None) return;

    if (kind == PromptMapKind::World) {
        capture_world_map_picture_tree(screen->search(MULTI_CHAR('zbtn_n1')));
        capture_world_map_picture_tree(screen->search(MULTI_CHAR('as_n')));
        capture_world_map_picture_tree(screen->search(MULTI_CHAR('juji_c_n')));
    }
    capture_world_map_picture_tree(map_button_root(screen, kind, true));
    capture_world_map_picture_tree(map_button_root(screen, kind, false));
}

void restore_world_map_temp_state(J2DScreen* screen) {
    if (screen == nullptr || screen != s_worldMapTempScreen) return;

    for (int i = s_worldMapPictureCount - 1; i >= 0; --i) {
        auto& st = s_worldMapPictures[i];
        if (st.pic == nullptr) continue;

        const u8 liveCount = st.pic->getTextureCount();
        const u8 count = st.textureCount < liveCount ? st.textureCount : liveCount;
        for (u8 t = 0; t < count; ++t) {
            if (st.textures[t] != nullptr) st.pic->changeTexture(st.textures[t], t);
        }
        st.pic->mBounds = st.bounds;
        st.pic->translate(st.x, st.y);
        st.pic->scale(st.sx, st.sy);
        st.pic->rotate(st.rotation);
        restore_picture_texcoords(st.pic, st.texCoords);
        st.pic->setAlpha(st.alpha);
        st.pic->setBlackWhite(st.black, st.white);
        st.pic->setCornerColor(st.corners[0], st.corners[1], st.corners[2], st.corners[3]);
        if (st.visible) st.pic->show(); else st.pic->hide();
    }

    s_worldMapPictureCount = 0;
    s_worldMapTempScreen = nullptr;
}

void offset_world_icon(J2DPicture* pane,ConfigVarHandle x,ConfigVarHandle y) {
    if (pane==nullptr) return;
    // Icon geometry was already captured by fit_world_icon for restoration.
    pane->translate(pane->getTranslateX()+cfg_pos(x,0.0f),
                    pane->getTranslateY()+cfg_pos(y,0.0f));
}

void adjust_world_arrows(J2DScreen* screen) {
    static const u64 tags[]={MULTI_CHAR('yaji_04'),MULTI_CHAR('yaji_05'),MULTI_CHAR('yaji_06'),MULTI_CHAR('yaji_07')};
    s_worldArrowCount=0;
    for (u64 tag:tags) {
        if (J2DPane* pane=screen->search(tag)) {
            s_worldArrows[s_worldArrowCount++]={pane,pane->isVisible()};
            apply_menu_pane_transform(pane,g_worldDpadX,g_worldDpadY,g_worldArrowScale);
            pane->translate(pane->getTranslateX()+cfg_pos(g_worldArrowX,0.0f),
                            pane->getTranslateY()+cfg_pos(g_worldArrowY,0.0f));
            // Enabled preserves native visibility and animation, never forces show.
            if (!cfg_bool(g_worldArrows,true)) pane->hide();
        }
    }
}

void fit_world_icon(J2DPicture* pic,const ResTIMG* texture,float side) {
    if (pic==nullptr || texture==nullptr || s_worldIconGeometryCount>=3) return;
    auto& st=s_worldIconGeometry[s_worldIconGeometryCount++];
    st={pic,pic->mBounds,pic->getTranslateX(),pic->getTranslateY(),
        pic->getScaleX(),pic->getScaleY(),pic->getRotateZ()};
    // Bounds are local to the pane, not coordinates accepted by move().
    // Preserve the original center in parent space and the resource aspect.
    const float cx=st.x+(st.bounds.i.x+st.bounds.getWidth()*0.5f)*st.sx;
    const float cy=st.y+(st.bounds.i.y+st.bounds.getHeight()*0.5f)*st.sy;
    const float aspect=texture->height ? float(texture->width)/float(texture->height) : 1.0f;
    pic->resize(side*aspect,side);
    pic->scale(1.0f,1.0f);
    pic->rotate(0.0f);
    const auto& fitted=pic->getBounds();
    pic->translate(cx-(fitted.i.x+fitted.getWidth()*0.5f),
                   cy-(fitted.i.y+fitted.getHeight()*0.5f));
    // UVs must be rebuilt after resizing the native glyph.
    replace_picture_texture(pic,texture);
}

void restore_world_icons(J2DScreen* screen) {
    if (screen!=s_worldIconScreen) return;
    while (s_worldArrowCount>0) {
        const auto& st=s_worldArrows[--s_worldArrowCount];
        if (st.visible) st.pane->show(); else st.pane->hide();
    }
    while (s_worldIconGeometryCount>0) {
        const auto& st=s_worldIconGeometry[--s_worldIconGeometryCount];
        // place() also adjusts descendants. On the map that can make the
        // confirm/back groups drift a little farther every draw.
        st.pane->mBounds=st.bounds;
        st.pane->translate(st.x,st.y);
        st.pane->scale(st.sx,st.sy);
        st.pane->rotate(st.rotation);
    }
    s_worldIconScreen=nullptr;
}

void move_world_navigation_text(J2DScreen* screen) {
    // Exact regional text layers from dMenu_Fmap2DTop_c. No dependency on
    // TBX1/TBX2 kind or on whether the text sits inside the icon's group.
    static const u64 portals[]={MULTI_CHAR('cont_zt'),MULTI_CHAR('cont_zt1'),MULTI_CHAR('cont_zt2'),MULTI_CHAR('cont_zt3'),MULTI_CHAR('cont_zt4'),MULTI_CHAR('font_zt1'),MULTI_CHAR('font_zt2'),MULTI_CHAR('font_zt3'),MULTI_CHAR('font_zt4'),MULTI_CHAR('font_zt5')};
    static const u64 move[]={MULTI_CHAR('ast_00'),MULTI_CHAR('ast_01'),MULTI_CHAR('ast_02'),MULTI_CHAR('ast_03'),MULTI_CHAR('ast_04'),MULTI_CHAR('fst_00'),MULTI_CHAR('fst_01'),MULTI_CHAR('fst_02'),MULTI_CHAR('fst_03'),MULTI_CHAR('fst_04')};
    static const u64 back[]={MULTI_CHAR('juji_c00'),MULTI_CHAR('juji_c01'),MULTI_CHAR('juji_c02'),MULTI_CHAR('juji_c03'),MULTI_CHAR('juji_c04'),MULTI_CHAR('fuji_c00'),MULTI_CHAR('fuji_c01'),MULTI_CHAR('fuji_c02'),MULTI_CHAR('fuji_c03'),MULTI_CHAR('fuji_c04')};
    for (u64 tag:portals) apply_menu_pane_transform(screen->search(tag),g_WorldPortalTextX,g_WorldPortalTextY,g_WorldPortalTextScale);
    for (u64 tag:move) apply_menu_pane_transform(screen->search(tag),g_WorldMoveTextX,g_WorldMoveTextY,g_WorldMoveTextScale);
    for (u64 tag:back) apply_menu_pane_transform(screen->search(tag),g_WorldReturnTextX,g_WorldReturnTextY,g_WorldReturnTextScale);
}

void replace_world_map_dpad(J2DPane* pane, const ResTIMG* texture, J2DPicture*& face) {
    if (pane==nullptr || texture==nullptr || world_map_arrow(pane)) return;
    if (J2DPicture* pic=as_picture(pane)) {
        if (face==nullptr) {
            face=pic;
            replace_picture_texture(pic,texture);
            const JUtility::TColor black(0,0,0,0), white(255,255,255,255);
            pic->setBlackWhite(black,white);
            pic->setCornerColor(white);
            fit_world_icon(pic,texture,22.0f*cfg_scale(g_worldDpadScale,1.0f));
        } else {
            pic->hide();
        }
    }
    for (J2DPane* child=pane->getFirstChildPane(); child!=nullptr; child=child->getNextChildPane())
        replace_world_map_dpad(child,texture,face);
}

void apply_known_menu_buttons(J2DScreen* screen) {
    // Unique howling layout signature; do not match other cbtn_n screens.
    if (screen != nullptr && screen->search(MULTI_CHAR('g_ltxt_n')) != nullptr &&
        screen->search(MULTI_CHAR('gr_txt_n')) != nullptr &&
        screen->search(MULTI_CHAR('line00')) != nullptr) {
        apply_menu_button_texture(screen->search(MULTI_CHAR('cbtn_n')),resource_timg(s_analog));
        apply_menu_button_texture(screen->search(MULTI_CHAR('abt_n')),resource_timg(s_cross));
    }
    if (screen == nullptr) return;

    // Never run this generic menu pass on the gameplay HUD itself.
    // The HUD has its own proven button path and must remain untouched.
    if (s_activeMeter != nullptr && screen == s_activeMeter->mpScreen) return;

    const ResTIMG* cross = resource_timg(s_cross);
    const ResTIMG* circle = gc_b_face_texture();
    if (cross == nullptr || circle == nullptr) return;

    // Known menu roots used across Collection/Options/TV setup/FMap.
    // Missing tags simply return nullptr, so each layout keeps its own
    // positioning/animation while only the icon artwork is replaced.
    static const u64 aTags[] = {
        MULTI_CHAR('g_abtn_n'),
        // TV Settings / Bright Check uses this GameCube A-button root.
        MULTI_CHAR('gcabtn_n'),
        MULTI_CHAR('abtn_n1'),
        MULTI_CHAR('abtn_n'),
        MULTI_CHAR('w_abtn_n'),
        MULTI_CHAR('w_n_abtn'),
        // In-game save menu (dMenu_save_c / zelda_file_select2.blo).
        MULTI_CHAR('w_nabtn'),
    };
    static const u64 bTags[] = {
        MULTI_CHAR('g_bbtn_n'),
        MULTI_CHAR('bbtn_n1'),
        MULTI_CHAR('bbtn_n'),
        MULTI_CHAR('w_bbtn_n'),
        MULTI_CHAR('w_n_bbtn'),
        // In-game save menu (dMenu_save_c / zelda_file_select2.blo).
        MULTI_CHAR('w_nbbtn'),
    };

    const bool itemWheelScreen =
        screen->search(MULTI_CHAR('fyx_tex')) != nullptr &&
        screen->search(MULTI_CHAR('x_btn_n')) != nullptr &&
        screen->search(MULTI_CHAR('y_btn_n')) != nullptr;

    const PromptMapKind mapKind=prompt_map_kind(screen);
    const bool sharedPrompt = !itemWheelScreen && mapKind==PromptMapKind::None &&
        (screen->search(MULTI_CHAR('atext1_1'))!=nullptr ||
         screen->search(MULTI_CHAR('btext1_1'))!=nullptr);
    if(sharedPrompt) begin_shared_prompt_temp_state(screen);
    if (mapKind!=PromptMapKind::None) begin_world_map_temp_state(screen,mapKind);
    if (mapKind==PromptMapKind::World) {
        s_worldIconScreen=screen;
        s_worldIconGeometryCount=0;
        apply_menu_button_texture(screen->search(MULTI_CHAR('zbtn_n1')),resource_timg(s_r1));
        fit_world_icon(first_picture_recursive(screen->search(MULTI_CHAR('zbtn_n1'))),resource_timg(s_r1),22.0f*cfg_scale(g_worldR1Scale,1.0f));
        offset_world_icon(first_picture_recursive(screen->search(MULTI_CHAR('zbtn_n1'))),g_worldR1X,g_worldR1Y);
        apply_menu_button_texture(screen->search(MULTI_CHAR('as_n')),resource_timg(s_analog));
        fit_world_icon(first_picture_recursive(screen->search(MULTI_CHAR('as_n'))),resource_timg(s_analog),22.0f*cfg_scale(g_worldAnalogScale,1.0f));
        offset_world_icon(first_picture_recursive(screen->search(MULTI_CHAR('as_n'))),g_worldAnalogX,g_worldAnalogY);
        J2DPicture* dpadFace=nullptr;
        replace_world_map_dpad(screen->search(MULTI_CHAR('juji_c_n')),resource_timg(s_dpad),dpadFace);
        offset_world_icon(dpadFace,g_worldDpadX,g_worldDpadY);
        adjust_world_arrows(screen);
        move_world_navigation_text(screen);
    }
    if (mapKind!=PromptMapKind::None) {
        apply_menu_button_texture(map_button_root(screen,mapKind,true),cross);
        apply_menu_button_texture(map_button_root(screen,mapKind,false),circle);
    } else if (!itemWheelScreen) {
        for (u64 tag : aTags) {
            J2DPane* root = screen->search(tag);
            if (root != nullptr) apply_menu_button_texture(root, cross);
        }
        for (u64 tag : bTags) {
            J2DPane* root = screen->search(tag);
            if (root != nullptr) apply_menu_button_texture(root, circle);
        }
    }

    // In-game save screen (zelda_file_select2.blo): only the Circle picture is
    // resized, using the Cross picture as the reference. Position/text stay intact.
    J2DPane* saveA = screen->search(MULTI_CHAR('w_nabtn'));
    J2DPane* saveB = screen->search(MULTI_CHAR('w_nbbtn'));
    if (saveA != nullptr && saveB != nullptr) {
        match_menu_button_picture_size(saveB, saveA);
        apply_menu_button_offset(saveA, s_saveCrossBase, g_saveCrossX, g_saveCrossY);
        apply_menu_button_offset(saveB, s_saveCircleBase, g_saveCircleX, g_saveCircleY);
    }

    // Initial file-select layout may also pass through this generic screen hook.
    J2DPane* fileA = screen->search(MULTI_CHAR('w_n_abtn'));
    J2DPane* fileB = screen->search(MULTI_CHAR('w_n_bbtn'));
    if (fileA != nullptr && fileB != nullptr) {
        match_menu_button_picture_size(fileB, fileA);
        apply_menu_button_offset(fileA, s_fileCrossBase, g_fileCrossX, g_fileCrossY);
        apply_menu_button_offset(fileB, s_fileCircleBase, g_fileCircleX, g_fileCircleY);
    }

    // Letter menu (zelda_letter_select_base.blo): the pane-tree diagnostic
    // identified the GameCube shoulder prompts exactly.  g_lbtn_n/g_rbtn_n
    // are their isolated roots; replace the first picture with our existing
    // L2/R2 art and hide the small vanilla L/R overlay pieces.
    if (screen->search(MULTI_CHAR('pi_no_00')) != nullptr &&
        screen->search(MULTI_CHAR('let_area')) != nullptr &&
        screen->search(MULTI_CHAR('g_lbtn_n')) != nullptr &&
        screen->search(MULTI_CHAR('g_rbtn_n')) != nullptr) {
        if (const ResTIMG* l2 = resource_timg(s_l2)) {
            J2DPane* root = screen->search(MULTI_CHAR('g_lbtn_n'));
            if (J2DPicture* face = first_picture_recursive(root)) {
                const auto& b = face->getBounds();
                const float h = b.getHeight();
                const float aspect = l2->height ? (float)l2->width / (float)l2->height : 1.0f;
                const float w = h * aspect;
                const float cx = b.i.x + b.getWidth() * 0.5f;
                replace_picture_texture(face, l2);
                set_bounds(face, cx - w * 0.5f, b.i.y, w, h);
                const JUtility::TColor black(0,0,0,0), white(255,255,255,255);
                face->setBlackWhite(black, white); face->setCornerColor(white); face->setAlpha(255); face->show();
            }
            if (J2DPane* p = screen->search(MULTI_CHAR('g_lbtn1'))) p->hide();
            if (J2DPane* p = screen->search(MULTI_CHAR('g_lbtn2'))) p->hide();
        }
        if (const ResTIMG* r2 = resource_timg(s_r2)) {
            J2DPane* root = screen->search(MULTI_CHAR('g_rbtn_n'));
            if (J2DPicture* face = first_picture_recursive(root)) {
                const auto& b = face->getBounds();
                const float h = b.getHeight();
                const float aspect = r2->height ? (float)r2->width / (float)r2->height : 1.0f;
                const float w = h * aspect;
                const float cx = b.i.x + b.getWidth() * 0.5f;
                replace_picture_texture(face, r2);
                set_bounds(face, cx - w * 0.5f, b.i.y, w, h);
                const JUtility::TColor black(0,0,0,0), white(255,255,255,255);
                face->setBlackWhite(black, white); face->setCornerColor(white); face->setAlpha(255); face->show();
            }
            if (J2DPane* p = screen->search(MULTI_CHAR('g_btn_t'))) p->hide();
        }
    }

    // Shared Collection/Options/Fishing/Skills/etc. A/B prompt layout.
    // The item wheel has its own dedicated path below; never let broad menu
    // aliases touch its panes because Twilit Essentials also anchors UI to them.
    if (!itemWheelScreen) {
        apply_shared_menu_prompt_layout(screen);
        // Map prompts use mutually exclusive layouts and independent configuration handles.
        apply_map_menu_prompt_layout(screen);
    }

    // Item Wheel controller prompts are handled by the dedicated
    // dMenu_Ring_c::_draw overlay hook below. Do not mutate the wheel's
    // J2DScreen/J2DMaterial tree here: Twilit Essentials hooks the same ring
    // object and reuses those panes as anchors.

    // Options menu: the GameCube Z prompt lives under z_gc_n.
    // This was part of the stable pre-compat implementation and was
    // accidentally dropped while isolating the Item Wheel/Collection.
    const ResTIMG* r1 = resource_timg(s_r1);
    if (r1 != nullptr) {
        J2DPane* zRoot = screen->search(MULTI_CHAR('z_gc_n'));
        if (zRoot != nullptr) {
            apply_menu_button_texture(zRoot, r1);
            if (J2DPicture* face = first_picture_recursive(zRoot)) {
                const auto& b = face->getBounds();
                const float oldW = b.getWidth();
                const float oldH = b.getHeight();
                const float cx = b.i.x + oldW * 0.5f;
                const float cy = b.i.y + oldH * 0.5f;
                const float aspect = r1->height != 0
                    ? ((float)r1->width / (float)r1->height) : 2.0f;
                const float newH = oldH;
                const float newW = newH * aspect;
                set_bounds(face, cx - newW * 0.5f, cy - newH * 0.5f, newW, newH);
            }
        }
    }

    // Dungeon map: restore the dedicated R3 "Mover" prompt.
    // dMenu_DmapBg_c builds it under c_btn beside c_text/c_text_s.
    const ResTIMG* r3 = resource_timg(s_r3);
    if (r3 != nullptr && screen->search(MULTI_CHAR('c_text')) != nullptr) {
        J2DPane* cRoot = screen->search(MULTI_CHAR('c_btn'));
        if (cRoot != nullptr) apply_menu_button_texture(cRoot, r3);
    }

    // Options help legend: restore the four analog-stick glyphs.
    // Keep neighbouring .yaji_* arrow panes untouched.
    const ResTIMG* analog = resource_timg(s_analog);
    if (analog != nullptr && screen->search(MULTI_CHAR('let_area')) != nullptr) {
        static const u64 analogTags[] = {
            MULTI_CHAR('wi_juji1'),
            MULTI_CHAR('wi_juji'),
            MULTI_CHAR('wi_juji2'),
            MULTI_CHAR('wi_juji3'),
        };
        for (u64 tag : analogTags) {
            J2DPane* pane = screen->search(tag);
            J2DPicture* pic = as_picture(pane);
            if (pic != nullptr) {
                replace_picture_texture(pic, analog);
                const JUtility::TColor neutralBlack(0, 0, 0, 0);
                const JUtility::TColor neutralWhite(255, 255, 255, 255);
                pic->setBlackWhite(neutralBlack, neutralWhite);
                pic->setCornerColor(neutralWhite);
                pic->show();
            }
        }
    }
}


// Apply TP Classic's contextual Midna artwork only after dMeterButton_c::_execute.
// Twilit Essentials updates this same prompt from _execute; our low-priority post
// hook runs afterwards, observes its state, and yields without modifying the tree.
void apply_tp_classic_midna_prompt(dMeterButton_c* self) {
    if (self == nullptr || self->mpButtonScreen == nullptr) return;

    J2DPicture* jumpFace = as_picture(self->mpButtonScreen->search(MULTI_CHAR('zbtn')));
    const ResTIMG* r1 = resource_timg(s_r1);
    if (jumpFace == nullptr || r1 == nullptr || r1->height == 0) return;

    replace_picture_texture(jumpFace, r1);
    jumpFace->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                            JUtility::TColor(255, 255, 255, 255));
    jumpFace->setCornerColor(JUtility::TColor(255, 255, 255, 255));
    const auto bounds = jumpFace->getBounds();
    const float sx = jumpFace->getScaleX();
    const float sy = jumpFace->getScaleY();
    if (sx > 0.0001f && sy > 0.0001f) {
        const float aspect = float(r1->width) / float(r1->height);
        const float height = bounds.getWidth() * sx / (aspect * sy);
        jumpFace->move(bounds.i.x, bounds.i.y + (bounds.getHeight() - height) * 0.5f);
        jumpFace->resize(bounds.getWidth(), height);
        jumpFace->rotate(0.0f);
    }

    J2DPane* root = self->mpButtonScreen->search(MULTI_CHAR('zbtn_n'));
    J2DPane* portrait = self->mpButtonScreen->search(MULTI_CHAR('midona'));
    J2DPane* stack[64];
    int count = 0;
    if (root != nullptr) stack[count++] = root;
    while (count > 0) {
        J2DPane* node = stack[--count];
        for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            if (child == portrait) continue;
            if (count < 64) stack[count++] = child;
            if (child == jumpFace || as_picture(child) == nullptr) continue;
            bool preserve = false;
            for (J2DPane* p = jumpFace->getParentPane(); p != nullptr; p = p->getParentPane())
                if (p == child) preserve = true;
            for (J2DPane* p = portrait; p != nullptr; p = p->getParentPane())
                if (p == child) preserve = true;
            if (!preserve) child->hide();
        }
    }

    // Keep the old Z highlight hidden for TP Classic's R1 artwork. Unlike
    // Essentials, TP Classic intentionally does not force this pane's alpha to 0;
    // that distinction is also a useful live ownership signature.
    if (J2DPane* oldLight = self->mpButtonScreen->search(MULTI_CHAR('z_btnl')))
        oldLight->hide();
}

bool external_midna_prompt_owner(dMeterButton_c* self) {
    if (self == nullptr || self->mpButtonScreen == nullptr) return false;
    J2DScreen* screen = self->mpButtonScreen;
    J2DPicture* face = as_picture(screen->search(MULTI_CHAR('zbtn')));

    if (screen != s_midnaPromptScreen) {
        // Defensive path for a rebuilt screen that did not pass our screen-init
        // hook in the expected order. Treat the first observed texture as baseline.
        s_midnaPromptScreen = screen;
        s_midnaPromptOriginalTexture =
            (face != nullptr && face->getTextureCount() != 0 && face->getTexture(0) != nullptr)
                ? face->getTexture(0)->getTexInfo()
                : nullptr;
        s_externalMidnaPromptOwner = false;
    }

    const ResTIMG* current =
        (face != nullptr && face->getTextureCount() != 0 && face->getTexture(0) != nullptr)
            ? face->getTexture(0)->getTexInfo()
            : nullptr;
    const ResTIMG* ours = resource_timg(s_r1);

    // Essentials' default Midna binding replaces zbtn with its D-pad texture.
    // Ignore our own R1 texture so repeated TP Classic frames remain idempotent.
    const bool foreignTexture =
        s_midnaPromptOriginalTexture != nullptr && current != nullptr &&
        current != s_midnaPromptOriginalTexture && current != ours;

    // When Midna is bound to L, Essentials can deliberately use the original Z
    // texture. In both modes it hides z_btnl and explicitly forces alpha to zero;
    // TP Classic only hides this pane and never zeros its alpha.
    J2DPane* oldLight = screen->search(MULTI_CHAR('z_btnl'));
    const bool essentialsLightSignature =
        oldLight != nullptr && !oldLight->isVisible() && oldLight->getAlpha() == 0;

    return foreignTexture || essentialsLightSignature;
}

void after_meter_button_execute(ModContext*, void* args, void*, void*) {
    dMeterButton_c* self = args != nullptr ? mods::arg<dMeterButton_c*>(args, 0) : nullptr;
    if (self == nullptr || self->mpButtonScreen == nullptr) return;

    if (external_midna_prompt_owner(self)) {
        s_externalMidnaPromptOwner = true;
        return;
    }
    if (s_externalMidnaPromptOwner) return;

    apply_tp_classic_midna_prompt(self);
}

// v0.11.20 brightness test, based directly on the stable v0.11.19 implementation.
// Replace only the contextual GameCube R artwork after its layout is created.
// No code from the experimental v0.11.07-v0.11.18 chain is carried over.
void after_meter_button_screen_init(ModContext*, void* args, void*, void*) {
    dMeterButton_c* self = args != nullptr ? mods::arg<dMeterButton_c*>(args, 0) : nullptr;
    if (self == nullptr || self->mpButtonScreen == nullptr) return;

    // mpButtonScreen is zelda_game_image_button_info.blo (contextual prompts).
    // It has its own 60x60 pane also named "midona" under zbtn_n; that is NOT
    // the 36x36 main HUD portrait and must never seed s_midnaPictures.
    capture_midna_diagnostic_snapshot("screenInitButton-contextual",nullptr,self->mpButtonScreen);

    // Fishing uses these exact faces in zelda_game_image_button_info.blo.
    // Change only artwork; directional arrows and the combined-prompt plus
    // sign are separate panes and remain under native visibility control.
    struct FishingFace { u64 tag; const ResTIMG* texture; };
    const FishingFace fishingFaces[] = {
        {MULTI_CHAR('c_btn'), resource_timg(s_r3)},
        {MULTI_CHAR('as_btn1'), resource_timg(s_analog)},
        {MULTI_CHAR('as_btn3'), resource_timg(s_analog)},
        {MULTI_CHAR('b_btn1'), gc_b_face_texture()},
    };
    for (const FishingFace& entry : fishingFaces) {
        J2DPicture* face = as_picture(self->mpButtonScreen->search(entry.tag));
        if (face == nullptr || entry.texture == nullptr || entry.texture->height == 0) continue;
        replace_picture_texture(face, entry.texture);
        face->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        face->setCornerColor(JUtility::TColor(255, 255, 255, 255));
        const auto box = face->getBounds();
        const float sx = face->getScaleX(), sy = face->getScaleY();
        if (sx > 0.0001f && sy > 0.0001f) {
            const float width = box.getHeight() * sy / sx * float(entry.texture->width) / float(entry.texture->height);
            face->move(box.i.x + (box.getWidth() - width) * 0.5f, box.i.y);
            face->resize(width, box.getHeight());
            face->rotate(0.0f);
        }
    }
    const u64 fishingOldLayers[] = {
        MULTI_CHAR('as_btn'), MULTI_CHAR('as_btn2'),
        MULTI_CHAR('as_btn4'), MULTI_CHAR('as_btn5'),
        MULTI_CHAR('b_btn_l1'), MULTI_CHAR('b_btn_t1'),
    };
    for (u64 tag : fishingOldLayers)
        if (J2DPane* old = self->mpButtonScreen->search(tag)) old->hide();
    // Contextual Midna/Z prompt: capture the untouched baseline only.
    // Do not mutate this subtree during screenInitButton. Twilit Essentials
    // snapshots the same vanilla zbtn later, so changing it here would poison
    // its own compatibility/original-texture state.
    J2DPicture* jumpFace = as_picture(self->mpButtonScreen->search(MULTI_CHAR('zbtn')));
    s_midnaPromptScreen = self->mpButtonScreen;
    s_midnaPromptOriginalTexture =
        (jumpFace != nullptr && jumpFace->getTextureCount() != 0 && jumpFace->getTexture(0) != nullptr)
            ? jumpFace->getTexture(0)->getTexInfo()
            : nullptr;
    s_externalMidnaPromptOwner = false;

    // Bottom contextual Y prompt (Wolf Dig), separate from the main HUD.
    // Replace the face only; retain its parent alpha and prompt animation.
    J2DPicture* digFace = as_picture(self->mpButtonScreen->search(MULTI_CHAR('y_btn')));
    const ResTIMG* yFace = gc_y_face_texture();
    if (digFace != nullptr && yFace != nullptr) {
        // Only the contextual Y button's artwork subtree. The label and
        // other prompts live outside ybtn_n and retain their native behavior.
        J2DPane* root = self->mpButtonScreen->search(MULTI_CHAR('ybtn_n'));
        J2DPane* stack[64];
        int count = 0;
        if (root != nullptr) stack[count++] = root;
        while (count > 0) {
            J2DPane* node = stack[--count];
            for (J2DPane* child = node->getFirstChildPane(); child != nullptr;
                 child = child->getNextChildPane()) {
                if (count < 64) stack[count++] = child;
                if (child == digFace || as_picture(child) == nullptr) continue;
                bool containsFace = false;
                for (J2DPane* parent = digFace->getParentPane(); parent != nullptr;
                     parent = parent->getParentPane()) {
                    if (parent == child) { containsFace = true; break; }
                }
                if (!containsFace) child->hide();
            }
        }
        replace_picture_texture(digFace, yFace);
        digFace->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                              JUtility::TColor(255, 255, 255, 255));
        digFace->setCornerColor(JUtility::TColor(255, 255, 255, 255));
        const JGeometry::TBox2<f32> bounds = digFace->getBounds();
        // Fit a circle to the original slot height, keeping its center.
        // Compensate any local nonuniform scale without changing parent layout.
        const float sx = digFace->getScaleX();
        const float sy = digFace->getScaleY();
        if (sx > 0.0001f && sy > 0.0001f) {
            const float width = bounds.getHeight() * sy / sx;
            digFace->move(bounds.i.x + (bounds.getWidth() - width) * 0.5f, bounds.i.y);
            digFace->resize(width, bounds.getHeight());
            digFace->rotate(0.0f);
        }
    }
    const ResTIMG* r2 = resource_timg(s_r2);
    J2DPicture* base = as_picture(self->mpButtonScreen->search(MULTI_CHAR('r_btn_b')));
    J2DPane* rightPiece = self->mpButtonScreen->search(MULTI_CHAR('r_btn_r'));
    J2DPane* leftPiece = self->mpButtonScreen->search(MULTI_CHAR('r_btn_l'));
    if (base == nullptr || r2 == nullptr) return;

    // Change only the primary image. Leave the rest of the material/layout state alone.
    if (base->getTextureCount() != 0) {
        base->changeTexture(r2, 0);
        if (base->getTexture(0) != nullptr)
            base->setTexCoord(base->getTexture(0), BIND15, MIRROR0, false);
    }

    // v0.11.20: neutralize the original GameCube R picture tint so the R2
    // keeps the brightness/colors authored in its texture. Pane alpha and the
    // game's prompt/glow animation remain untouched.
    const JUtility::TColor neutralBlack(0, 0, 0, 0);
    const JUtility::TColor neutralWhite(255, 255, 255, 255);
    base->setBlackWhite(neutralBlack, neutralWhite);
    base->setCornerColor(neutralWhite);

    // Preserve the R2 artwork's 4:3 aspect ratio inside the original 55x36 slot.
    const JGeometry::TBox2<f32> box = base->getBounds();
    const float oldW = box.getWidth();
    const float oldH = box.getHeight();
    const float newW = oldH * (4.0f / 3.0f);
    base->move(box.i.x + (oldW - newW) * 0.5f, box.i.y);
    base->resize(newW, oldH);

    // Remove the two pieces that draw the original GameCube R glyph.
    if (rightPiece != nullptr) rightPiece->hide();
    if (leftPiece != nullptr) leftPiece->hide();

    // v0.11.27: Start/S prompt used by BUTTON_STATUS_CANT_SKIP (0x4D).
    // dMeterButton_c owns this as sbtn_n; a_btn2 is the picture used for
    // BUTTON_S_e sizing/rendering. Replace only that picture with the
    // user-supplied PlayStation Options artwork, preserving Start behavior.
    const ResTIMG* options = resource_timg(s_options);
    J2DPane* sRoot = self->mpButtonScreen->search(MULTI_CHAR('sbtn_n'));
    J2DPicture* startFace = as_picture(self->mpButtonScreen->search(MULTI_CHAR('a_btn2')));
    if (sRoot != nullptr && startFace != nullptr && options != nullptr) {
        if (startFace->getTextureCount() != 0) {
            startFace->changeTexture(options, 0);
            if (startFace->getTexture(0) != nullptr)
                startFace->setTexCoord(startFace->getTexture(0), BIND15, MIRROR0, false);
        }
        startFace->setBlackWhite(neutralBlack, neutralWhite);
        startFace->setCornerColor(neutralWhite);
        startFace->show();

        // Remove only the two original GameCube Start artwork layers identified
        // by the v0.11.26 sbtn_n tree diagnostic. Keep sbtn_n itself and a_btn2
        // intact so the original positioning/alpha animation and Start behavior remain.
        if (J2DPane* oldStartLight = self->mpButtonScreen->search(MULTI_CHAR('a_btn_l2')))
            oldStartLight->hide();
        if (J2DPane* oldStartGlyph = self->mpButtonScreen->search(MULTI_CHAR('a_btn_t1')))
            oldStartGlyph->hide();
    }
}

// The white Wolf glows are not x_light/y_light themselves. They are the
// emphasized-button Pikari drawn by dMeterButton_c::draw().  Control the two
// emphasis slots directly, then restore them after drawing so Preview never
// mutates the game's persistent button state.
HookAction before_meter_button_draw(ModContext*, void* args, void*, void*) {
    s_activeMeterButton = args != nullptr ? mods::arg<dMeterButton_c*>(args, 0) : nullptr;
    if (s_activeMeterButton == nullptr) return HOOK_CONTINUE;

    s_meterButtonGlowState.valid = true;
    for (int i=0;i<2;++i) {
        s_meterButtonGlowState.frame[i] = s_activeMeterButton->field_0x2e8[i];
        s_meterButtonGlowState.button[i] = s_activeMeterButton->field_0x4be[i];

        const ConfigVarHandle wolfXEnabled = layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled);
        const ConfigVarHandle wolfYEnabled = layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled);
        if (s_activeMeterButton->field_0x4be[i] == dMeterButton_c::BUTTON_X_e &&
            !cfg_bool(wolfXEnabled,true))
            s_activeMeterButton->field_0x2e8[i] = 0.0f;
        if (s_activeMeterButton->field_0x4be[i] == dMeterButton_c::BUTTON_Y_e &&
            !cfg_bool(wolfYEnabled,true))
            s_activeMeterButton->field_0x2e8[i] = 0.0f;
    }

    if (cfg_bool(g_wolfGlowPreview,false)) {
        // dMeterButton has exactly two emphasis/Pikari slots. Preview borrows
        // them for X and Y for this draw only, then the post-hook restores all
        // original values.
        s_activeMeterButton->field_0x4be[0] = dMeterButton_c::BUTTON_X_e;
        s_activeMeterButton->field_0x4be[1] = dMeterButton_c::BUTTON_Y_e;
        s_activeMeterButton->field_0x2e8[0] =
            cfg_bool(layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled),true) ? 18.0f : 0.0f;
        s_activeMeterButton->field_0x2e8[1] =
            cfg_bool(layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled),true) ? 18.0f : 0.0f;
    }
    return HOOK_CONTINUE;
}

void after_meter_button_draw(ModContext*, void*, void*, void*) {
    if (s_activeMeterButton != nullptr && s_meterButtonGlowState.valid) {
        for (int i=0;i<2;++i) {
            s_activeMeterButton->field_0x2e8[i] = s_meterButtonGlowState.frame[i];
            s_activeMeterButton->field_0x4be[i] = s_meterButtonGlowState.button[i];
        }
    }
    s_meterButtonGlowState.valid = false;
    s_activeMeterButton = nullptr;
}

int classify_current_pikari() {
    dMeter2Draw_c* meter = s_activeMeter != nullptr ? s_activeMeter : s_meterInstance;
    if (meter == nullptr || meter->mpPikariParent == nullptr) return 0;
    J2DPane* p = meter->mpPikariParent->getPanePtr();
    if (p == nullptr) return 0;

    const float px = p->getTranslateX();
    const float py = p->getTranslateY();

    if (meter->mpBTextA != nullptr) {
        Vec a = meter->mpBTextA->getGlobalVtxCenter(false, 0);
        if (fabsf(px - a.x) < 1.0f && fabsf(py - a.y) < 1.0f) return 1;
    }
    if (meter->mpBTextB != nullptr) {
        Vec b = meter->mpBTextB->getGlobalVtxCenter(false, 0);
        if (fabsf(px - b.x) < 1.0f && fabsf(py - b.y) < 1.0f) return 2;
    }
    // Contextual X/Y (Wolf: Senses/Dig) Pikari is anchored to b_text_x /
    // b_text_y through mpBTextXY[], not to the emphasized-button overlay.
    if (meter->mpBTextXY[0] != nullptr) {
        Vec x = meter->mpBTextXY[0]->getGlobalVtxCenter(false, 0);
        if (fabsf(px - x.x) < 1.5f && fabsf(py - x.y) < 1.5f) return 3;
    }
    if (meter->mpBTextXY[1] != nullptr) {
        Vec y = meter->mpBTextXY[1]->getGlobalVtxCenter(false, 0);
        if (fabsf(px - y.x) < 1.5f && fabsf(py - y.y) < 1.5f) return 4;
    }
    return 0;
}


struct RingDrawTarget {
    J2DPane* anchor = nullptr;
    const ResTIMG* texture = nullptr;
    ConfigVarHandle x = 0;
    ConfigVarHandle y = 0;
    ConfigVarHandle scale = 0;
    bool shoulder = false;
    bool maxSquare = false;
    JGeometry::TBox2<f32> bounds{};
    u8 alpha = 255;
    bool active = false;
};

struct RingHiddenPaneState {
    J2DPane* pane = nullptr;
    bool visible = false;
};

RingDrawTarget s_ringDrawTargets[8];
int s_ringDrawTargetCount = 0;
RingHiddenPaneState s_ringHidden[24];
int s_ringHiddenCount = 0;
dMenu_Ring_c* s_ringDrawOwner = nullptr;
dMenu_Collect2D_c* s_activeCollect = nullptr;

bool pane_effectively_visible(J2DPane* pane) {
    if(pane==nullptr) return false;
    for(J2DPane* p=pane; p!=nullptr; p=p->getParentPane()) {
        if(!p->isVisible() || p->getAlpha()==0) return false;
    }
    return true;
}

u8 pane_effective_alpha(J2DPane* pane) {
    float a=1.0f;
    for(J2DPane* p=pane; p!=nullptr; p=p->getParentPane())
        a*=((float)p->getAlpha()/255.0f);
    if(a<0.0f) a=0.0f;
    if(a>1.0f) a=1.0f;
    return (u8)(a*255.0f);
}

void ring_hide_temporarily(J2DPane* pane) {
    if(pane==nullptr || s_ringHiddenCount >= (int)(sizeof(s_ringHidden)/sizeof(s_ringHidden[0])))
        return;
    for(int i=0;i<s_ringHiddenCount;++i)
        if(s_ringHidden[i].pane==pane) return;

    s_ringHidden[s_ringHiddenCount++] = {pane,pane->isVisible()};
    if(pane->isVisible()) pane->hide();
}

void ring_restore_hidden() {
    for(int i=s_ringHiddenCount-1;i>=0;--i) {
        RingHiddenPaneState& st=s_ringHidden[i];
        if(st.pane==nullptr) continue;
        if(st.visible) st.pane->show(); else st.pane->hide();
        st.pane=nullptr;
    }
    s_ringHiddenCount=0;
}

void ring_add_draw_target(J2DPane* anchor, const ResTIMG* texture,
                          ConfigVarHandle x, ConfigVarHandle y, ConfigVarHandle scale,
                          bool shoulder=false, bool maxSquare=false) {
    if(anchor==nullptr || texture==nullptr || !pane_effectively_visible(anchor) ||
       s_ringDrawTargetCount >= (int)(sizeof(s_ringDrawTargets)/sizeof(s_ringDrawTargets[0])))
        return;

    RingDrawTarget& t=s_ringDrawTargets[s_ringDrawTargetCount++];
    t.anchor=anchor;
    t.texture=texture;
    t.x=x; t.y=y; t.scale=scale;
    t.shoulder=shoulder;
    t.maxSquare=maxSquare;
    t.bounds=anchor->getGlbBounds();
    // The vanilla Item Wheel fades several parent panes even when the
    // controller prompt itself should be fully legible. Multiplying the entire
    // parent alpha chain made our independent PS/Xbox overlays look washed out.
    // Visibility is still respected above; once visible, draw controller art at
    // full opacity just like the original TP Classic textures.
    t.alpha=255;
    t.active=t.bounds.getWidth()>0.0f && t.bounds.getHeight()>0.0f;
}

void ring_collect_root(J2DScreen* screen, u64 tag, const ResTIMG* texture,
                       ConfigVarHandle x, ConfigVarHandle y, ConfigVarHandle scale,
                       bool shoulder=false, bool maxSquare=false) {
    J2DPane* root=screen!=nullptr ? screen->search(tag) : nullptr;
    if(root==nullptr || !pane_effectively_visible(root)) return;

    // Use the actual visible face as the geometry anchor. The previous build
    // used the whole x_btn_n/y_btn_n container, which is wider/larger than the
    // authored button face and made Triangle/Square oversized and displaced.
    // This matches the stable pre-compat Item Wheel sizing/positioning.
    J2DPane* anchor=root;
    if(J2DPicture* face=first_picture_recursive(root))
        anchor=face;

    ring_add_draw_target(anchor,texture,x,y,scale,shoulder,maxSquare);
    // Hide the complete vanilla group only for the original draw, so its GC
    // layers do not show underneath our independent overlay.
    ring_hide_temporarily(root);
}

void ring_collect_exact_group(J2DScreen* screen, u64 faceTag,
                              const u64* hideTags, int hideCount,
                              const ResTIMG* texture,
                              ConfigVarHandle x, ConfigVarHandle y, ConfigVarHandle scale,
                              bool maxSquare=true) {
    if(screen==nullptr) return;
    J2DPane* face=screen->search(faceTag);
    if(face==nullptr || !pane_effectively_visible(face)) return;

    ring_add_draw_target(face,texture,x,y,scale,false,maxSquare);
    ring_hide_temporarily(face);
    for(int i=0;i<hideCount;++i)
        ring_hide_temporarily(screen->search(hideTags[i]));
}

void draw_independent_prompt_overlay(const RingDrawTarget& t) {
    if(!t.active || t.texture==nullptr) return;

    const auto& b=t.bounds;
    const float w=b.getWidth();
    const float h=b.getHeight();
    if(w<=0.0f || h<=0.0f) return;

    const float sc=cfg_scale(t.scale,1.0f);
    float drawW=0.0f, drawH=0.0f;
    if(t.shoulder) {
        drawH=h*sc;
        const float aspect=t.texture->height ? (float)t.texture->width/(float)t.texture->height : 1.0f;
        drawW=drawH*aspect;
    } else {
        const float side=(t.maxSquare ? (w>h?w:h) : (w<h?w:h))*sc;
        drawW=side;
        drawH=side;
    }

    const float cx=b.i.x+w*0.5f+cfg_pos(t.x,0.0f);
    const float cy=b.i.y+h*0.5f+cfg_pos(t.y,0.0f);

    J2DPicture overlay(t.texture);
    const JUtility::TColor black(0,0,0,0), white(255,255,255,255);
    overlay.setBlackWhite(black,white);
    overlay.setCornerColor(white);
    overlay.setAlpha(t.alpha);
    overlay.draw(cx-drawW*0.5f,cy-drawH*0.5f,drawW,drawH,false,false,false);
}

HookAction before_ring_controller_overlay(ModContext*, void* args, void*, void*) {
    dMenu_Ring_c* ring = args != nullptr ? mods::arg<dMenu_Ring_c*>(args,0) : nullptr;
    s_ringDrawOwner=nullptr;
    s_ringDrawTargetCount=0;
    s_ringHiddenCount=0;
    if(ring==nullptr || ring->mpScreen==nullptr) return HOOK_CONTINUE;

    s_ringDrawOwner=ring;

    // The explanation window owns its own state machine. Checking the ring's
    // mStatus is not sufficient: the item-description panel can remain visible
    // while the ring itself has already returned to another status. Because our
    // controller icons are independent overlays, suppress them whenever the
    // actual dMenu_ItemExplain_c window is active. This applies to both human
    // and wolf Item Wheels and covers opening, fully-open and closing states.
    if (ring->mpItemExplain != nullptr && ring->mpItemExplain->getStatus() != 0) {
        s_ringDrawOwner=nullptr;
        return HOOK_CONTINUE;
    }

    // The analog prompts exist in both the human and wolf Item Wheel. Use the
    // same independent overlay path for both so the wolf wheel no longer falls
    // back to the original GameCube C-stick artwork.
    const u64 selectHide[] = {
        MULTI_CHAR('cbtn3'), MULTI_CHAR('cbtn'), MULTI_CHAR('cbtn2')
    };
    ring_collect_exact_group(ring->mpScreen,MULTI_CHAR('cbtn1'),selectHide,3,
                             resource_timg(s_analog),
                             g_wheelSelectAnalogX,g_wheelSelectAnalogY,g_wheelSelectAnalogScale);

    const u64 directHide[] = {
        MULTI_CHAR('cbtn5'), MULTI_CHAR('cbtn6'), MULTI_CHAR('cbtn7')
    };
    ring_collect_exact_group(ring->mpScreen,MULTI_CHAR('cbtn4'),directHide,3,
                             resource_timg(s_analog),
                             g_wheelDirectAnalogX,g_wheelDirectAnalogY,g_wheelDirectAnalogScale);

    // Direct Select L exists in both forms.
    ring_collect_root(ring->mpScreen,MULTI_CHAR('l_btn_n'),resource_timg(s_l2),
                      g_wheelL2X,g_wheelL2Y,g_wheelL2Scale,true);

    // Human Link additionally has X/Y assignment and R bow-combination prompts.
    // Wolf Link intentionally receives only L2 + the two L3 overlays above.
    if(!ring->mPlayerIsWolf) {
        // X (GC) -> Triangle (PS) / Y (XB)
        ring_collect_root(ring->mpScreen,MULTI_CHAR('x_btn_n'),gc_x_face_texture(),
                          layout_handle4(g_wheelSquareX,g_swapWheelSquareX,g_ybWheelSquareX,g_xyxbWheelSquareX),
                          layout_handle4(g_wheelSquareY,g_swapWheelSquareY,g_ybWheelSquareY,g_xyxbWheelSquareY),
                          layout_handle4(g_wheelSquareScale,g_swapWheelSquareScale,g_ybWheelSquareScale,g_xyxbWheelSquareScale));
        // Y (GC) slot; face and calibration may be swapped by the alternate profile.
        ring_collect_root(ring->mpScreen,MULTI_CHAR('y_btn_n'),gc_y_face_texture(),
                          layout_handle4(g_wheelTriangleX,g_swapWheelTriangleX,g_ybWheelTriangleX,g_xyxbWheelTriangleX),
                          layout_handle4(g_wheelTriangleY,g_swapWheelTriangleY,g_ybWheelTriangleY,g_xyxbWheelTriangleY),
                          layout_handle4(g_wheelTriangleScale,g_swapWheelTriangleScale,g_ybWheelTriangleScale,g_xyxbWheelTriangleScale));

        ring_collect_root(ring->mpScreen,MULTI_CHAR('gr_btn_n'),resource_timg(s_r2),
                          g_wheelR2X,g_wheelR2Y,g_wheelR2Scale,true);
        ring_collect_root(ring->mpScreen,MULTI_CHAR('r_btn_n'),resource_timg(s_r2),
                          g_wheelR2X,g_wheelR2Y,g_wheelR2Scale,true);
    }
    return HOOK_CONTINUE;
}

void after_ring_controller_overlay(ModContext*, void* args, void*, void*) {
    dMenu_Ring_c* ring = args != nullptr ? mods::arg<dMenu_Ring_c*>(args,0) : nullptr;
    if(ring==nullptr || ring!=s_ringDrawOwner) {
        ring_restore_hidden();
        s_ringDrawOwner=nullptr;
        s_ringDrawTargetCount=0;
        return;
    }

    // Restore the exact vanilla visibility first. Our modern artwork is then
    // drawn independently, so no JUTTexture/J2DMaterial owned by the wheel is
    // changed and the original GC layers do not remain visible underneath.
    ring_restore_hidden();
    for(int i=0;i<s_ringDrawTargetCount;++i)
        draw_independent_prompt_overlay(s_ringDrawTargets[i]);

    s_ringDrawOwner=nullptr;
    s_ringDrawTargetCount=0;
}

J2DScreen* s_collectPromptScreen = nullptr;

HookAction before_collect_compat_draw(ModContext*, void* args, void*, void*) {
    s_activeCollect = args != nullptr ? mods::arg<dMenu_Collect2D_c*>(args,0) : nullptr;
    s_collectPromptScreen = nullptr;

    // Twilit Essentials injects its custom pages/icons into mpScreen. Leave that
    // tree completely untouched. The controller prompts live on mpScreenIcon,
    // which is a separate vanilla screen, so restore TP Classic's Collection
    // Cross/Circle/ornament there only.
    if (s_activeCollect != nullptr && s_activeCollect->mpScreenIcon != nullptr) {
        s_collectPromptScreen = s_activeCollect->mpScreenIcon;
        begin_menu_prompt_draw(s_collectPromptScreen);
        apply_known_menu_buttons(s_collectPromptScreen);
        prepare_menu_ornament_before_draw(s_collectPromptScreen);
    }
    return HOOK_CONTINUE;
}

void after_collect_compat_draw(ModContext*, void* args, void*, void*) {
    dMenu_Collect2D_c* c = args != nullptr ? mods::arg<dMenu_Collect2D_c*>(args,0) : nullptr;
    if(c==nullptr || c!=s_activeCollect) return;

    // Restore the prompt-only screen immediately after Collection finished
    // drawing. Twilit's mpScreen/custom pages never receive any TP Classic
    // texture/material/tree mutation.
    if (s_collectPromptScreen != nullptr) {
        restore_shared_menu_ornament_after_draw(s_collectPromptScreen);
        restore_menu_prompt_after_draw(s_collectPromptScreen);
        restore_shared_prompt_temp_state(s_collectPromptScreen);
    }
    s_collectPromptScreen=nullptr;
    s_activeCollect=nullptr;
}

bool is_item_wheel_screen(J2DScreen* screen) {
    return screen!=nullptr &&
           screen->search(MULTI_CHAR('fyx_tex'))!=nullptr &&
           screen->search(MULTI_CHAR('x_btn_n'))!=nullptr &&
           screen->search(MULTI_CHAR('y_btn_n'))!=nullptr;
}

bool is_twilit_quick_access_screen(J2DScreen* screen) {
    // Twilit Essentials creates a private J2DScreen from
    // zelda_item_select_icon3_center_parts.blo. These four panes form a
    // reliable signature and are not part of the vanilla item wheel screen.
    return screen!=nullptr &&
           screen->search(MULTI_CHAR('center_n'))!=nullptr &&
           screen->search(MULTI_CHAR('label_n'))!=nullptr &&
           screen->search(MULTI_CHAR('a_itmn_n'))!=nullptr &&
           screen->search(MULTI_CHAR('itemn_n'))!=nullptr;
}

bool collection_draw_active() {
    // dMenu_Collect2D_c::_draw owns not only the main Collection screen but
    // also its Letters/Skills/Fishing/Options/Save submenus. While it is active,
    // never let the generic J2DScreen hook mutate any BLO tree: Twilit's
    // collection-lib reparents/clones panes across these transitions.
    return s_activeCollect!=nullptr;
}

HookAction before_screen_draw(ModContext* ctx, void* args, void* retval, void* userdata) {
    J2DScreen* screen = mods::arg<J2DScreen*>(args, 0);

    if (is_item_wheel_screen(screen) || is_twilit_quick_access_screen(screen)) {
        // Absolute isolation for both the vanilla ring and Twilit Essentials'
        // private Quick Access wheel. Our ring art is drawn independently later.
    } else if (collection_draw_active()) {
        // Strong compatibility mode for the whole Collection family, including
        // Letters/Skills/Fishing/Options/Save submenus. Do not touch textures,
        // visibility, pane order, bounds or transforms while Twilit is actively
        // managing this menu tree.
    } else {
        begin_menu_prompt_draw(screen);
        apply_known_menu_buttons(screen);
        prepare_menu_ornament_before_draw(screen);
    }

    dMeter2Draw_c* meter = s_activeMeter != nullptr ? s_activeMeter : s_meterInstance;
    if (meter == nullptr) return HOOK_CONTINUE;
    if (screen != meter->mpPikariScreen) return HOOK_CONTINUE;

    const int target = classify_current_pikari();
    if (target == 1) {
        mods::arg_ref<f32>(args, 1) += cfg_pos(g_actionGlowX, 0.0f);
        mods::arg_ref<f32>(args, 2) += cfg_pos(g_actionGlowY, 0.0f);

        // drawPikari() has already applied the vanilla Pikari scale at this point.
        // Apply only the user's extra factor immediately before this Pikari screen draw.
        if (meter->mpPikariParent != nullptr) {
            J2DPane* pane = meter->mpPikariParent->getPanePtr();
            if (pane != nullptr) {
                const float factor = cfg_scale(g_actionGlowScale, 1.0f);
                pane->scale(pane->getScaleX() * factor, pane->getScaleY() * factor);
            }
        }
    } else if (target == 2) {
        const ConfigVarHandle backGlowX =
            layout_handle4(g_backGlowX,g_backGlowX,g_ybBackGlowX,g_xyxbBackGlowX);
        const ConfigVarHandle backGlowY =
            layout_handle4(g_backGlowY,g_backGlowY,g_ybBackGlowY,g_xyxbBackGlowY);
        const ConfigVarHandle backGlowScale =
            layout_handle4(g_backGlowScale,g_backGlowScale,g_ybBackGlowScale,g_xyxbBackGlowScale);
        const ConfigVarHandle backGlowEnabled =
            layout_handle4(g_backGlowEnabled,g_backGlowEnabled,g_ybBackGlowEnabled,g_xyxbBackGlowEnabled);

        mods::arg_ref<f32>(args, 1) += cfg_pos(backGlowX, 0.0f);
        mods::arg_ref<f32>(args, 2) += cfg_pos(backGlowY, 0.0f);

        if (meter->mpPikariParent != nullptr) {
            J2DPane* pane = meter->mpPikariParent->getPanePtr();
            if (pane != nullptr) {
                const float factor = cfg_scale(backGlowScale, 1.0f);
                pane->scale(pane->getScaleX() * factor, pane->getScaleY() * factor);
                if (!cfg_bool(backGlowEnabled,true)) pane->setAlpha(0);
            }
        }
    } else if (target == 3 || target == 4) {
        const bool isX = target == 3;
        ConfigVarHandle xh = isX
            ? layout_handle4(g_wolfXGlowX,g_swapWolfXGlowX,g_ybWolfXGlowX,g_xyxbWolfXGlowX)
            : layout_handle4(g_wolfYGlowX,g_swapWolfYGlowX,g_ybWolfYGlowX,g_xyxbWolfYGlowX);
        ConfigVarHandle yh = isX
            ? layout_handle4(g_wolfXGlowY,g_swapWolfXGlowY,g_ybWolfXGlowY,g_xyxbWolfXGlowY)
            : layout_handle4(g_wolfYGlowY,g_swapWolfYGlowY,g_ybWolfYGlowY,g_xyxbWolfYGlowY);
        ConfigVarHandle sh = isX
            ? layout_handle4(g_wolfXGlowScale,g_swapWolfXGlowScale,g_ybWolfXGlowScale,g_xyxbWolfXGlowScale)
            : layout_handle4(g_wolfYGlowScale,g_swapWolfYGlowScale,g_ybWolfYGlowScale,g_xyxbWolfYGlowScale);
        ConfigVarHandle eh = isX
            ? layout_handle4(g_wolfXGlowEnabled,g_swapWolfXGlowEnabled,g_ybWolfXGlowEnabled,g_xyxbWolfXGlowEnabled)
            : layout_handle4(g_wolfYGlowEnabled,g_swapWolfYGlowEnabled,g_ybWolfYGlowEnabled,g_xyxbWolfYGlowEnabled);
        mods::arg_ref<f32>(args, 1) += cfg_pos(xh, 0.0f);
        mods::arg_ref<f32>(args, 2) += cfg_pos(yh, 0.0f);
        if (meter->mpPikariParent != nullptr) {
            J2DPane* pane = meter->mpPikariParent->getPanePtr();
            if (pane != nullptr) {
                const float factor = cfg_scale(sh, 1.0f);
                pane->scale(pane->getScaleX() * factor, pane->getScaleY() * factor);
                if (!cfg_bool(eh,true)) pane->setAlpha(0);
            }
        }
    }
    return HOOK_CONTINUE;
}

void after_screen_draw(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    J2DScreen* screen = mods::arg<J2DScreen*>(args, 0);
    restore_item_wheel_temp_state(screen);
    restore_world_icons(screen);
    restore_world_map_temp_state(screen);
    restore_shared_menu_ornament_after_draw(screen);
    restore_menu_prompt_after_draw(screen);
    restore_shared_prompt_temp_state(screen);
}

ModResult mod_initialize(ModError* error) {
    ModResult styleResult=reg_int("controllerStyle",0,g_controllerStyle,error);
    if(styleResult!=MOD_OK) return styleResult;
    s_controllerStyleLocked=false; s_useXbox=false; s_useSwitch=false;
    // Persisted live layout editor values. X/Y are stored as tenths of a pixel;
    // scale is stored as percent to use Dusklight's native integer steppers.
    struct R { const char* n; int64_t d; ConfigVarHandle* h; };
    R vars[]={
        {"worldR1X",80,&g_worldR1X},
        {"worldR1Y",180,&g_worldR1Y},
        {"worldR1Scale",100,&g_worldR1Scale},
        {"worldAnalogScale",100,&g_worldAnalogScale},
        {"worldDpadScale",100,&g_worldDpadScale},
        {"worldArrowScale",100,&g_worldArrowScale},
        {"WorldPortalTextScale",75,&g_WorldPortalTextScale},
        {"WorldMoveTextScale",75,&g_WorldMoveTextScale},
        {"WorldReturnTextScale",75,&g_WorldReturnTextScale},
        {"worldArrowX",-225,&g_worldArrowX},{"worldArrowY",-230,&g_worldArrowY},
        {"worldAnalogX",180,&g_worldAnalogX},{"worldAnalogY",680,&g_worldAnalogY},{"worldDpadX",260,&g_worldDpadX},{"worldDpadY",220,&g_worldDpadY},
        {"WorldPortalTextX",-130,&g_WorldPortalTextX},{"WorldPortalTextY",0,&g_WorldPortalTextY},
        {"WorldMoveTextX",270,&g_WorldMoveTextX},{"WorldMoveTextY",20,&g_WorldMoveTextY},
        {"WorldReturnTextX",0,&g_WorldReturnTextX},{"WorldReturnTextY",20,&g_WorldReturnTextY},
        {"triX",962,&g_triX},{"triY",387,&g_triY},{"triScale",90,&g_triScale},
        {"squareX",1218,&g_squareX},{"squareY",119,&g_squareY},{"squareScale",90,&g_squareScale},
        {"circleX",1505,&g_circleX},{"circleY",414,&g_circleY},{"circleScale",90,&g_circleScale},
        {"crossX",1174,&g_crossX},{"crossY",597,&g_crossY},{"crossScale",90,&g_crossScale},
        {"fishingCheckX",190,&g_fishingCheckX},{"fishingCheckY",110,&g_fishingCheckY},{"fishingCheckScale",65,&g_fishingCheckScale},
        {"r1X",1620,&g_r1X},{"r1Y",-90,&g_r1Y},{"r1Scale",90,&g_r1Scale},
        {"guideX",870,&g_guideX},{"guideY",30,&g_guideY},{"guideScale",100,&g_guideScale},
        {"dpadX",0,&g_dpadX},{"dpadY",0,&g_dpadY},{"dpadScale",100,&g_dpadScale},
        {"itemTextX",0,&g_itemTextX},{"itemTextY",0,&g_itemTextY},{"itemTextScale",100,&g_itemTextScale},
        {"mapTextX",0,&g_mapTextX},{"mapTextY",0,&g_mapTextY},{"mapTextScale",100,&g_mapTextScale},
        {"itemsAnchorX",0,&g_itemsAnchorX},{"itemsAnchorY",0,&g_itemsAnchorY},
        {"itemSquareX",-430,&g_itemSquareX},{"itemSquareY",-640,&g_itemSquareY},{"itemSquareScale",50,&g_itemSquareScale},
        {"itemTriangleX",220,&g_itemTriangleX},{"itemTriangleY",230,&g_itemTriangleY},{"itemTriangleScale",50,&g_itemTriangleScale},
        {"itemCircleX",0,&g_itemCircleX},{"itemCircleY",0,&g_itemCircleY},{"itemCircleScale",100,&g_itemCircleScale},
        {"itemR1X",0,&g_itemR1X},{"itemR1Y",0,&g_itemR1Y},{"itemR1Scale",100,&g_itemR1Scale},
        {"swordX",83,&g_swordX},{"swordY",-52,&g_swordY},{"swordScale",50,&g_swordScale},
        {"midnaX",80,&g_midnaX},{"midnaY",-90,&g_midnaY},{"midnaScale",65,&g_midnaScale},
        {"howlActionX",440,&g_howlActionX},{"howlActionY",830,&g_howlActionY},{"howlActionScale",65,&g_howlActionScale},
        {"shopActionX",450,&g_shopActionX},{"shopActionY",830,&g_shopActionY},{"shopActionScale",65,&g_shopActionScale},
        {"howlBackX",910,&g_howlBackX},{"howlBackY",300,&g_howlBackY},{"howlBackScale",65,&g_howlBackScale},
        {"shopBackX",900,&g_shopBackX},{"shopBackY",300,&g_shopBackY},{"shopBackScale",65,&g_shopBackScale},
        {"actionTextX",250,&g_actionTextX},{"actionTextY",220,&g_actionTextY},{"actionTextScale",55,&g_actionTextScale},
        {"dialogActionTextX",200,&g_dialogActionTextX},{"dialogActionTextY",480,&g_dialogActionTextY},
        {"whistleActionX",460,&g_whistleActionX},{"whistleActionY",840,&g_whistleActionY},{"whistleActionScale",55,&g_whistleActionScale},
        {"whistleBackX",910,&g_whistleBackX},{"whistleBackY",310,&g_whistleBackY},{"whistleBackScale",55,&g_whistleBackScale},
        {"backTextX",820,&g_backTextX},{"backTextY",-350,&g_backTextY},{"backTextScale",55,&g_backTextScale},
        {"wolfSenseX",-670,&g_wolfSenseX},{"wolfSenseY",-760,&g_wolfSenseY},{"wolfSenseScale",55,&g_wolfSenseScale},
        {"wolfDigX",490,&g_wolfDigX},{"wolfDigY",575,&g_wolfDigY},{"wolfDigScale",55,&g_wolfDigScale},
        {"actionGlowX",-30,&g_actionGlowX},{"actionGlowY",-25,&g_actionGlowY},{"actionGlowScale",50,&g_actionGlowScale},
        {"backGlowX",10,&g_backGlowX},{"backGlowY",30,&g_backGlowY},{"backGlowScale",100,&g_backGlowScale},
        {"wolfXGlowX",5,&g_wolfXGlowX},{"wolfXGlowY",-55,&g_wolfXGlowY},{"wolfXGlowScale",50,&g_wolfXGlowScale},
        {"wolfYGlowX",-70,&g_wolfYGlowX},{"wolfYGlowY",20,&g_wolfYGlowY},{"wolfYGlowScale",50,&g_wolfYGlowScale},
        {"fileCrossX",15,&g_fileCrossX},{"fileCrossY",0,&g_fileCrossY},
        {"fileCircleX",10,&g_fileCircleX},{"fileCircleY",30,&g_fileCircleY},
        {"saveCrossX",15,&g_saveCrossX},{"saveCrossY",0,&g_saveCrossY},
        {"saveCircleX",20,&g_saveCircleX},{"saveCircleY",25,&g_saveCircleY},
        {"menuCrossX",-228,&g_menuCrossX},{"menuCrossY",260,&g_menuCrossY},{"menuCrossScale",70,&g_menuCrossScale},
        {"menuCircleX",110,&g_menuCircleX},{"menuCircleY",-240,&g_menuCircleY},{"menuCircleScale",100,&g_menuCircleScale},
        {"menuConfirmTextX",-140,&g_menuConfirmTextX},{"menuConfirmTextY",280,&g_menuConfirmTextY},{"menuConfirmTextScale",60,&g_menuConfirmTextScale},
        {"menuBackTextX",170,&g_menuBackTextX},{"menuBackTextY",-220,&g_menuBackTextY},{"menuBackTextScale",60,&g_menuBackTextScale},
        {"menuOrnamentX",-290,&g_menuOrnamentX},{"menuOrnamentY",50,&g_menuOrnamentY},{"menuOrnamentScale",75,&g_menuOrnamentScale},
        {"mapCrossX",-220,&g_mapCrossX},{"mapCrossY",238,&g_mapCrossY},{"mapCrossScale",75,&g_mapCrossScale},
        {"mapCircleX",119,&g_mapCircleX},{"mapCircleY",-243,&g_mapCircleY},{"mapCircleScale",90,&g_mapCircleScale},
        {"mapConfirmTextX",-120,&g_mapConfirmTextX},{"mapConfirmTextY",270,&g_mapConfirmTextY},{"mapConfirmTextScale",75,&g_mapConfirmTextScale},
        {"mapBackTextX",230,&g_mapBackTextX},{"mapBackTextY",-240,&g_mapBackTextY},{"mapBackTextScale",75,&g_mapBackTextScale},
        {"mapOrnamentX",-300,&g_mapOrnamentX},{"mapOrnamentY",0,&g_mapOrnamentY},{"mapOrnamentScale",75,&g_mapOrnamentScale},
        {"hudOrnamentX",20,&g_hudOrnamentX},{"hudOrnamentY",20,&g_hudOrnamentY},{"hudOrnamentScale",100,&g_hudOrnamentScale},
        {"wheelSquareX",-170,&g_wheelSquareX},{"wheelSquareY",-10,&g_wheelSquareY},{"wheelSquareScale",87,&g_wheelSquareScale},
        {"wheelTriangleX",-100,&g_wheelTriangleX},{"wheelTriangleY",-40,&g_wheelTriangleY},{"wheelTriangleScale",90,&g_wheelTriangleScale},
        {"wheelSelectAnalogX",50,&g_wheelSelectAnalogX},{"wheelSelectAnalogY",0,&g_wheelSelectAnalogY},{"wheelSelectAnalogScale",90,&g_wheelSelectAnalogScale},
        {"wheelDirectAnalogX",100,&g_wheelDirectAnalogX},{"wheelDirectAnalogY",0,&g_wheelDirectAnalogY},{"wheelDirectAnalogScale",90,&g_wheelDirectAnalogScale},
        {"wheelL2X",250,&g_wheelL2X},{"wheelL2Y",-10,&g_wheelL2Y},{"wheelL2Scale",90,&g_wheelL2Scale},
        {"wheelR2X",-130,&g_wheelR2X},{"wheelR2Y",0,&g_wheelR2Y},{"wheelR2Scale",100,&g_wheelR2Scale},
    };
    for (auto& v:vars) {
        ModResult rr=reg_int(v.n,v.d,*v.h,error);
        if(rr!=MOD_OK) return rr;
    }
    {
        ModResult rr=reg_bool("showGuide",false,g_showGuide,error);
        rr=reg_bool("dpadShadowsEnabled",true,g_dpadShadowsEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("worldMapArrowsEnabled",true,g_worldArrows,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("dpadArrowsEnabled",true,g_dpadArrowsEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("dpadMapAnimation",true,g_dpadMapAnimation,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("actionGlowEnabled",true,g_actionGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("backGlowEnabled",true,g_backGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("glowAdjustmentPreview",false,g_glowPreview,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("wolfXGlowEnabled",false,g_wolfXGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("wolfYGlowEnabled",false,g_wolfYGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("wolfGlowPreview",false,g_wolfGlowPreview,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("backButtonAnimation",false,g_backButtonAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("backTextAnimation",false,g_backTextAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("menuPromptOrnament",true,g_menuPromptOrnament,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("mapOrnamentEnabled",true,g_mapOrnamentEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("hudOrnamentEnabled",true,g_hudOrnamentEnabled,error); if(rr!=MOD_OK) return rr;
        if(rr!=MOD_OK) return rr;
        rr=reg_bool("itemSquareFlipH",false,g_itemSquareFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("itemSquareFlipV",false,g_itemSquareFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("itemTriangleFlipH",false,g_itemTriangleFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("itemTriangleFlipV",false,g_itemTriangleFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swordFlipH",false,g_swordFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swordFlipV",false,g_swordFlipV,error); if(rr!=MOD_OK) return rr;
    }
    // Dungeon defaults must be constant: unsaved defaults must never track world-map edits.
    // Existing map keys remain the world-map settings.
    R dungeonVars[]={
        {"dungeonMapCrossX",-120,&g_dungeonMapCrossX},
        {"dungeonMapCrossY",30,&g_dungeonMapCrossY},
        {"dungeonMapCrossScale",70,&g_dungeonMapCrossScale},
        {"dungeonMapCircleX",0,&g_dungeonMapCircleX},
        {"dungeonMapCircleY",-10,&g_dungeonMapCircleY},
        {"dungeonMapCircleScale",100,&g_dungeonMapCircleScale},
        {"dungeonMapConfirmTextX",-50,&g_dungeonMapConfirmTextX},
        {"dungeonMapConfirmTextY",60,&g_dungeonMapConfirmTextY},
        {"dungeonMapConfirmTextScale",75,&g_dungeonMapConfirmTextScale},
        {"dungeonMapBackTextX",70,&g_dungeonMapBackTextX},
        {"dungeonMapBackTextY",10,&g_dungeonMapBackTextY},
        {"dungeonMapBackTextScale",75,&g_dungeonMapBackTextScale},
    };
    for (auto& v:dungeonVars) {
        ModResult rr=reg_int(v.n,v.d,*v.h,error);
        if(rr!=MOD_OK) return rr;
    }


    // Switch has a complete independent copy of Base/shared visual values.
    // It starts from the accepted layout, then can be calibrated without
    // mutating PlayStation/Xbox configuration.
    {
        R switchVars[]={
            {"switch_worldR1X",80,&g_swWorldR1X},
            {"switch_worldR1Y",180,&g_swWorldR1Y},
            {"switch_worldR1Scale",100,&g_swWorldR1Scale},
            {"switch_worldAnalogScale",100,&g_swWorldAnalogScale},
            {"switch_worldDpadScale",100,&g_swWorldDpadScale},
            {"switch_worldArrowScale",100,&g_swWorldArrowScale},
            {"switch_WorldPortalTextScale",75,&g_swWorldPortalTextScale},
            {"switch_WorldMoveTextScale",75,&g_swWorldMoveTextScale},
            {"switch_WorldReturnTextScale",75,&g_swWorldReturnTextScale},
            {"switch_worldArrowX",-225,&g_swWorldArrowX},
            {"switch_worldArrowY",-230,&g_swWorldArrowY},
            {"switch_worldAnalogX",180,&g_swWorldAnalogX},
            {"switch_worldAnalogY",680,&g_swWorldAnalogY},
            {"switch_worldDpadX",260,&g_swWorldDpadX},
            {"switch_worldDpadY",220,&g_swWorldDpadY},
            {"switch_WorldPortalTextX",-130,&g_swWorldPortalTextX},
            {"switch_WorldPortalTextY",0,&g_swWorldPortalTextY},
            {"switch_WorldMoveTextX",270,&g_swWorldMoveTextX},
            {"switch_WorldMoveTextY",20,&g_swWorldMoveTextY},
            {"switch_WorldReturnTextX",0,&g_swWorldReturnTextX},
            {"switch_WorldReturnTextY",20,&g_swWorldReturnTextY},
            {"switch_triX",962,&g_swTriX},
            {"switch_triY",387,&g_swTriY},
            {"switch_triScale",90,&g_swTriScale},
            {"switch_squareX",1218,&g_swSquareX},
            {"switch_squareY",119,&g_swSquareY},
            {"switch_squareScale",90,&g_swSquareScale},
            {"switch_circleX",1245,&g_swCircleX},
            {"switch_circleY",665,&g_swCircleY},
            {"switch_circleScale",90,&g_swCircleScale},
            {"switch_crossX",1432,&g_swCrossX},
            {"switch_crossY",343,&g_swCrossY},
            {"switch_crossScale",90,&g_swCrossScale},
            {"switch_fishingCheckX",190,&g_swFishingCheckX},
            {"switch_fishingCheckY",110,&g_swFishingCheckY},
            {"switch_fishingCheckScale",65,&g_swFishingCheckScale},
            {"switch_r1X",1620,&g_swR1X},
            {"switch_r1Y",-90,&g_swR1Y},
            {"switch_r1Scale",90,&g_swR1Scale},
            {"switch_guideX",870,&g_swGuideX},
            {"switch_guideY",30,&g_swGuideY},
            {"switch_guideScale",100,&g_swGuideScale},
            {"switch_dpadX",0,&g_swDpadX},
            {"switch_dpadY",0,&g_swDpadY},
            {"switch_dpadScale",100,&g_swDpadScale},
            {"switch_itemTextX",0,&g_swItemTextX},
            {"switch_itemTextY",0,&g_swItemTextY},
            {"switch_itemTextScale",100,&g_swItemTextScale},
            {"switch_mapTextX",0,&g_swMapTextX},
            {"switch_mapTextY",0,&g_swMapTextY},
            {"switch_mapTextScale",100,&g_swMapTextScale},
            {"switch_itemsAnchorX",0,&g_swItemsAnchorX},
            {"switch_itemsAnchorY",0,&g_swItemsAnchorY},
            {"switch_itemSquareX",-430,&g_swItemSquareX},
            {"switch_itemSquareY",-640,&g_swItemSquareY},
            {"switch_itemSquareScale",50,&g_swItemSquareScale},
            {"switch_itemTriangleX",220,&g_swItemTriangleX},
            {"switch_itemTriangleY",230,&g_swItemTriangleY},
            {"switch_itemTriangleScale",50,&g_swItemTriangleScale},
            {"switch_itemCircleX",0,&g_swItemCircleX},
            {"switch_itemCircleY",0,&g_swItemCircleY},
            {"switch_itemCircleScale",100,&g_swItemCircleScale},
            {"switch_itemR1X",0,&g_swItemR1X},
            {"switch_itemR1Y",0,&g_swItemR1Y},
            {"switch_itemR1Scale",100,&g_swItemR1Scale},
            {"switch_swordX",40,&g_swSwordX},
            {"switch_swordY",-13,&g_swSwordY},
            {"switch_swordScale",50,&g_swSwordScale},
            {"switch_midnaX",80,&g_swMidnaX},
            {"switch_midnaY",-90,&g_swMidnaY},
            {"switch_midnaScale",65,&g_swMidnaScale},
            {"switch_howlActionX",710,&g_swHowlActionX},
            {"switch_howlActionY",580,&g_swHowlActionY},
            {"switch_howlActionScale",65,&g_swHowlActionScale},
            {"switch_shopActionX",700,&g_swShopActionX},
            {"switch_shopActionY",570,&g_swShopActionY},
            {"switch_shopActionScale",65,&g_swShopActionScale},
            {"switch_howlBackX",640,&g_swHowlBackX},
            {"switch_howlBackY",550,&g_swHowlBackY},
            {"switch_howlBackScale",65,&g_swHowlBackScale},
            {"switch_shopBackX",640,&g_swShopBackX},
            {"switch_shopBackY",550,&g_swShopBackY},
            {"switch_shopBackScale",65,&g_swShopBackScale},
            {"switch_actionTextX",520,&g_swActionTextX},
            {"switch_actionTextY",-35,&g_swActionTextY},
            {"switch_actionTextScale",55,&g_swActionTextScale},
            {"switch_dialogActionTextX",200,&g_swDialogActionTextX},
            {"switch_dialogActionTextY",480,&g_swDialogActionTextY},
            {"switch_whistleActionX",710,&g_swWhistleActionX},
            {"switch_whistleActionY",585,&g_swWhistleActionY},
            {"switch_whistleActionScale",55,&g_swWhistleActionScale},
            {"switch_whistleBackX",640,&g_swWhistleBackX},
            {"switch_whistleBackY",555,&g_swWhistleBackY},
            {"switch_whistleBackScale",55,&g_swWhistleBackScale},
            {"switch_backTextX",550,&g_swBackTextX},
            {"switch_backTextY",-90,&g_swBackTextY},
            {"switch_backTextScale",55,&g_swBackTextScale},
            {"switch_wolfSenseX",-670,&g_swWolfSenseX},
            {"switch_wolfSenseY",-760,&g_swWolfSenseY},
            {"switch_wolfSenseScale",55,&g_swWolfSenseScale},
            {"switch_wolfDigX",490,&g_swWolfDigX},
            {"switch_wolfDigY",575,&g_swWolfDigY},
            {"switch_wolfDigScale",55,&g_swWolfDigScale},
            {"switch_actionGlowX",-30,&g_swActionGlowX},
            {"switch_actionGlowY",-25,&g_swActionGlowY},
            {"switch_actionGlowScale",50,&g_swActionGlowScale},
            {"switch_backGlowX",10,&g_swBackGlowX},
            {"switch_backGlowY",30,&g_swBackGlowY},
            {"switch_backGlowScale",100,&g_swBackGlowScale},
            {"switch_wolfXGlowX",5,&g_swWolfXGlowX},
            {"switch_wolfXGlowY",-55,&g_swWolfXGlowY},
            {"switch_wolfXGlowScale",50,&g_swWolfXGlowScale},
            {"switch_wolfYGlowX",-70,&g_swWolfYGlowX},
            {"switch_wolfYGlowY",20,&g_swWolfYGlowY},
            {"switch_wolfYGlowScale",50,&g_swWolfYGlowScale},
            {"switch_fileCrossX",15,&g_swFileCrossX},
            {"switch_fileCrossY",0,&g_swFileCrossY},
            {"switch_fileCircleX",10,&g_swFileCircleX},
            {"switch_fileCircleY",30,&g_swFileCircleY},
            {"switch_saveCrossX",15,&g_swSaveCrossX},
            {"switch_saveCrossY",0,&g_swSaveCrossY},
            {"switch_saveCircleX",20,&g_swSaveCircleX},
            {"switch_saveCircleY",25,&g_swSaveCircleY},
            {"switch_menuCrossX",-122,&g_swMenuCrossX},
            {"switch_menuCrossY",30,&g_swMenuCrossY},
            {"switch_menuCrossScale",70,&g_swMenuCrossScale},
            {"switch_menuCircleX",3,&g_swMenuCircleX},
            {"switch_menuCircleY",-10,&g_swMenuCircleY},
            {"switch_menuCircleScale",100,&g_swMenuCircleScale},
            {"switch_menuConfirmTextX",-40,&g_swMenuConfirmTextX},
            {"switch_menuConfirmTextY",60,&g_swMenuConfirmTextY},
            {"switch_menuConfirmTextScale",60,&g_swMenuConfirmTextScale},
            {"switch_menuBackTextX",60,&g_swMenuBackTextX},
            {"switch_menuBackTextY",10,&g_swMenuBackTextY},
            {"switch_menuBackTextScale",60,&g_swMenuBackTextScale},
            {"switch_menuOrnamentX",-290,&g_swMenuOrnamentX},
            {"switch_menuOrnamentY",50,&g_swMenuOrnamentY},
            {"switch_menuOrnamentScale",75,&g_swMenuOrnamentScale},
            {"switch_mapCrossX",-115,&g_swMapCrossX},
            {"switch_mapCrossY",8,&g_swMapCrossY},
            {"switch_mapCrossScale",75,&g_swMapCrossScale},
            {"switch_mapCircleX",15,&g_swMapCircleX},
            {"switch_mapCircleY",-13,&g_swMapCircleY},
            {"switch_mapCircleScale",90,&g_swMapCircleScale},
            {"switch_mapConfirmTextX",-20,&g_swMapConfirmTextX},
            {"switch_mapConfirmTextY",20,&g_swMapConfirmTextY},
            {"switch_mapConfirmTextScale",75,&g_swMapConfirmTextScale},
            {"switch_mapBackTextX",100,&g_swMapBackTextX},
            {"switch_mapBackTextY",0,&g_swMapBackTextY},
            {"switch_mapBackTextScale",75,&g_swMapBackTextScale},
            {"switch_mapOrnamentX",-300,&g_swMapOrnamentX},
            {"switch_mapOrnamentY",0,&g_swMapOrnamentY},
            {"switch_mapOrnamentScale",75,&g_swMapOrnamentScale},
            {"switch_hudOrnamentX",20,&g_swHudOrnamentX},
            {"switch_hudOrnamentY",20,&g_swHudOrnamentY},
            {"switch_hudOrnamentScale",100,&g_swHudOrnamentScale},
            {"switch_wheelSquareX",-170,&g_swWheelSquareX},
            {"switch_wheelSquareY",-10,&g_swWheelSquareY},
            {"switch_wheelSquareScale",87,&g_swWheelSquareScale},
            {"switch_wheelTriangleX",-100,&g_swWheelTriangleX},
            {"switch_wheelTriangleY",-40,&g_swWheelTriangleY},
            {"switch_wheelTriangleScale",90,&g_swWheelTriangleScale},
            {"switch_wheelSelectAnalogX",50,&g_swWheelSelectAnalogX},
            {"switch_wheelSelectAnalogY",0,&g_swWheelSelectAnalogY},
            {"switch_wheelSelectAnalogScale",90,&g_swWheelSelectAnalogScale},
            {"switch_wheelDirectAnalogX",100,&g_swWheelDirectAnalogX},
            {"switch_wheelDirectAnalogY",0,&g_swWheelDirectAnalogY},
            {"switch_wheelDirectAnalogScale",90,&g_swWheelDirectAnalogScale},
            {"switch_wheelL2X",250,&g_swWheelL2X},
            {"switch_wheelL2Y",-10,&g_swWheelL2Y},
            {"switch_wheelL2Scale",90,&g_swWheelL2Scale},
            {"switch_wheelR2X",-130,&g_swWheelR2X},
            {"switch_wheelR2Y",0,&g_swWheelR2Y},
            {"switch_wheelR2Scale",100,&g_swWheelR2Scale},
            {"switch_dungeonMapCrossX",-120,&g_swDungeonMapCrossX},
            {"switch_dungeonMapCrossY",30,&g_swDungeonMapCrossY},
            {"switch_dungeonMapCrossScale",70,&g_swDungeonMapCrossScale},
            {"switch_dungeonMapCircleX",0,&g_swDungeonMapCircleX},
            {"switch_dungeonMapCircleY",-10,&g_swDungeonMapCircleY},
            {"switch_dungeonMapCircleScale",100,&g_swDungeonMapCircleScale},
            {"switch_dungeonMapConfirmTextX",-50,&g_swDungeonMapConfirmTextX},
            {"switch_dungeonMapConfirmTextY",60,&g_swDungeonMapConfirmTextY},
            {"switch_dungeonMapConfirmTextScale",75,&g_swDungeonMapConfirmTextScale},
            {"switch_dungeonMapBackTextX",70,&g_swDungeonMapBackTextX},
            {"switch_dungeonMapBackTextY",10,&g_swDungeonMapBackTextY},
            {"switch_dungeonMapBackTextScale",75,&g_swDungeonMapBackTextScale}
        };
        for (auto& v:switchVars) {
            ModResult rr=reg_int(v.n,v.d,*v.h,error);
            if(rr!=MOD_OK) return rr;
        }
        ModResult rr=MOD_OK;
        rr=reg_bool("switch_dpadShadowsEnabled",true,g_swDpadShadowsEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_worldMapArrowsEnabled",true,g_swWorldArrows,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_dpadArrowsEnabled",true,g_swDpadArrowsEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_dpadMapAnimation",true,g_swDpadMapAnimation,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_actionGlowEnabled",true,g_swActionGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_backGlowEnabled",true,g_swBackGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_glowAdjustmentPreview",false,g_swGlowPreview,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_wolfXGlowEnabled",true,g_swWolfXGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_wolfYGlowEnabled",true,g_swWolfYGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_wolfGlowPreview",false,g_swWolfGlowPreview,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_backButtonAnimation",false,g_swBackButtonAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_backTextAnimation",false,g_swBackTextAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_menuPromptOrnament",true,g_swMenuPromptOrnament,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_mapOrnamentEnabled",true,g_swMapOrnamentEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_hudOrnamentEnabled",true,g_swHudOrnamentEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_itemSquareFlipH",false,g_swItemSquareFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_itemSquareFlipV",false,g_swItemSquareFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_itemTriangleFlipH",false,g_swItemTriangleFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_itemTriangleFlipV",false,g_swItemTriangleFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_swordFlipH",false,g_swSwordFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("switch_swordFlipV",false,g_swSwordFlipV,error); if(rr!=MOD_OK) return rr;
    }

    // Hidden migration marker: not exposed in the UI, only persisted in config.json.
    {
        ModResult rr=reg_int("layoutSchemaVersion",0,g_layoutSchemaVersion,error);
        if(rr!=MOD_OK) return rr;
    }
    {
        ModResult rr=MOD_OK;
        if (kDeveloperOptions) {
            rr=reg_bool("visualHudEditorEnabled",false,g_visualHudEditorEnabled,error);
            if(rr!=MOD_OK) return rr;
            rr=reg_bool("swapXYButtonLayout",false,g_swapXYButtonLayout,error);
            if(rr!=MOD_OK) return rr;
        }
        rr=reg_bool("swapBackButtonAnim",false,g_swapBackButtonAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapBackTextAnim",false,g_swapBackTextAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybBackButtonAnim",false,g_ybBackButtonAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybBackTextAnim",false,g_ybBackTextAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbBackButtonAnim",false,g_xyxbBackButtonAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbBackTextAnim",false,g_xyxbBackTextAnim,error); if(rr!=MOD_OK) return rr;
        rr=reg_int("buttonLayoutPreset",0,g_buttonLayoutPreset,error);
        if(rr!=MOD_OK) return rr;
        rr=reg_int("inputSyncLastPreset",-1,g_inputSyncLastPreset,error);
        if(rr!=MOD_OK) return rr;
        rr=reg_int("inputSyncControllerIndex",-1,g_inputSyncControllerIndex,error);
        if(rr!=MOD_OK) return rr;

        struct DevInt { const char* n; int64_t d; ConfigVarHandle* h; };
        DevInt swapVars[] = {
            {"swapTriX",1210,&g_swapTriX},{"swapTriY",126,&g_swapTriY},{"swapTriScale",90,&g_swapTriScale},
            {"swapSquareX",972,&g_swapSquareX},{"swapSquareY",379,&g_swapSquareY},{"swapSquareScale",90,&g_swapSquareScale},
            {"swapItemSquareX",-670,&g_swapItemSquareX},{"swapItemSquareY",-380,&g_swapItemSquareY},{"swapItemSquareScale",50,&g_swapItemSquareScale},
            {"swapItemTriangleX",480,&g_swapItemTriangleX},{"swapItemTriangleY",-30,&g_swapItemTriangleY},{"swapItemTriangleScale",50,&g_swapItemTriangleScale},
            {"swapDialogActionTextX",200,&g_swapDialogActionTextX},{"swapDialogActionTextY",480,&g_swapDialogActionTextY},
            {"swapWhistleActionX",460,&g_swapWhistleActionX},{"swapWhistleActionY",840,&g_swapWhistleActionY},{"swapWhistleActionScale",55,&g_swapWhistleActionScale},
            {"swapWhistleBackX",910,&g_swapWhistleBackX},{"swapWhistleBackY",310,&g_swapWhistleBackY},{"swapWhistleBackScale",55,&g_swapWhistleBackScale},
            {"swapHowlActionX",440,&g_swapHowlActionX},{"swapHowlActionY",830,&g_swapHowlActionY},{"swapHowlActionScale",65,&g_swapHowlActionScale},
            {"swapHowlBackX",910,&g_swapHowlBackX},{"swapHowlBackY",300,&g_swapHowlBackY},{"swapHowlBackScale",65,&g_swapHowlBackScale},
            {"swapShopActionX",450,&g_swapShopActionX},{"swapShopActionY",830,&g_swapShopActionY},{"swapShopActionScale",65,&g_swapShopActionScale},
            {"swapShopBackX",900,&g_swapShopBackX},{"swapShopBackY",300,&g_swapShopBackY},{"swapShopBackScale",65,&g_swapShopBackScale},
            {"swapWolfSenseX",-920,&g_swapWolfSenseX},{"swapWolfSenseY",-500,&g_swapWolfSenseY},{"swapWolfSenseScale",55,&g_swapWolfSenseScale},
            {"swapWolfDigX",730,&g_swapWolfDigX},{"swapWolfDigY",310,&g_swapWolfDigY},{"swapWolfDigScale",55,&g_swapWolfDigScale},
            {"swapWolfXGlowX",5,&g_swapWolfXGlowX},{"swapWolfXGlowY",-55,&g_swapWolfXGlowY},{"swapWolfXGlowScale",50,&g_swapWolfXGlowScale},
            {"swapWolfYGlowX",-75,&g_swapWolfYGlowX},{"swapWolfYGlowY",20,&g_swapWolfYGlowY},{"swapWolfYGlowScale",50,&g_swapWolfYGlowScale},
            {"swapWheelSquareX",-430,&g_swapWheelSquareX},{"swapWheelSquareY",-10,&g_swapWheelSquareY},{"swapWheelSquareScale",87,&g_swapWheelSquareScale},
            {"swapWheelTriangleX",160,&g_swapWheelTriangleX},{"swapWheelTriangleY",-40,&g_swapWheelTriangleY},{"swapWheelTriangleScale",90,&g_swapWheelTriangleScale},

            // Swap Y/B starts from the calibrated Base preset.
            {"ybTriX",1467,&g_ybTriX},{"ybTriY",387,&g_ybTriY},{"ybTriScale",90,&g_ybTriScale},
            {"ybSquareX",1218,&g_ybSquareX},{"ybSquareY",119,&g_ybSquareY},{"ybSquareScale",90,&g_ybSquareScale},
            {"ybCircleX",997,&g_ybCircleX},{"ybCircleY",414,&g_ybCircleY},{"ybCircleScale",90,&g_ybCircleScale},
            {"ybItemSquareX",-430,&g_ybItemSquareX},{"ybItemSquareY",-640,&g_ybItemSquareY},{"ybItemSquareScale",50,&g_ybItemSquareScale},
            {"ybItemTriangleX",880,&g_ybItemTriangleX},{"ybItemTriangleY",230,&g_ybItemTriangleY},{"ybItemTriangleScale",50,&g_ybItemTriangleScale},
            {"ybSwordX",14,&g_ybSwordX},{"ybSwordY",-40,&g_ybSwordY},{"ybSwordScale",50,&g_ybSwordScale},
            {"ybBackTextX",310,&g_ybBackTextX},{"ybBackTextY",-345,&g_ybBackTextY},{"ybBackTextScale",55,&g_ybBackTextScale},
            {"ybBackGlowX",10,&g_ybBackGlowX},{"ybBackGlowY",30,&g_ybBackGlowY},{"ybBackGlowScale",100,&g_ybBackGlowScale},
            {"ybDialogActionTextX",200,&g_ybDialogActionTextX},{"ybDialogActionTextY",480,&g_ybDialogActionTextY},
            {"ybWhistleActionX",460,&g_ybWhistleActionX},{"ybWhistleActionY",840,&g_ybWhistleActionY},{"ybWhistleActionScale",55,&g_ybWhistleActionScale},
            {"ybWhistleBackX",410,&g_ybWhistleBackX},{"ybWhistleBackY",300,&g_ybWhistleBackY},{"ybWhistleBackScale",55,&g_ybWhistleBackScale},
            {"ybHowlActionX",440,&g_ybHowlActionX},{"ybHowlActionY",830,&g_ybHowlActionY},{"ybHowlActionScale",65,&g_ybHowlActionScale},
            {"ybHowlBackX",400,&g_ybHowlBackX},{"ybHowlBackY",300,&g_ybHowlBackY},{"ybHowlBackScale",65,&g_ybHowlBackScale},
            {"ybShopActionX",450,&g_ybShopActionX},{"ybShopActionY",830,&g_ybShopActionY},{"ybShopActionScale",65,&g_ybShopActionScale},
            {"ybShopBackX",400,&g_ybShopBackX},{"ybShopBackY",300,&g_ybShopBackY},{"ybShopBackScale",65,&g_ybShopBackScale},
            {"ybWolfSenseX",-670,&g_ybWolfSenseX},{"ybWolfSenseY",-760,&g_ybWolfSenseY},{"ybWolfSenseScale",55,&g_ybWolfSenseScale},
            {"ybWolfDigX",1000,&g_ybWolfDigX},{"ybWolfDigY",575,&g_ybWolfDigY},{"ybWolfDigScale",55,&g_ybWolfDigScale},
            {"ybWolfXGlowX",5,&g_ybWolfXGlowX},{"ybWolfXGlowY",-55,&g_ybWolfXGlowY},{"ybWolfXGlowScale",50,&g_ybWolfXGlowScale},
            {"ybWolfYGlowX",-70,&g_ybWolfYGlowX},{"ybWolfYGlowY",20,&g_ybWolfYGlowY},{"ybWolfYGlowScale",50,&g_ybWolfYGlowScale},
            {"ybWheelSquareX",-430,&g_ybWheelSquareX},{"ybWheelSquareY",-10,&g_ybWheelSquareY},{"ybWheelSquareScale",87,&g_ybWheelSquareScale},
            {"ybWheelTriangleX",160,&g_ybWheelTriangleX},{"ybWheelTriangleY",-40,&g_ybWheelTriangleY},{"ybWheelTriangleScale",90,&g_ybWheelTriangleScale},

            // Swap X/Y + X/B starts from the calibrated Swap X/Y preset.
            {"xyxbTriX",1208,&g_xyxbTriX},{"xyxbTriY",126,&g_xyxbTriY},{"xyxbTriScale",90,&g_xyxbTriScale},
            {"xyxbSquareX",1477,&g_xyxbSquareX},{"xyxbSquareY",379,&g_xyxbSquareY},{"xyxbSquareScale",90,&g_xyxbSquareScale},
            {"xyxbCircleX",997,&g_xyxbCircleX},{"xyxbCircleY",414,&g_xyxbCircleY},{"xyxbCircleScale",90,&g_xyxbCircleScale},
            {"xyxbItemSquareX",-30,&g_xyxbItemSquareX},{"xyxbItemSquareY",-380,&g_xyxbItemSquareY},{"xyxbItemSquareScale",50,&g_xyxbItemSquareScale},
            {"xyxbItemTriangleX",480,&g_xyxbItemTriangleX},{"xyxbItemTriangleY",-30,&g_xyxbItemTriangleY},{"xyxbItemTriangleScale",50,&g_xyxbItemTriangleScale},
            {"xyxbSwordX",14,&g_xyxbSwordX},{"xyxbSwordY",-40,&g_xyxbSwordY},{"xyxbSwordScale",50,&g_xyxbSwordScale},
            {"xyxbBackTextX",310,&g_xyxbBackTextX},{"xyxbBackTextY",-345,&g_xyxbBackTextY},{"xyxbBackTextScale",55,&g_xyxbBackTextScale},
            {"xyxbBackGlowX",10,&g_xyxbBackGlowX},{"xyxbBackGlowY",30,&g_xyxbBackGlowY},{"xyxbBackGlowScale",100,&g_xyxbBackGlowScale},
            {"xyxbDialogActionTextX",200,&g_xyxbDialogActionTextX},{"xyxbDialogActionTextY",480,&g_xyxbDialogActionTextY},
            {"xyxbWhistleActionX",460,&g_xyxbWhistleActionX},{"xyxbWhistleActionY",840,&g_xyxbWhistleActionY},{"xyxbWhistleActionScale",55,&g_xyxbWhistleActionScale},
            {"xyxbWhistleBackX",410,&g_xyxbWhistleBackX},{"xyxbWhistleBackY",300,&g_xyxbWhistleBackY},{"xyxbWhistleBackScale",55,&g_xyxbWhistleBackScale},
            {"xyxbHowlActionX",440,&g_xyxbHowlActionX},{"xyxbHowlActionY",830,&g_xyxbHowlActionY},{"xyxbHowlActionScale",65,&g_xyxbHowlActionScale},
            {"xyxbHowlBackX",400,&g_xyxbHowlBackX},{"xyxbHowlBackY",300,&g_xyxbHowlBackY},{"xyxbHowlBackScale",65,&g_xyxbHowlBackScale},
            {"xyxbShopActionX",450,&g_xyxbShopActionX},{"xyxbShopActionY",830,&g_xyxbShopActionY},{"xyxbShopActionScale",65,&g_xyxbShopActionScale},
            {"xyxbShopBackX",400,&g_xyxbShopBackX},{"xyxbShopBackY",300,&g_xyxbShopBackY},{"xyxbShopBackScale",65,&g_xyxbShopBackScale},
            {"xyxbWolfSenseX",-400,&g_xyxbWolfSenseX},{"xyxbWolfSenseY",-500,&g_xyxbWolfSenseY},{"xyxbWolfSenseScale",55,&g_xyxbWolfSenseScale},
            {"xyxbWolfDigX",730,&g_xyxbWolfDigX},{"xyxbWolfDigY",310,&g_xyxbWolfDigY},{"xyxbWolfDigScale",55,&g_xyxbWolfDigScale},
            {"xyxbWolfXGlowX",5,&g_xyxbWolfXGlowX},{"xyxbWolfXGlowY",-55,&g_xyxbWolfXGlowY},{"xyxbWolfXGlowScale",50,&g_xyxbWolfXGlowScale},
            {"xyxbWolfYGlowX",-75,&g_xyxbWolfYGlowX},{"xyxbWolfYGlowY",20,&g_xyxbWolfYGlowY},{"xyxbWolfYGlowScale",50,&g_xyxbWolfYGlowScale},
            {"xyxbWheelSquareX",-170,&g_xyxbWheelSquareX},{"xyxbWheelSquareY",-10,&g_xyxbWheelSquareY},{"xyxbWheelSquareScale",87,&g_xyxbWheelSquareScale},
            {"xyxbWheelTriangleX",-100,&g_xyxbWheelTriangleX},{"xyxbWheelTriangleY",-40,&g_xyxbWheelTriangleY},{"xyxbWheelTriangleScale",90,&g_xyxbWheelTriangleScale},
        };
        for (auto& v : swapVars) {
            rr=reg_int(v.n,v.d,*v.h,error);
            if(rr!=MOD_OK) return rr;
        }
        rr=reg_bool("swapItemSquareFlipH",false,g_swapItemSquareFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapItemSquareFlipV",false,g_swapItemSquareFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapItemTriangleFlipH",false,g_swapItemTriangleFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapItemTriangleFlipV",false,g_swapItemTriangleFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapWolfXGlowEnabled",true,g_swapWolfXGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("swapWolfYGlowEnabled",true,g_swapWolfYGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybItemSquareFlipH",false,g_ybItemSquareFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybItemSquareFlipV",false,g_ybItemSquareFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybItemTriangleFlipH",false,g_ybItemTriangleFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybItemTriangleFlipV",false,g_ybItemTriangleFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybSwordFlipH",false,g_ybSwordFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybSwordFlipV",false,g_ybSwordFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybBackGlowEnabled",true,g_ybBackGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybWolfXGlowEnabled",true,g_ybWolfXGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("ybWolfYGlowEnabled",true,g_ybWolfYGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbItemSquareFlipH",false,g_xyxbItemSquareFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbItemSquareFlipV",false,g_xyxbItemSquareFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbItemTriangleFlipH",false,g_xyxbItemTriangleFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbItemTriangleFlipV",false,g_xyxbItemTriangleFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbSwordFlipH",false,g_xyxbSwordFlipH,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbSwordFlipV",false,g_xyxbSwordFlipV,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbBackGlowEnabled",true,g_xyxbBackGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbWolfXGlowEnabled",true,g_xyxbWolfXGlowEnabled,error); if(rr!=MOD_OK) return rr;
        rr=reg_bool("xyxbWolfYGlowEnabled",true,g_xyxbWolfYGlowEnabled,error); if(rr!=MOD_OK) return rr;
    }
    apply_layout_schema_migrations();

    UiModsPanelDesc panel=UI_MODS_PANEL_DESC_INIT;
    panel.build=build_layout_panel;
    if(svc_ui->register_mods_panel(mod_ctx,&panel)!=MOD_OK)
        return mods::set_error(error,MOD_ERROR,"failed to register layout editor panel");
    {
        UiMenuTabDesc menuTab=UI_MENU_TAB_DESC_INIT;
        menuTab.label="CONTROLLER UI";
        menuTab.on_selected=open_public_window;
        if(svc_ui->register_menu_tab(mod_ctx,&menuTab,&g_menuTab)!=MOD_OK)
            return mods::set_error(error,MOD_ERROR,"failed to register Classic Buttons public menu tab");
    }
    if (kDeveloperOptions) {
        UiMenuTabDesc devTab=UI_MENU_TAB_DESC_INIT;
        devTab.label="CONTROLLER DEV";
        devTab.on_selected=open_layout_window;
        if(svc_ui->register_menu_tab(mod_ctx,&devTab,&g_devMenuTab)!=MOD_OK)
            return mods::set_error(error,MOD_ERROR,"failed to register Classic Buttons developer menu tab");
    }

    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "TP Classic Modern Controller UI v1.2.0 starting - by XandasLegend");
        if (kDeveloperOptions)
            svc_log->info(mod_ctx, "Developer HUD calibration options enabled");
    }

    if (svc_hook == nullptr)
        return mods::set_error(error, MOD_ERROR, "HookService unavailable");
    if (svc_resource == nullptr)
        return mods::set_error(error, MOD_ERROR, "ResourceService unavailable");

    if (!load_button_texture("buttons/cross.bti", &s_cross) ||
        !load_button_texture("buttons/circle.bti", &s_circle) ||
        !load_button_texture("buttons/square.bti", &s_square) ||
        !load_button_texture("buttons/triangle.bti", &s_triangle) ||
        !load_button_texture("buttons/r1.bti", &s_r1) ||
        !load_button_texture("buttons/r1_hud.bti", &s_r1_hud) ||
        !load_button_texture("buttons/l3.bti", &s_analog) ||
        !load_button_texture("buttons/animated_analog_base.bti", &s_animated_analog_base) ||
        !load_button_texture("buttons/skill_l3.bti", &s_skill_l3) ||
        !load_button_texture("buttons/shop_l3_right.bti", &s_shop_l3_right) ||
        !load_button_texture("buttons/r3.bti", &s_r3) ||
        !load_button_texture("buttons/l2.bti", &s_l2) ||
        !load_button_texture("buttons/r2.bti", &s_r2) ||
        !load_button_texture("buttons/options.bti", &s_options) ||
        !load_button_texture("buttons/dpad.bti", &s_dpad) ||
        !load_button_texture("ornament/menu_ornament.bti", &s_menu_ornament) ||
        !load_button_texture("ornament/hud_ornament.bti", &s_hud_ornament) ||
        !load_button_texture("layout_guide.bti", &s_guide)) {
        free_resources();
        return mods::set_error(error, MOD_UNAVAILABLE, "failed to load calibration BTI resources");
    }

    for(auto& texture:s_controllerTextures) {
        if(!load_button_texture(texture.xboxPath,&texture.xbox)) {
            free_resources();
            return mods::set_error(error,MOD_UNAVAILABLE,"failed to load Xbox controller texture");
        }
        if(!load_button_texture(texture.switchPath,&texture.switchTexture)) {
            free_resources();
            return mods::set_error(error,MOD_UNAVAILABLE,"failed to load Switch controller texture");
        }
    }

    ModResult pre = mods::hook::add_pre<MeterDrawHook>(svc_hook, before_meter_draw);
    if (pre != MOD_OK) {
        free_resources();
        return mods::set_error(error, pre, "failed to install PRE hook for dMeter2Draw_c::draw");
    }
    s_drawPreInstalled = true;

    ModResult xyPost = mods::hook::add_post<ButtonXYDrawHook>(svc_hook, after_button_xy_draw);
    if (xyPost != MOD_OK) {
        mods::hook::uninstall<MeterDrawHook>();
        s_drawPreInstalled = false;
        free_resources();
        return mods::set_error(error, xyPost, "failed to install POST hook for dMeter2Draw_c::drawButtonXY");
    }
    s_buttonXYHookInstalled = true;

    ModResult crossPre = mods::hook::add_pre<ButtonCrossDrawHook>(svc_hook, before_button_cross_draw);
    if (crossPre != MOD_OK) {
        mods::hook::uninstall<ButtonXYDrawHook>();
        mods::hook::uninstall<MeterDrawHook>();
        s_buttonXYHookInstalled = false;
        s_drawPreInstalled = false;
        free_resources();
        return mods::set_error(error, crossPre, "failed to install PRE hook for dMeter2Draw_c::drawButtonCross");
    }
    s_buttonCrossHookInstalled = true;

    ModResult pt = mods::hook::add_pre<PaneTransHook>(svc_hook, before_pane_trans);
    ModResult psd = mods::hook::add_pre<ScreenDrawHook>(before_screen_draw, nullptr);
    if (psd != MOD_OK) {
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return psd;
    }
    HookOptions ringPreOptions = HOOK_OPTIONS_INIT;
    ringPreOptions.priority = -1000;
    ModResult ringPre = mods::hook::add_pre<RingControllerOverlayHook>(svc_hook, before_ring_controller_overlay, &ringPreOptions);
    if (ringPre != MOD_OK) {
        mods::hook::uninstall<PaneTransHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        free_resources();
        return mods::set_error(error, ringPre, "failed to install PRE hook for dMenu_Ring_c::_draw");
    }
    HookOptions ringPostOptions = HOOK_OPTIONS_INIT;
    ringPostOptions.priority = 1000;
    ModResult ringPost = mods::hook::add_post<RingControllerOverlayHook>(svc_hook, after_ring_controller_overlay, &ringPostOptions);
    if (ringPost != MOD_OK) {
        mods::hook::uninstall<RingControllerOverlayHook>();
        mods::hook::uninstall<PaneTransHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        free_resources();
        return mods::set_error(error, ringPost, "failed to install POST hook for dMenu_Ring_c::_draw");
    }

    ModResult collectPre = mods::hook::add_pre<CollectCompatDrawHook>(svc_hook, before_collect_compat_draw);
    if (collectPre != MOD_OK) {
        mods::hook::uninstall<RingControllerOverlayHook>();
        mods::hook::uninstall<PaneTransHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        free_resources();
        return mods::set_error(error, collectPre, "failed to install PRE hook for dMenu_Collect2D_c::_draw");
    }
    ModResult collectPost = mods::hook::add_post<CollectCompatDrawHook>(svc_hook, after_collect_compat_draw);
    if (collectPost != MOD_OK) {
        mods::hook::uninstall<CollectCompatDrawHook>();
        mods::hook::uninstall<RingControllerOverlayHook>();
        mods::hook::uninstall<PaneTransHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        free_resources();
        return mods::set_error(error, collectPost, "failed to install POST hook for dMenu_Collect2D_c::_draw");
    }

    ModResult psdPost = mods::hook::add_post<ScreenDrawHook>(svc_hook, after_screen_draw);
    if (psdPost != MOD_OK) {
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return psdPost;
    }
    ModResult fsd = mods::hook::add_pre<FileSelDrawHook>(svc_hook, before_file_select_draw);
    if (fsd != MOD_OK) {
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return fsd;
    }
    ModResult mbInitPost = mods::hook::add_post<MeterButtonScreenInitHook>(svc_hook, after_meter_button_screen_init);
    if (mbInitPost != MOD_OK) {
        mods::hook::uninstall<FileSelDrawHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return mods::set_error(error, mbInitPost, "failed to install POST hook for dMeterButton_c::screenInitButton");
    }
    ModResult ofd = mods::hook::add_post<OutFontCreatePaneHook>(svc_hook, after_outfont_create_pane);
    if (ofd != MOD_OK) {
        mods::hook::uninstall<MeterButtonScreenInitHook>();
        mods::hook::uninstall<FileSelDrawHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return ofd;
    }
    ModResult ofDraw = mods::hook::add_pre<OutFontDrawFontHook>(svc_hook, before_outfont_draw_font);
    if (ofDraw != MOD_OK) {
        mods::hook::uninstall<MeterButtonScreenInitHook>();
        mods::hook::uninstall<OutFontDrawFontHook>();
        mods::hook::uninstall<OutFontCreatePaneHook>();
        mods::hook::uninstall<FileSelDrawHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        free_resources();
        return mods::set_error(error, ofDraw, "failed to install PRE hook for COutFont_c::drawFont");
    }
    if (pt != MOD_OK) {
        free_resources();
        return mods::set_error(error, pt, "failed to install PRE hook for CPaneMgr::paneTrans");
    }
    s_paneTransHookInstalled = true;

    ModResult post = mods::hook::add_post<MeterDrawHook>(svc_hook, after_meter_draw);
    if (post != MOD_OK) {
        free_resources();
        return mods::set_error(error, post, "failed to install POST hook for dMeter2Draw_c::draw");
    }
    s_drawHookInstalled = true;

    HookOptions midnaCompatPostOptions = HOOK_OPTIONS_INIT;
    midnaCompatPostOptions.priority = -1000;
    ModResult midnaCompatPost = mods::hook::add_post<MeterButtonExecuteHook>(
        svc_hook, after_meter_button_execute, &midnaCompatPostOptions);
    if (midnaCompatPost != MOD_OK) {
        free_resources();
        return mods::set_error(error, midnaCompatPost,
                               "failed to install POST hook for dMeterButton_c::_execute");
    }

    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) { return MOD_OK; }

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    mods::hook::uninstall<MeterButtonExecuteHook>();
    mods::hook::uninstall<CollectCompatDrawHook>();
    mods::hook::uninstall<RingControllerOverlayHook>();
    mods::hook::uninstall<MeterButtonScreenInitHook>();
    if (s_buttonCrossHookInstalled) {
        mods::hook::uninstall<ButtonCrossDrawHook>();
        s_buttonCrossHookInstalled = false;
    }
    if (s_buttonXYHookInstalled) {
        mods::hook::uninstall<ButtonXYDrawHook>();
        s_buttonXYHookInstalled = false;
    }
    if (s_paneTransHookInstalled) {
        mods::hook::uninstall<OutFontDrawFontHook>();
        mods::hook::uninstall<OutFontCreatePaneHook>();
        mods::hook::uninstall<FileSelDrawHook>();
        mods::hook::uninstall<ScreenDrawHook>();
        mods::hook::uninstall<PaneTransHook>();
        s_paneTransHookInstalled = false;
    }
    if (s_drawHookInstalled || s_drawPreInstalled) {
        mods::hook::uninstall<MeterDrawHook>();
        s_drawHookInstalled = false;
        s_drawPreInstalled = false;
    }
    s_activeMeter = nullptr;
    s_meterInstance = nullptr;
    s_activeMeterButton = nullptr;
    s_midnaPromptScreen = nullptr;
    s_midnaPromptOriginalTexture = nullptr;
    s_externalMidnaPromptOwner = false;
    for (auto& st : s_midnaPictures) st = {};
    s_midnaDiagnostic.clear();
    s_midnaDiagLastEvent=false;
    s_midnaDiagPostEventFrame=-1;
    g_publicWindow = 0;
    g_layoutWindow = 0;
    g_visualHudEditorEnabled = 0;
    g_swapXYButtonLayout = 0;
    g_layoutSchemaVersion = 0;
    free_resources();
    return MOD_OK;
}

}
