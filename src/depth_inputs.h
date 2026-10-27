#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>
#include "camera_pose.h"
namespace cge {
enum class DepthState {Unavailable,CameraUnavailable,Ready};
struct DepthInputs {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
    float nearPlane{},farPlane{};
    DepthState state{DepthState::Unavailable};
    float left{},right{},top{},bottom{};
    CameraPose camera;
    bool ValidFrustum() const noexcept {
        return std::isfinite(left) && std::isfinite(right) && std::isfinite(top) && std::isfinite(bottom) &&
            left<right && bottom<top && right-left>0.01f && top-bottom>0.01f &&
            std::abs(left)<20 && std::abs(right)<20 && std::abs(top)<20 && std::abs(bottom)<20;
    }
};
inline bool ValidDepthProjection(float nearPlane,float farPlane) noexcept {
    return std::isfinite(nearPlane) && std::isfinite(farPlane) &&
        nearPlane>0 && farPlane>nearPlane && farPlane<=1.0e8f;
}
}
