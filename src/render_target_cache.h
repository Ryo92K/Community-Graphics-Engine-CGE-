#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace cge {
class RenderTargetCache {
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
    ID3D11Texture2D* texture_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
public:
    void Reset() noexcept {view_.Reset();texture_=nullptr;format_=DXGI_FORMAT_UNKNOWN;}
    ID3D11RenderTargetView* Get(ID3D11Device* device,ID3D11Texture2D* texture,DXGI_FORMAT format) {
        if(!device || !texture){Reset();return nullptr;}
        if(view_ && texture_==texture && format_==format)return view_.Get();
        Reset();D3D11_RENDER_TARGET_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        if(FAILED(device->CreateRenderTargetView(texture,&d,&view_)))return nullptr;
        texture_=texture;format_=format;return view_.Get();
    }
};
}
