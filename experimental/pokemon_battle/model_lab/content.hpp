#pragma once
#include "gx_renderer.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace pokemon_battle::content {
bool initialize(std::string& error);
void shutdown();
std::shared_ptr<runtime::GxAsset> get(std::uint16_t species,std::string& error);
std::uint16_t identify(const runtime::Model& model);
bool install(runtime::Model model,const char* location,std::uint16_t& species,std::string& error);
bool restore(std::uint16_t species,std::string& error);
} // namespace pokemon_battle::content
