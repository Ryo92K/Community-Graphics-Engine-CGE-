#pragma once
#include "settings.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <functional>
#include <string>
namespace cge {
using Microsoft::WRL::ComPtr;
class Renderer {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext1> context_;
    ComPtr<ID3DDeviceContextState> isolated_;
    ComPtr<ID3D11VertexShader> vertex_;
    ComPtr<ID3D11PixelShader> pixel_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11Texture2D> source_;
    ComPtr<ID3D11ShaderResourceView> sourceView_;
    unsigned width_{},height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
public:
    bool Initialize(ID3D11Device*,std::string& error);
    std::string Draw(ID3D11Texture2D*,const Settings&,const std::function<void()>& menu);
};
}
