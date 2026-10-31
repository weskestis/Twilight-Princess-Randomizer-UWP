cmake_minimum_required(VERSION 3.25)
project(pokemon_battle_lab CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DUSK_DIR "" CACHE PATH "Dusklight source root used by Xbox .749")
    if (NOT EXISTS "${DUSK_DIR}/sdk/CMakeLists.txt")
        message(FATAL_ERROR "Set DUSK_DIR to the pinned Dusklight source root")
    endif ()
    add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
endif ()

# Desktop SDK module for development. Xbox .749 requires a signed, static port
# registration; copying this desktop module into an Xbox mods folder is not enough.
add_mod(pokemon_battle_lab
    SOURCES mod.cpp ../src/battle.cpp
    MOD_JSON mod.json
)
target_include_directories(pokemon_battle_lab PRIVATE ../include)
target_compile_features(pokemon_battle_lab PRIVATE cxx_std_20)
