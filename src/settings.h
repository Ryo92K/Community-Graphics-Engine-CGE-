#pragma once
#include <filesystem>
namespace cge {
enum class Environment { Unknown, Exterior, Interior };
struct BloomSettings {
    bool enabled{false};
    float intensity{0.15f},threshold{1.0f},radius{32};
    bool operator==(const BloomSettings&) const =default;
};
struct FogSettings {
    bool enabled{false};
    float density{0.35f},startDistance{2000};
    bool operator==(const FogSettings&) const =default;
};
struct ColorSettings {
    float exposure{},contrast{1},saturation{1},sharpen{},gamma{1},temperature{};
    BloomSettings bloom;
    FogSettings fog;
    bool Neutral() const noexcept {return exposure==0 && contrast==1 && saturation==1 && sharpen==0 && gamma==1 && temperature==0;}
    bool operator==(const ColorSettings&) const =default;
};
struct MotionSettings {
    bool enabled{false};
    float strength{0.5f},radius{24};
    int samples{12};
    bool operator==(const MotionSettings&) const =default;
    static bool ValidSamples(int value) noexcept {return value==8 || value==12 || value==16;}
};
struct Settings:ColorSettings {
    bool enabled{true};
    bool separateProfiles{false};
    float transitionSeconds{1.5f};
    ColorSettings interior,exterior,exteriorNight;
    float dayHour{6},nightHour{20};
    bool profilesConfigured{};
    MotionSettings motion;
    void SetSeparateProfiles(bool value) noexcept {
        if(value && !profilesConfigured){interior=exterior=exteriorNight=static_cast<const ColorSettings&>(*this);profilesConfigured=true;}
        separateProfiles=value;
    }
    const ColorSettings& For(Environment environment) const noexcept {
        if(separateProfiles && environment==Environment::Interior)return interior;
        if(separateProfiles && environment==Environment::Exterior)return exterior;
        return *this;
    }
    static Settings Load(const std::filesystem::path&);
    bool Save(const std::filesystem::path&) const noexcept;
};
}
