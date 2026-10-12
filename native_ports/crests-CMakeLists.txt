cmake_minimum_required(VERSION 3.25)
add_mod(port_crests_of_courage
    FEATURES game fmt
    SOURCES src/mod.cpp src/puppet.cpp src/util.cpp src/models.cpp src/features.cpp src/menu.cpp src/colors.cpp src/upnp.cpp src/online.cpp src/skipvote.cpp src/drops.cpp src/softlocks.cpp src/storytest.cpp src/twilight.cpp src/skills.cpp src/rando.cpp src/checks.cpp src/check_names.cpp src/ganon.cpp src/beast.cpp src/sumo.cpp src/map_markers.cpp src/notify.cpp src/selftest.cpp src/private_arc.cpp src/projectiles.cpp src/effects.cpp src/pvp.cpp src/spinner.cpp src/rival.cpp src/world.cpp src/enemies.cpp src/enemy_layout_wb.cpp src/enemy_layout_yk.cpp src/enemy_layout_po.cpp src/spawns.cpp src/boss.cpp src/join_sync.cpp src/epona.cpp src/grass.cpp src/party_hud.cpp src/skins.cpp src/icons.cpp src/voices.cpp src/game_mode.cpp src/horses.cpp src/report.cpp src/lan.cpp src/global.cpp src/chat.cpp src/crash_guard.cpp src/global_hud.cpp src/chat_keys.cpp src/voice.cpp src/voice_io.cpp
    MOD_JSON mod.json
    RES_DIR res
    OVERLAY_DIR overlay
    TEXTURES_DIR textures
    BUNDLE
)
target_compile_definitions(port_crests_of_courage PRIVATE DUSK_STATIC_PORT_28=1 COOP_VOICE=1 COOP_MOD_VERSION="1.10.0")
target_include_directories(port_crests_of_courage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(port_crests_of_courage PRIVATE opus SDL3::SDL3)
if (DUSK_UWP)
    target_link_libraries(port_crests_of_courage PRIVATE xaudio2.lib ws2_32.lib)
else ()
    target_compile_definitions(port_crests_of_courage PRIVATE COOP_VOICE_SDL=1)
endif ()
if (MSVC)
    target_compile_options(port_crests_of_courage PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/xbox_private_aliases.hpp" /Zc:__cplusplus)
else ()
    target_compile_options(port_crests_of_courage PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/src/xbox_private_aliases.hpp")
endif ()
