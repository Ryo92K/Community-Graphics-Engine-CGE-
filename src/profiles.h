#pragma once
#include "settings.h"
#include <algorithm>
#include <cmath>
namespace cge {
inline bool ValidGameHour(float hour) noexcept {return std::isfinite(hour) && hour>=0 && hour<=24;}
inline float Smooth(float value) noexcept {value=std::clamp(value,0.0f,1.0f);return value*value*(3-2*value);}
inline float NightWeight(float hour,float dayHour=6,float nightHour=20) noexcept {
    if(!ValidGameHour(hour))return 0;
    dayHour=std::isfinite(dayHour)?std::clamp(dayHour,0.0f,11.0f):6;
    nightHour=std::isfinite(nightHour)?std::clamp(nightHour,13.0f,23.0f):20;
    const float phase=std::fmod(hour-dayHour+24.0f,24.0f);
    const float daylight=nightHour-dayHour;
                                                                            
    if(phase>=23.5f)return 1-Smooth(phase-23.5f);
    if(phase<0.5f)return 1-Smooth(phase+0.5f);
    return Smooth(phase-daylight+0.5f);
}

                                                                             
class ProfileTransition {
    ColorSettings current_,origin_;
    Environment environment_{Environment::Unknown};
    bool initialized_{},separate_{};
    float elapsed_{},duration_{},lastHour_{-1};
    static ColorSettings Mix(const ColorSettings& a,const ColorSettings& b,float t) noexcept {
        auto mix=[t](float x,float y){return x+(y-x)*t;};
        return {mix(a.exposure,b.exposure),mix(a.contrast,b.contrast),
            mix(a.saturation,b.saturation),mix(a.sharpnessEnabled?a.sharpen:0,b.sharpnessEnabled?b.sharpen:0),
            mix(a.gamma,b.gamma),mix(a.temperature,b.temperature),
            {a.bloom.enabled || b.bloom.enabled,
                mix(a.bloom.enabled?a.bloom.intensity:0,b.bloom.enabled?b.bloom.intensity:0),
                mix(a.bloom.threshold,b.bloom.threshold),mix(a.bloom.radius,b.bloom.radius)},
            {a.fog.enabled || b.fog.enabled,
                mix(a.fog.enabled?a.fog.density:0,b.fog.enabled?b.fog.density:0),
                mix(a.fog.startDistance,b.fog.startDistance)},
            {a.dof.enabled || b.dof.enabled,mix(a.dof.focusDistance,b.dof.focusDistance),
                mix(a.dof.enabled?a.dof.strength:0,b.dof.enabled?b.dof.strength:0),
                t<0.5f?a.dof.autofocus:b.dof.autofocus,mix(a.dof.focusSpeed,b.dof.focusSpeed),mix(a.dof.focusRadius,b.dof.focusRadius)},
            {a.clarity.enabled || b.clarity.enabled,
                mix(a.clarity.enabled?a.clarity.strength:0,b.clarity.enabled?b.clarity.strength:0)},
            {a.vibrance.enabled || b.vibrance.enabled,
                mix(a.vibrance.enabled?a.vibrance.strength:0,b.vibrance.enabled?b.vibrance.strength:0)},
            {a.vignette.enabled || b.vignette.enabled,mix(a.vignette.enabled?a.vignette.strength:0,b.vignette.enabled?b.vignette.strength:0),
                mix(a.vignette.radius,b.vignette.radius),mix(a.vignette.softness,b.vignette.softness)},
            {a.filmGrain.enabled || b.filmGrain.enabled,mix(a.filmGrain.enabled?a.filmGrain.strength:0,b.filmGrain.enabled?b.filmGrain.strength:0),
                mix(a.filmGrain.size,b.filmGrain.size)},
            {a.splitToning.enabled || b.splitToning.enabled,mix(a.splitToning.enabled?a.splitToning.strength:0,b.splitToning.enabled?b.splitToning.strength:0),
                mix(a.splitToning.balance,b.splitToning.balance),
                mix(a.splitToning.shadowRed,b.splitToning.shadowRed),mix(a.splitToning.shadowGreen,b.splitToning.shadowGreen),mix(a.splitToning.shadowBlue,b.splitToning.shadowBlue),
                mix(a.splitToning.highlightRed,b.splitToning.highlightRed),mix(a.splitToning.highlightGreen,b.splitToning.highlightGreen),mix(a.splitToning.highlightBlue,b.splitToning.highlightBlue)},
            {a.chromaticAberration.enabled || b.chromaticAberration.enabled,
                mix(a.chromaticAberration.enabled?a.chromaticAberration.strength:0,b.chromaticAberration.enabled?b.chromaticAberration.strength:0),
                mix(a.chromaticAberration.radius,b.chromaticAberration.radius)},
            {a.debanding.enabled || b.debanding.enabled,
                mix(a.debanding.enabled?a.debanding.strength:0,b.debanding.enabled?b.debanding.strength:0),
                mix(a.debanding.radius,b.debanding.radius),mix(a.debanding.threshold,b.debanding.threshold)},
            {a.liftGammaGain.enabled || b.liftGammaGain.enabled,
                mix(a.liftGammaGain.enabled?a.liftGammaGain.strength:0,b.liftGammaGain.enabled?b.liftGammaGain.strength:0),
                mix(a.liftGammaGain.liftRed,b.liftGammaGain.liftRed),
                mix(a.liftGammaGain.liftGreen,b.liftGammaGain.liftGreen),
                mix(a.liftGammaGain.liftBlue,b.liftGammaGain.liftBlue),
                mix(a.liftGammaGain.gammaRed,b.liftGammaGain.gammaRed),
                mix(a.liftGammaGain.gammaGreen,b.liftGammaGain.gammaGreen),
                mix(a.liftGammaGain.gammaBlue,b.liftGammaGain.gammaBlue),
                mix(a.liftGammaGain.gainRed,b.liftGammaGain.gainRed),
                mix(a.liftGammaGain.gainGreen,b.liftGammaGain.gainGreen),
                mix(a.liftGammaGain.gainBlue,b.liftGammaGain.gainBlue)},
            {a.lut.enabled || b.lut.enabled,
                a.lut.file==b.lut.file?mix(a.lut.enabled?a.lut.strength:0,b.lut.enabled?b.lut.strength:0):
                    t<0.5f?(a.lut.enabled?a.lut.strength:0)*(1-2*t):(b.lut.enabled?b.lut.strength:0)*(2*t-1),
                t<0.5f?a.lut.file:b.lut.file},
            {a.lensDirt.enabled || b.lensDirt.enabled,
                mix(a.lensDirt.enabled?a.lensDirt.strength:0,b.lensDirt.enabled?b.lensDirt.strength:0),
                mix(a.lensDirt.threshold,b.lensDirt.threshold)},
            {a.lensDistortion.enabled || b.lensDistortion.enabled,
                mix(a.lensDistortion.enabled?a.lensDistortion.amount:0,b.lensDistortion.enabled?b.lensDistortion.amount:0)},
            a.sharpnessEnabled || b.sharpnessEnabled,
            {a.ssao.enabled || b.ssao.enabled,
                mix(a.ssao.enabled?a.ssao.strength:0,b.ssao.enabled?b.ssao.strength:0),
                mix(a.ssao.radius,b.ssao.radius),mix(a.ssao.bias,b.ssao.bias),mix(a.ssao.power,b.ssao.power),
                mix(a.ssao.maxDistance,b.ssao.maxDistance),t<0.5f?a.ssao.quality:b.ssao.quality,mix(a.ssao.lightProtection,b.ssao.lightProtection)}};
    }
public:
    void Reset() noexcept {*this=ProfileTransition{};}
    ColorSettings Update(const Settings& settings,Environment environment,float seconds,float hour=-1) noexcept {
                                                                                
        if(environment==Environment::Unknown && initialized_)return current_;
        const bool validHour=ValidGameHour(hour),hadHour=ValidGameHour(lastHour_);
        float clockDifference=0;
        if(validHour && hadHour) {
            clockDifference=std::abs(hour-lastHour_);
            clockDifference=std::min(clockDifference,24-clockDifference);
        }
        const bool timeJump=validHour && (!hadHour || clockDifference>0.25f);
        if(validHour)lastHour_=hour==24?0:hour;
        auto target=settings.For(environment);
        if(settings.separateProfiles && environment==Environment::Exterior && ValidGameHour(lastHour_))
            target=Mix(settings.exterior,settings.exteriorNight,NightWeight(lastHour_,settings.dayHour,settings.nightHour));
        if(!initialized_) {
            current_=origin_=target;environment_=environment;separate_=settings.separateProfiles;
            initialized_=environment!=Environment::Unknown;return current_;
        }
        if(environment!=environment_ || separate_!=settings.separateProfiles ||
            (settings.separateProfiles && environment==Environment::Exterior && timeJump)) {
            origin_=current_;elapsed_=0;
            duration_=std::isfinite(settings.transitionSeconds)?std::clamp(settings.transitionSeconds,0.0f,5.0f):1.5f;
            environment_=environment;separate_=settings.separateProfiles;
        }
        if(duration_<=0 || elapsed_>=duration_)return current_=target;
        if(std::isfinite(seconds))elapsed_+=std::max(seconds,0.0f);
        const float t=std::clamp(elapsed_/duration_,0.0f,1.0f);
        const float smooth=t*t*(3-2*t);
        return current_=Mix(origin_,target,smooth);
    }
};
}
