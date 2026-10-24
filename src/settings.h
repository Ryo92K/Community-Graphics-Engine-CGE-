#pragma once
#include <filesystem>
namespace cge {
struct Settings {
    bool enabled{true};
    float exposure{},contrast{1},saturation{1},sharpen{},gamma{1},temperature{};
    int toggleKey{0x77};
    static bool ValidToggleKey(int key) noexcept {
        return key>=0x70 && key<=0x7B && key!=0x75 && key!=0x76 && key!=0x79;
    }
    static Settings Load(const std::filesystem::path&);
    bool Save(const std::filesystem::path&) const noexcept;
    bool Neutral() const noexcept {return exposure==0 && contrast==1 && saturation==1 && sharpen==0 && gamma==1 && temperature==0;}
};
}
