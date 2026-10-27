#include "presets.h"
#include <array>
namespace cge::presets {
namespace {
enum class Light {Shared,Interior,Day,Night};
SsaoSettings Occlusion(Light light,float strength,float radius,float power) {
    SsaoSettings result;result.enabled=true;result.strength=strength;result.radius=radius;result.power=power;result.quality=0;
    result.maxDistance=12000;
    if(light==Light::Interior){result.strength*=0.8f;result.radius*=0.8f;result.maxDistance=4500;}
    if(light==Light::Night)result.strength*=0.8f;
    return result;
}
ColorSettings Natural(Light light) {
    ColorSettings p;p.contrast=1.08f;p.saturation=1.06f;p.sharpen=0.1f;
    p.ssao=Occlusion(light,0.4f,45,1.2f);p.bloom={true,0.05f,0.95f,18};
    p.clarity={true,0.25f};p.vibrance={true,0.22f};p.debanding={true,0.35f,4,0.025f};
    if(light==Light::Interior){p.exposure=0.08f;p.contrast=1.06f;p.gamma=1.03f;p.sharpen=0.08f;p.temperature=2;p.clarity.strength=0.2f;p.vibrance.strength=0.18f;}
    if(light==Light::Night){p.exposure=0.12f;p.contrast=1.04f;p.gamma=1.06f;p.saturation=1.02f;p.sharpen=0.06f;p.clarity.strength=0.15f;p.vibrance.strength=0.14f;}
    return p;
}
ColorSettings Nordic(Light light) {
    ColorSettings p;p.contrast=1.19f;p.saturation=0.92f;p.sharpen=0.1f;p.gamma=1.03f;p.temperature=-7;
    p.ssao=Occlusion(light,0.5f,60,1.25f);
    p.bloom={true,0.12f,0.8f,32};p.fog={true,0.16f,4000};p.clarity={true,0.35f};p.vibrance={true,0.18f};
    p.splitToning={true,0.1f,0.48f,0.29f,0.36f,0.43f,0.86f,0.83f,0.77f};p.debanding={true,0.5f,6,0.04f};
    p.liftGammaGain.enabled=true;p.liftGammaGain.strength=0.35f;p.liftGammaGain.gammaBlue=1.01f;p.liftGammaGain.gainRed=0.99f;p.liftGammaGain.gainBlue=1.01f;
    p.lensDirt={true,0.12f,0.35f};
    if(light==Light::Interior){p.exposure=0.12f;p.contrast=1.1f;p.temperature=-3;p.gamma=1.07f;p.fog.enabled=false;p.bloom.intensity=0.08f;p.clarity.strength=0.25f;p.vibrance.strength=0.14f;p.liftGammaGain.strength=0.25f;}
    if(light==Light::Night){p.exposure=0.18f;p.contrast=1.12f;p.saturation=0.94f;p.gamma=1.1f;p.temperature=-10;p.sharpen=0.06f;p.clarity.strength=0.2f;p.vibrance.strength=0.1f;p.bloom.intensity=0.08f;p.fog.density=0.08f;p.fog.startDistance=6000;p.liftGammaGain.strength=0.25f;p.lensDirt.strength=0.08f;}
    return p;
}
ColorSettings WarmFantasy(Light light) {
    ColorSettings p;p.exposure=0.06f;p.contrast=1.1f;p.saturation=1.16f;p.sharpen=0.06f;p.gamma=1.04f;p.temperature=26;
    p.ssao=Occlusion(light,0.42f,55,1.2f);
    p.bloom={true,0.2f,0.65f,40};p.fog={true,0.08f,4500};p.clarity={true,0.18f};p.vibrance={true,0.4f};
    p.splitToning={true,0.28f,0.52f,0.32f,0.22f,0.52f,1,0.74f,0.4f};p.debanding={true,0.5f,6,0.04f};
    p.liftGammaGain.enabled=true;p.liftGammaGain.strength=0.5f;p.liftGammaGain.gammaRed=1.04f;p.liftGammaGain.gainBlue=0.96f;
    p.lut={true,0.4f,"Warm.cube"};p.lensDirt={true,0.2f,0.3f};
    if(light==Light::Interior){p.exposure=0.1f;p.temperature=20;p.bloom.intensity=0.16f;p.fog.enabled=false;p.clarity.strength=0.14f;p.vibrance.strength=0.32f;p.lut.strength=0.3f;}
    if(light==Light::Night){p.exposure=0.16f;p.contrast=1.06f;p.saturation=1.06f;p.gamma=1.1f;p.temperature=12;p.sharpen=0.03f;p.clarity.strength=0.08f;p.vibrance.strength=0.24f;p.bloom.intensity=0.12f;p.fog.density=0.04f;p.fog.startDistance=6500;p.lut.strength=0.22f;p.lensDirt.strength=0.12f;}
    return p;
}
ColorSettings Cinematic(Light light) {
    ColorSettings p;p.exposure=-0.02f;p.contrast=1.2f;p.saturation=1.04f;p.sharpen=0.08f;p.gamma=1.04f;p.temperature=-3;
    p.ssao=Occlusion(light,0.55f,65,1.35f);
    p.bloom={true,0.16f,0.8f,36};p.fog={true,0.2f,3500};p.clarity={true,0.35f};p.vibrance={true,0.2f};
    p.vignette={true,0.26f,0.35f,0.6f};p.filmGrain={true,0.06f,1.2f};p.chromaticAberration={true,0.2f,0.45f};
    p.splitToning={true,0.2f,0.5f,0.12f,0.36f,0.6f,1,0.7f,0.38f};p.debanding={true,0.65f,8,0.045f};
    p.liftGammaGain.enabled=true;p.liftGammaGain.strength=0.65f;p.liftGammaGain.liftBlue=0.012f;p.liftGammaGain.gainRed=1.03f;p.liftGammaGain.gainBlue=0.96f;
    p.lensDirt={true,0.25f,0.35f};p.lensDistortion={true,0.08f};
    if(light==Light::Interior){p.exposure=0.12f;p.contrast=1.18f;p.gamma=1.08f;p.fog.enabled=false;p.bloom.intensity=0.12f;p.clarity.strength=0.25f;p.vibrance.strength=0.16f;p.vignette.strength=0.22f;}
    if(light==Light::Night){p.exposure=0.2f;p.contrast=1.1f;p.saturation=1.03f;p.gamma=1.12f;p.sharpen=0.04f;p.clarity.strength=0.18f;p.vibrance.strength=0.12f;p.bloom.intensity=0.1f;p.fog.density=0.09f;p.fog.startDistance=5500;p.vignette.strength=0.18f;p.filmGrain.strength=0.045f;p.chromaticAberration.strength=0.14f;p.lensDirt.strength=0.15f;}
    return p;
}
ColorSettings SoftFocus(Light light) {
    ColorSettings p;p.exposure=0.08f;p.contrast=1.08f;p.saturation=1.08f;p.sharpen=0.01f;p.gamma=1.08f;p.temperature=10;
    p.ssao=Occlusion(light,0.28f,40,1.1f);
    p.bloom={true,0.24f,0.65f,44};p.dof={true,600,0.8f};p.clarity.enabled=false;p.vibrance={true,0.2f};
    p.vignette={true,0.18f,0.4f,0.6f};p.filmGrain={true,0.05f,1.3f};p.chromaticAberration={true,0.12f,0.55f};
    p.splitToning={true,0.2f,0.5f,0.3f,0.35f,0.58f,1,0.78f,0.5f};p.debanding={true,0.5f,6,0.04f};
    p.liftGammaGain.enabled=true;p.liftGammaGain.strength=0.5f;p.liftGammaGain.liftRed=0.012f;p.liftGammaGain.liftBlue=0.018f;
    p.lut={true,0.15f,"Warm.cube"};p.lensDirt={true,0.18f,0.3f};p.lensDistortion={true,0.04f};
    if(light==Light::Interior){p.exposure=0.12f;p.dof.focusDistance=400;p.bloom.intensity=0.18f;p.vignette.strength=0.15f;p.vibrance.strength=0.18f;}
    if(light==Light::Day)p.dof.focusDistance=800;
    if(light==Light::Night){p.exposure=0.2f;p.gamma=1.12f;p.temperature=4;p.saturation=1.02f;p.dof.focusDistance=600;p.bloom.intensity=0.16f;p.vignette.strength=0.14f;p.vibrance.strength=0.14f;p.filmGrain.strength=0.035f;p.chromaticAberration.strength=0.08f;p.lensDirt.strength=0.1f;p.lut.strength=0.1f;}
    return p;
}
Preset Build(const char* name,const char* description,ColorSettings(*profile)(Light)) {
    return {name,description,profile(Light::Shared),profile(Light::Interior),profile(Light::Day),profile(Light::Night)};
}
const std::array<Preset,5> definitions={
    Build("Natural","Clean color, subtle bloom and gentle ambient contact shadows.",Natural),
    Build("Nordic","Restrained cool grading, balanced colors and atmospheric outdoor haze.",Nordic),
    Build("Warm Fantasy","Golden grading, richer colors and soft luminous highlights.",WarmFantasy),
    Build("Cinematic","Rich cinematic color, controlled contrast and a subtle lens treatment.",Cinematic),
    Build("Soft Focus","Warm pastel grading and manual background depth of field for screenshots.",SoftFocus)};
}
std::span<const Preset> List() noexcept {return definitions;}
bool Apply(std::size_t index,Settings& settings) noexcept {
    if(index>=definitions.size())return false;
    const auto& preset=definitions[index];Settings next;next.weather=settings.weather;next.toggleScanCode=settings.toggleScanCode;next.motion=settings.motion;next.dofAllowed=settings.dofAllowed;next.showFps=settings.showFps;
    static_cast<ColorSettings&>(next)=preset.shared;
    next.interior=preset.interior;next.exterior=preset.day;next.exteriorNight=preset.night;
    next.separateProfiles=true;next.profilesConfigured=true;next.transitionSeconds=2;
    settings=next;return true;
}
}
