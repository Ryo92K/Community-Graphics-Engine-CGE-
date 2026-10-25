#include "render_target_cache.h"
#include <d3d11.h>
#include <cstdio>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
void Check(bool v,const char* s){if(!v){std::printf("FAIL: %s\n",s);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=16;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> a,b;device->CreateTexture2D(&d,nullptr,&a);d.Width=17;device->CreateTexture2D(&d,nullptr,&b);
    cge::RenderTargetCache cache;ComPtr<ID3D11RenderTargetView> first=cache.Get(device.Get(),a.Get(),d.Format);
    Check(first!=nullptr,"Create cached view");
    for(unsigned i=0;i<100;++i)Check(cache.Get(device.Get(),a.Get(),d.Format)==first.Get(),"Reuse identical view without per-frame creation");
    ComPtr<ID3D11RenderTargetView> resized=cache.Get(device.Get(),b.Get(),d.Format);
    Check(resized && resized.Get()!=first.Get(),"Texture change invalidates view");
    Check(!cache.Get(device.Get(),b.Get(),DXGI_FORMAT_R32_FLOAT),"Invalid format cannot reuse stale view");
    Check(cache.Get(device.Get(),b.Get(),d.Format)!=nullptr,"Cache recovers from rejected view");
    cache.Reset();Check(cache.Get(device.Get(),a.Get(),d.Format)!=nullptr,"Reset allows reattachment");
    Check(!cache.Get(device.Get(),nullptr,d.Format),"Null resource clears cache");
    std::puts("PASS: render view reuse, resize, failure, reset and reattachment.");
}
