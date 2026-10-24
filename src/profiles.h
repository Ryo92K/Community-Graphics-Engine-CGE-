#pragma once
#include "settings.h"
#include <algorithm>
#include <cmath>
namespace cge {
// Retarget from the displayed value so a second doorway never causes a jump.
class ProfileTransition {
    ColorSettings current_,origin_;
    Environment environment_{Environment::Unknown};
    bool initialized_{},separate_{};
    float elapsed_{},duration_{};
    static ColorSettings Mix(const ColorSettings& a,const ColorSettings& b,float t) noexcept {
        auto mix=[t](float x,float y){return x+(y-x)*t;};
        return {mix(a.exposure,b.exposure),mix(a.contrast,b.contrast),
            mix(a.saturation,b.saturation),mix(a.sharpen,b.sharpen),
            mix(a.gamma,b.gamma),mix(a.temperature,b.temperature)};
    }
public:
    void Reset() noexcept {*this=ProfileTransition{};}
    ColorSettings Update(const Settings& settings,Environment environment,float seconds) noexcept {
        // Keep the last valid location through temporary missing cells/loading.
        if(environment==Environment::Unknown && initialized_)return current_;
        const auto& target=settings.For(environment);
        if(!initialized_) {
            current_=origin_=target;environment_=environment;separate_=settings.separateProfiles;
            initialized_=environment!=Environment::Unknown;return current_;
        }
        if(environment!=environment_ || separate_!=settings.separateProfiles) {
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
