#pragma once
#include "render_target_cache.h"
#include "settings.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>
#include <string>
namespace cge {
                                                                             
class BloomRenderer {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    struct Surface {
        Ptr<ID3D11Texture2D> texture;
        Ptr<ID3D11ShaderResourceView> view;
        Ptr<ID3D11RenderTargetView> target;
        unsigned width{},height{};
    };
    RenderTargetCache output_;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext1> context_;
    Ptr<ID3DDeviceContextState> isolated_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> bright_,downsample_,upsample_,composite_;
    Ptr<ID3D11SamplerState> sampler_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Surface source_;
    std::array<Surface,6> down_,up_;
    Ptr<ID3D11ShaderResourceView> dirtMask_;
    bool CreateDirtMask();
    bool ready_{};
    DXGI_FORMAT sourceFormat_{DXGI_FORMAT_UNKNOWN};
    bool CreateSurface(Surface&,unsigned,unsigned,DXGI_FORMAT,bool target);
    bool Resize(unsigned,unsigned,DXGI_FORMAT);
public:
    bool Initialize(ID3D11Device*,std::string& error);
    std::string Draw(ID3D11ShaderResourceView*,const BloomSettings&,bool enabled,const LensDirtSettings& dirt={});
};
}
