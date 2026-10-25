#pragma once
#include <cmath>
namespace cge {
class FpsCounter {
    double previous_{-1},elapsed_{};
    unsigned frames_{};
    float value_{};
public:
    void Tick(double now) noexcept {
        if(!std::isfinite(now) || now<0){*this=FpsCounter{};return;}
        if(previous_<0){previous_=now;return;}
        const double delta=now-previous_;previous_=now;
        if(delta<=0 || delta>2){elapsed_=0;frames_=0;value_=0;return;}
        elapsed_+=delta;++frames_;
        if(elapsed_>=0.5){value_=static_cast<float>(double(frames_)/elapsed_);elapsed_=0;frames_=0;}
    }
    float Value() const noexcept{return value_;}
};
}
