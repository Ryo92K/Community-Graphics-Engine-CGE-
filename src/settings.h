#pragma once
#include <filesystem>
#include <string>
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
struct DofSettings {
    bool enabled{false};
    float focusDistance{2000},strength{0.5f};
    bool autofocus{false};
    float focusSpeed{5},focusRadius{0.03f};
    bool operator==(const DofSettings&) const =default;
};
struct ClaritySettings {
    bool enabled{false};
    float strength{0.25f};
    bool operator==(const ClaritySettings&) const =default;
};
struct VibranceSettings {
    bool enabled{false};
    float strength{0.25f};
    bool operator==(const VibranceSettings&) const =default;
};
struct VignetteSettings {
    bool enabled{false};
    float strength{0.25f},radius{0.35f},softness{0.5f};
    bool operator==(const VignetteSettings&) const =default;
};
struct FilmGrainSettings {
    bool enabled{false};
    float strength{0.1f},size{1};
    bool operator==(const FilmGrainSettings&) const =default;
};
struct SplitToningSettings {
    bool enabled{false};
    float strength{0.25f},balance{0.5f};
    float shadowRed{0.25f},shadowGreen{0.4f},shadowBlue{0.65f};
    float highlightRed{1},highlightGreen{0.72f},highlightBlue{0.45f};
    bool operator==(const SplitToningSettings&) const =default;
};
struct ChromaticAberrationSettings {
    bool enabled{false};
    float strength{0.25f},radius{0.5f};
    bool operator==(const ChromaticAberrationSettings&) const =default;
};
struct DebandingSettings {
    bool enabled{false};
    float strength{0.5f},radius{4},threshold{0.025f};
    bool operator==(const DebandingSettings&) const =default;
};
struct LiftGammaGainSettings {
    bool enabled{false};
    float strength{1};
    float liftRed{},liftGreen{},liftBlue{};
    float gammaRed{1},gammaGreen{1},gammaBlue{1};
    float gainRed{1},gainGreen{1},gainBlue{1};
    bool Neutral() const noexcept {
        return !enabled || strength<=0 || (liftRed==0 && liftGreen==0 && liftBlue==0 &&
            gammaRed==1 && gammaGreen==1 && gammaBlue==1 && gainRed==1 && gainGreen==1 && gainBlue==1);
    }
    bool operator==(const LiftGammaGainSettings&) const =default;
};
struct LensDistortionSettings {
    bool enabled{false};
    float amount{0.2f};
    bool operator==(const LensDistortionSettings&) const =default;
};
struct LensDirtSettings {
    bool enabled{false};
    float strength{0.35f},threshold{0.25f};
    bool operator==(const LensDirtSettings&) const =default;
};
struct LutSettings {
    bool enabled{false};
    float strength{1};
    std::string file;
    bool operator==(const LutSettings&) const =default;
};
struct SsaoSettings {
    bool enabled{false};
    float strength{0.65f},radius{60},bias{0.05f},power{1.5f},maxDistance{8000};
    int quality{1};
    float lightProtection{1};
    bool operator==(const SsaoSettings&) const =default;
};
struct ColorSettings {
    float exposure{},contrast{1},saturation{1},sharpen{},gamma{1},temperature{};
    BloomSettings bloom;
    FogSettings fog;
    DofSettings dof;
    ClaritySettings clarity;
    VibranceSettings vibrance;
    VignetteSettings vignette;
    FilmGrainSettings filmGrain;
    SplitToningSettings splitToning;
    ChromaticAberrationSettings chromaticAberration;
    DebandingSettings debanding;
    LiftGammaGainSettings liftGammaGain;
    LutSettings lut;
    LensDirtSettings lensDirt;
    LensDistortionSettings lensDistortion;
    bool sharpnessEnabled{true};
    SsaoSettings ssao;
    bool Neutral() const noexcept {return exposure==0 && contrast==1 && saturation==1 && (!sharpnessEnabled || sharpen==0) && gamma==1 && temperature==0 && (!clarity.enabled || clarity.strength<=0) && (!vibrance.enabled || vibrance.strength<=0) &&
        (!vignette.enabled || vignette.strength<=0 || vignette.radius>=1) &&
        (!filmGrain.enabled || filmGrain.strength<=0) &&
        (!splitToning.enabled || splitToning.strength<=0) &&
        (!chromaticAberration.enabled || chromaticAberration.strength<=0 || chromaticAberration.radius>=1) &&
        (!debanding.enabled || debanding.strength<=0) && liftGammaGain.Neutral() && (!lut.enabled || lut.strength<=0 || lut.file.empty()) && (!lensDistortion.enabled || lensDistortion.amount==0);}
    bool operator==(const ColorSettings&) const =default;
};
struct MotionSettings {
    bool enabled{false};
    float strength{1.0f},radius{24};
    int samples{12};
    bool operator==(const MotionSettings&) const =default;
    static bool ValidSamples(int value) noexcept {return value==8 || value==12 || value==16;}
};
struct Settings:ColorSettings {
    bool enabled{true};
    bool dofAllowed{true};
    bool showFps{false};
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
