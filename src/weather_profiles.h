#pragma once
#include "settings.h"
#include <algorithm>
#include <cmath>
namespace cge {
inline constexpr const char* WeatherNames[]={"Clear","Cloudy","Rain","Snow"};
struct WeatherObservation {
    std::array<float,4> weights{};
    bool valid{};
};
inline int WeatherClass(unsigned flags) noexcept {
    return flags&8?3:flags&4?2:flags&2?1:flags&1?0:-1;
}
inline WeatherObservation ObserveWeather(unsigned current,unsigned previous,float progress,bool hasPrevious) noexcept {
    WeatherObservation result;
    const int next=WeatherClass(current),last=WeatherClass(previous);
    if(next<0 || !std::isfinite(progress) || progress<0 || progress>1 || (hasPrevious && progress<1 && last<0))return result;
    result.valid=true;
    if(!hasPrevious)result.weights[next]=1;
    else {result.weights[next]+=progress;if(last>=0)result.weights[last]+=1-progress;}
    return result;
}
template<class T,class F> void WeatherParameters(T& p,F&& f) {
    f("ExposureOffset","Exposure Offset",p.exposure,-3.0f,3.0f,0.0f);
    f("ContrastMultiplier","Contrast Multiplier",p.contrast,0.0f,2.0f,1.0f);
    f("SaturationMultiplier","Saturation Multiplier",p.saturation,0.0f,2.0f,1.0f);
    f("TemperatureOffset","Temperature Offset",p.temperature,-100.0f,100.0f,0.0f);
    f("BloomMultiplier","Bloom Multiplier",p.bloom,0.0f,2.0f,1.0f);
    f("FogMultiplier","Fog Multiplier",p.fog,0.0f,2.0f,1.0f);
}
class WeatherTransition {
    WeatherObservation last_;
    WeatherAdjustment current_;
public:
    void Reset() noexcept {*this=WeatherTransition{};}
    ColorSettings Update(ColorSettings base,const WeatherSettings& settings,Environment environment,
        WeatherObservation observed,float seconds) noexcept {
        if(environment==Environment::Interior){current_=WeatherAdjustment{};return base;}
        if(observed.valid) {
            float sum=0;bool valid=true;
            for(float w:observed.weights){valid&=std::isfinite(w) && w>=0 && w<=1;sum+=w;}
            if(valid && std::abs(sum-1)<0.001f)last_=observed;
        }
        WeatherAdjustment target;
        if(settings.enabled && last_.valid) {
            target={0,0,0,0,0,0};
            for(unsigned i=0;i<4;++i) {
                auto add=[w=last_.weights[i]](float& to,float from){to+=from*w;};
                const auto& p=settings.profiles[i];
                add(target.exposure,p.exposure);add(target.contrast,p.contrast);add(target.saturation,p.saturation);
                add(target.temperature,p.temperature);add(target.bloom,p.bloom);add(target.fog,p.fog);
            }
        }
        const float duration=std::isfinite(settings.transitionSeconds)?std::clamp(settings.transitionSeconds,0.0f,10.0f):2;
        const float delta=std::isfinite(seconds)?std::max(seconds,0.0f):0;
        const float t=duration<=0?1:1-std::exp(-3*delta/duration);
        auto mix=[t](float& a,float b){a+=(b-a)*t;if(std::abs(a-b)<0.00001f)a=b;};
        mix(current_.exposure,target.exposure);mix(current_.contrast,target.contrast);mix(current_.saturation,target.saturation);
        mix(current_.temperature,target.temperature);mix(current_.bloom,target.bloom);mix(current_.fog,target.fog);
        if(environment!=Environment::Exterior)return base;
        base.exposure=std::clamp(base.exposure+current_.exposure,-3.0f,3.0f);
        base.contrast=std::clamp(base.contrast*current_.contrast,0.0f,2.0f);
        base.saturation=std::clamp(base.saturation*current_.saturation,0.0f,2.0f);
        base.temperature=std::clamp(base.temperature+current_.temperature,-100.0f,100.0f);
        base.bloom.intensity=std::clamp(base.bloom.intensity*current_.bloom,0.0f,2.0f);
        base.fog.density=std::clamp(base.fog.density*current_.fog,0.0f,1.0f);
        return base;
    }
};
}
