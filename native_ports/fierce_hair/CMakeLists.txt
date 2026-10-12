cmake_minimum_required(VERSION 3.25)
add_mod(port_fierce_hair
    FEATURES game
    SOURCES mod.cpp
    MOD_JSON mod.json
    RES_DIR res
    BUNDLE
)
target_compile_definitions(port_fierce_hair PRIVATE DUSK_STATIC_PORT_31=1)
