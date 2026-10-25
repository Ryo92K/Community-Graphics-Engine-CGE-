#include "presets.h"
#include <array>
namespace cge::presets {
namespace {
constexpr std::array<Preset,5> definitions={{
    {"Natural","Subtle contrast and detail, with no bloom, fog or blur.",
        {0.0f,1.03f,1.02f,0.06f,1.0f,0.0f,{false,0.08f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.15f},{true,0.12f}},
        {0.06f,1.02f,1.02f,0.05f,1.03f,2.0f,{false,0.08f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.12f},{true,0.10f}},
        {0.0f,1.03f,1.02f,0.06f,1.0f,0.0f,{false,0.08f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.15f},{true,0.12f}},
        {0.06f,1.02f,0.98f,0.03f,1.03f,-2.0f,{false,0.08f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.08f},{true,0.08f}}},
    {"Nordic","Cooler, quieter colors with a light outdoor haze and restrained bloom.",
        {-0.02f,1.07f,0.91f,0.06f,1.02f,-12.0f,{true,0.07f,0.8f,24.0f},{true,0.09f,4500.0f},{false,2000.0f,0.5f},{true,0.22f},{true,0.14f}},
        {0.06f,1.05f,0.95f,0.04f,1.04f,-4.0f,{true,0.06f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.16f},{true,0.10f}},
        {-0.02f,1.07f,0.91f,0.06f,1.02f,-12.0f,{true,0.07f,0.8f,24.0f},{true,0.09f,4500.0f},{false,2000.0f,0.5f},{true,0.22f},{true,0.14f}},
        {0.06f,1.05f,0.87f,0.03f,1.04f,-16.0f,{true,0.06f,0.85f,24.0f},{true,0.05f,6000.0f},{false,2000.0f,0.5f},{true,0.12f},{true,0.08f}}},
    {"Warm Fantasy","Warmer colors and softer highlights, with a gentle outdoor haze.",
        {0.03f,1.04f,1.1f,0.04f,1.02f,12.0f,{true,0.14f,0.65f,36.0f},{true,0.05f,5000.0f},{false,2000.0f,0.5f},{true,0.10f},{true,0.30f}},
        {0.05f,1.03f,1.07f,0.03f,1.03f,10.0f,{true,0.12f,0.7f,32.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.08f},{true,0.24f}},
        {0.03f,1.04f,1.1f,0.04f,1.02f,12.0f,{true,0.14f,0.65f,36.0f},{true,0.05f,5000.0f},{false,2000.0f,0.5f},{true,0.10f},{true,0.30f}},
        {0.07f,1.03f,1.03f,0.02f,1.05f,3.0f,{true,0.1f,0.75f,32.0f},{true,0.03f,6500.0f},{false,2000.0f,0.5f},{true,0.05f},{true,0.16f}}},
    {"Cinematic","Stronger contrast and restrained colors, without forced gameplay blur.",
        {-0.03f,1.13f,0.88f,0.05f,1.03f,-5.0f,{true,0.1f,0.8f,28.0f},{true,0.13f,3500.0f},{false,2000.0f,0.5f},{true,0.18f},{true,0.08f}},
        {0.06f,1.09f,0.92f,0.04f,1.05f,3.0f,{true,0.08f,0.8f,24.0f},{false,0.08f,4000.0f},{false,2000.0f,0.5f},{true,0.14f},{true,0.06f}},
        {-0.03f,1.13f,0.88f,0.05f,1.03f,-5.0f,{true,0.1f,0.8f,28.0f},{true,0.13f,3500.0f},{false,2000.0f,0.5f},{true,0.18f},{true,0.08f}},
        {0.07f,1.08f,0.85f,0.02f,1.06f,-9.0f,{true,0.07f,0.85f,28.0f},{true,0.06f,5500.0f},{false,2000.0f,0.5f},{true,0.10f},{true,0.04f}}},
    {"Soft Focus","A screenshot starting point with manual background depth of field.",
        {0.0f,1.06f,0.97f,0.02f,1.02f,3.0f,{true,0.12f,0.7f,32.0f},{false,0.08f,4000.0f},{true,600.0f,0.75f},{false,0.25f},{true,0.14f}},
        {0.06f,1.04f,0.98f,0.01f,1.04f,5.0f,{true,0.1f,0.75f,28.0f},{false,0.08f,4000.0f},{true,400.0f,0.6f},{false,0.25f},{true,0.12f}},
        {0.0f,1.06f,0.97f,0.02f,1.02f,3.0f,{true,0.12f,0.7f,32.0f},{false,0.08f,4000.0f},{true,800.0f,0.7f},{false,0.25f},{true,0.14f}},
        {0.07f,1.04f,0.94f,0.01f,1.05f,-2.0f,{true,0.1f,0.8f,32.0f},{false,0.08f,4000.0f},{true,600.0f,0.65f},{false,0.25f},{true,0.10f}}},
}};
}
std::span<const Preset> List() noexcept {return definitions;}
bool Apply(std::size_t index,Settings& settings) noexcept {
    if(index>=definitions.size())return false;
    const auto& preset=definitions[index];
    Settings next;
    static_cast<ColorSettings&>(next)=preset.shared;
    next.interior=preset.interior;next.exterior=preset.day;next.exteriorNight=preset.night;
    next.separateProfiles=true;next.profilesConfigured=true;next.transitionSeconds=2;
    settings=next;return true;
}
}
