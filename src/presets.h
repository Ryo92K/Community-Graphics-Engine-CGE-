#pragma once
#include "settings.h"
#include <cstddef>
#include <span>
namespace cge::presets {
struct Preset {
    const char* name;
    const char* description;
    ColorSettings shared,interior,day,night;
};
std::span<const Preset> List() noexcept;
bool Apply(std::size_t index,Settings& settings) noexcept;
}
