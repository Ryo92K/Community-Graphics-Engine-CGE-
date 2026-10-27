#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace cge;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());
    D3D11_TEXTURE2D_DESC d{};d.Width=64;d.Height=32;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R10G10B10A2_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,readback;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"10-bit output");
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"10-bit readback");
    constexpr std::uint32_t original=256u|(128u<<10)|(64u<<20)|(2u<<30);
    std::vector<std::uint32_t> input(64*32,original);
    auto upload=[&]{context->UpdateSubresource(output.Get(),0,nullptr,input.data(),64*4,0);};
    auto pixel=[&]{
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"Read output");
        const auto value=*static_cast<const std::uint32_t*>(mapped.pData);context->Unmap(readback.Get(),0);return value;
    };
    ColorSettings settings;settings.exposure=1;
    for(auto color:{DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
        DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709}) {
        renderer.SetColorSpace(color);upload();const auto status=renderer.Draw(output.Get(),settings,true);const auto value=pixel();
        if(color==DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) {
            Check(status=="native color pass active","10-bit SDR effects active");
            Check((value&1023)>=510 && (value&1023)<=514 && ((value>>10)&1023)>=254 && ((value>>10)&1023)<=258 &&
                ((value>>20)&1023)>=126 && ((value>>20)&1023)<=130,"Exposure reaches all 10-bit channels");
            Check((value>>30)==2,"10-bit alpha preserved");
        }else Check(status.find("unsupported output color space=")==0 && value==original,"HDR and wide-gamut output bypass unchanged");
    }
    upload();Check(renderer.Draw(output.Get(),settings,false)=="idle" && pixel()==original,"Disabled effects preserve 10-bit output");
    std::puts("PASS: 10-bit SDR, exposure, alpha, HDR bypass, wide-gamut bypass and color-space transitions.");
}
