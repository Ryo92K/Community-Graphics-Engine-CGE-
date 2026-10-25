#pragma once
#include "render_target_cache.h"
#include "settings.h"
#include "depth_inputs.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <cmath>
#include <string>
namespace cge {
class FogRenderer {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    RenderTargetCache output_;
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
    std::string Draw(ID3D11ShaderResourceView* scene,const DepthInputs&,const FogSettings&,bool enabled);
};
}
