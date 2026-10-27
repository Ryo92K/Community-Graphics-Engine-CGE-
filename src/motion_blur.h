#pragma once
#include "settings.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
namespace cge {
enum class MotionState { Off, Ready, Unavailable, TaaDisabled, Paused, Warming, Unsupported };
inline const char* MotionLabel(MotionState state) noexcept {
    switch(state) {
        case MotionState::Off:return "Off";
        case MotionState::Ready:return "Ready";
        case MotionState::TaaDisabled:return "Native TAA required";
        case MotionState::Paused:return "Paused";
        case MotionState::Warming:return "Preparing";
        case MotionState::Unsupported:return "Unsupported motion buffers";
        default:return "Motion data unavailable";
    }
}
struct MotionInputs {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocity,depth;
    MotionState state{MotionState::Unavailable};
    const char* probeStatus{"Not sampled"};
    unsigned char taa{255};
    bool worldTaa{};
};
struct MotionPass {
    MotionInputs inputs;
    MotionSettings settings;
    float seconds{1.0f/60.0f};
};
inline float MotionExposure(float strength,float seconds) noexcept {
    if(!std::isfinite(strength)||!std::isfinite(seconds)||seconds<=0 || seconds>0.1f)return 0;
                                                                                      
    return std::clamp(strength,0.0f,10.0f)/(60.0f*std::max(seconds,1.0f/240.0f));
}
}
