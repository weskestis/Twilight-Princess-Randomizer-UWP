cmake_minimum_required(VERSION 3.26)
add_mod(port_a_link_between_twilight
        FEATURES game webgpu
        SOURCES
            src/mod.cpp
            src/albt_shield_api.cpp
            src/albw_common.cpp
            src/albw_save_flags.cpp
            src/devil_trigger.cpp
            src/bokoblin_dt.cpp
            src/poise_suppress.cpp
            src/lantern_fix.cpp
            src/king_bulblin_dt.cpp
            src/guay_dt.cpp
            src/bulblin_dt.cpp
            src/lizalfos_dt.cpp
            src/dynalfos_dt.cpp
            src/kargarok_dt.cpp
            src/stalfos_dt.cpp
            src/albw_profile_hook.cpp
            src/albw_l1_input.cpp
            src/albw_settings_ui.cpp
            src/albw_stage.cpp
            src/config_vars.cpp
            src/meter.cpp
            src/lockout.cpp
            src/enemy_lockout.cpp
            src/enemy_hp_bars.cpp
            src/magic_jar.cpp
            src/rupee_popup.cpp
            src/tear_particles.cpp
            src/tear_actor.cpp
            src/tear_glow.cpp
            src/mq_heart_meter.cpp
            src/shield.cpp
            src/shield_adapt.cpp
            src/shield_game.cpp
            src/shield_hooks.cpp
            src/stick_cycle.cpp
            src/region_port.cpp
            src/soul_of_light.cpp
            src/enemy_rupees.cpp
            src/wolf_combat.cpp
            src/wolf_guard.cpp
            src/wolf_guard_hooks.cpp
            src/e_s1_hooks.cpp
            src/end_game_transform.cpp
            src/hurricane_spin.cpp
            src/deku_leaf.cpp
            src/air_tricks.cpp
            src/ending_blow_aoe.cpp
            src/tutorial_dummy.cpp
            src/wolf_charge_art.cpp
            src/wolf_howl_combat.cpp
            src/midna_arm.cpp
            src/midna_arm_visual.cpp
            src/wolf_charge_hud.cpp
            src/hold_a_crawl.cpp
            src/quick_swap.cpp
            src/quick_equip.cpp
            src/extra_item_slot.cpp
            src/extra_item_slot_hooks.cpp
            src/z_item_hud.cpp
            src/mail.cpp
            src/mail_hooks.cpp
            src/focused_arts.cpp
            src/sim_time_scale.cpp
            src/sim_time_scale_hooks.cpp
            src/flurry_rush.cpp
            src/flurry_hooks.cpp
            src/flurry_proc.cpp
            src/wolf_arts.cpp
            src/parry_master.cpp
            src/parry_hooks.cpp
            src/albw_combat.cpp
            src/btn_parry.cpp
            src/boss_hp_hud.cpp
            src/lop_hud.cpp
            src/lop_item_belt.cpp
            src/boss_refinement.cpp
            src/diababa.cpp
            src/fyrus.cpp
            src/fyrus_golem.cpp
            src/armogohma.cpp
            src/epona_spur_hud.cpp
            src/hidden_skill_charge.cpp
            src/fyrus_kids.cpp
            src/fyrus_phases.cpp
            src/boss_refinement_hooks.cpp
            src/rental_eligibility.cpp
            src/rental_shop.cpp
            src/rental_ui_text.cpp
            src/rental_dialogue.cpp
            src/rental_postman_hooks.cpp
            src/rental_shop_ui.cpp
            src/oocoo.cpp
            src/mq_hearts.cpp
            src/sword_atp.cpp
            src/potion.cpp
            src/potion_grant.cpp
            src/potion_bottle.cpp
            src/potion_realtime.cpp
            src/potion_hooks.cpp
            src/shade_refuge.cpp
            src/outfit_stats.cpp
            src/outfit_swim.cpp
            src/outfit.cpp
            src/outfit_debug.cpp
            src/sumo_test.cpp
            src/wardrobe.cpp
            src/alink_compat.cpp
            src/albw_fork_compat.cpp
            src/albw_gameinfo_compat.cpp
            src/colossal_wallet.cpp
            src/clothes_pipeline.cpp
            src/changelink.cpp
            src/ext_quick_equip.cpp
            src/menu_ext_status.cpp
            src/menu_ext_members.cpp
            src/menu_window_ext.cpp
            src/menu_ring_ext.cpp
        MOD_JSON mod.json
        RES_DIR res
        OVERLAY_DIR overlay
        TEXTURES_DIR textures
        BUNDLE
)
if (MSVC)
    target_link_options(port_a_link_between_twilight PRIVATE "/PDBALTPATH:%_PDB%")
endif ()

target_compile_definitions(port_a_link_between_twilight PRIVATE DUSK_STATIC_PORT_29=1)
target_include_directories(port_a_link_between_twilight PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(port_a_link_between_twilight PRIVATE SDL3::SDL3)
if (MSVC)
    target_compile_options(port_a_link_between_twilight PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/xbox_private_aliases.hpp")
else ()
    target_compile_options(port_a_link_between_twilight PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/src/xbox_private_aliases.hpp")
endif ()
