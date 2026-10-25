#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace cge {
struct AmbientShader {
    std::vector<std::uint8_t> bytecode;
    unsigned ambientRows{};
    std::string status;
};
AmbientShader AddAmbientOcclusion(std::span<const std::uint8_t> source);
}
