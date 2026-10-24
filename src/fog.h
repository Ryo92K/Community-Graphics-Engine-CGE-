#pragma once
#include "settings.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <cmath>
#include <string>
namespace cge {
enum class FogDataState {Unavailable,CameraUnavailable,Ready};
struct FogInputs {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depth;
    float nearPlane{},farPlane{};
    FogDataState state{FogDataState::Unavailable};
};
inline bool ValidFogProjection(float nearPlane,float farPlane) noexcept {
    return std::isfinite(nearPlane) && std::isfinite(farPlane) &&
        nearPlane>0 && farPlane>nearPlane && farPlane<=1.0e8f;
}
class FogRenderer {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext1> context_;
    Ptr<ID3DDeviceContextState> isolated_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> pixel_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11Texture2D> source_;
    Ptr<ID3D11ShaderResourceView> sourceView_;
    unsigned width_{},height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    bool ready_{};
public:
    bool Initialize(ID3D11Device*,std::string& error);
    std::string Draw(ID3D11ShaderResourceView* scene,const FogInputs&,const FogSettings&,bool enabled);
};
}
