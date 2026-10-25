#pragma once
#include "settings.h"
#include "depth_inputs.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <cmath>
#include <string>
#include <array>
namespace cge {
class DepthOfFieldRenderer {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext1> context_;
    Ptr<ID3DDeviceContextState> isolated_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> pixel_,focusPixel_;
    struct FocusSurface {
        Ptr<ID3D11Texture2D> texture;
        Ptr<ID3D11ShaderResourceView> view;
        Ptr<ID3D11RenderTargetView> target;
    };
    std::array<FocusSurface,2> focus_;
    unsigned focusIndex_{};
    bool focusInitialized_{};
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11Texture2D> source_;
    Ptr<ID3D11ShaderResourceView> sourceView_;
    unsigned width_{},height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    bool ready_{};
public:
    bool Initialize(ID3D11Device*,std::string& error);
    void ResetAutofocus() noexcept {focusInitialized_=false;}
    ID3D11Texture2D* FocusTexture() const noexcept {return focusInitialized_?focus_[focusIndex_].texture.Get():nullptr;}
    std::string Draw(ID3D11ShaderResourceView* scene,const DepthInputs&,const DofSettings&,bool enabled,float seconds=1.0f/60);
};
}
