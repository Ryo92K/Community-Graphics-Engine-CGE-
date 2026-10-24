#pragma once
#include <filesystem>
namespace cge {
struct Settings {
    bool enabled{true};
    float exposure{},contrast{1},saturation{1},sharpen{};
    static Settings Load(const std::filesystem::path&);
    bool Save(const std::filesystem::path&) const noexcept;
    bool Neutral() const noexcept {return exposure==0 && contrast==1 && saturation==1 && sharpen==0;}
};
}
