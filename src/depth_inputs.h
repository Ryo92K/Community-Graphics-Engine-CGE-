#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>
namespace cge {
enum class DepthState {Unavailable,CameraUnavailable,Ready};
struct DepthInputs {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
    float nearPlane{},farPlane{};
    DepthState state{DepthState::Unavailable};
};
inline bool ValidDepthProjection(float nearPlane,float farPlane) noexcept {
    return std::isfinite(nearPlane) && std::isfinite(farPlane) &&
        nearPlane>0 && farPlane>nearPlane && farPlane<=1.0e8f;
}
}
