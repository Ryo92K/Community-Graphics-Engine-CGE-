#pragma once
#include <filesystem>
namespace cge {
enum class Environment { Unknown, Exterior, Interior };
struct ColorSettings {
    float exposure{},contrast{1},saturation{1},sharpen{},gamma{1},temperature{};
    bool Neutral() const noexcept {return exposure==0 && contrast==1 && saturation==1 && sharpen==0 && gamma==1 && temperature==0;}
    bool operator==(const ColorSettings&) const =default;
};
struct Settings:ColorSettings {
    bool enabled{true};
    int toggleKey{0x77};
    bool separateProfiles{false};
    float transitionSeconds{1.5f};
    ColorSettings interior,exterior;
    bool profilesConfigured{};
    void SetSeparateProfiles(bool value) noexcept {
        if(value && !profilesConfigured){interior=exterior=static_cast<const ColorSettings&>(*this);profilesConfigured=true;}
        separateProfiles=value;
    }
    const ColorSettings& For(Environment environment) const noexcept {
        if(separateProfiles && environment==Environment::Interior)return interior;
        if(separateProfiles && environment==Environment::Exterior)return exterior;
        return *this;
    }
    static bool ValidToggleKey(int key) noexcept {
        return key>=0x70 && key<=0x7B && key!=0x75 && key!=0x76 && key!=0x79;
    }
    static Settings Load(const std::filesystem::path&);
    bool Save(const std::filesystem::path&) const noexcept;
};
}
