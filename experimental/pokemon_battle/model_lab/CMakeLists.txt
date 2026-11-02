cmake_minimum_required(VERSION 3.25)
project(pokemon_model_lab CXX)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DUSK_DIR "" CACHE PATH "Dusklight source root pinned by Xbox .749")
    if(NOT EXISTS "${DUSK_DIR}/sdk/CMakeLists.txt")
        message(FATAL_ERROR "Set DUSK_DIR to .749's Dusklight checkout")
    endif()
    add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
endif()
add_mod(pokemon_model_lab FEATURES game
    SOURCES mod.cpp gx_renderer.cpp field.cpp field_effects.cpp content.cpp ../field/party.cpp ../field/combat.cpp ../field/roster.cpp ../field/animation.cpp ../runtime/model.cpp ../runtime/effects.cpp ../field/ball.cpp
    MOD_JSON mod.json)
target_compile_features(pokemon_model_lab PRIVATE cxx_std_20)
