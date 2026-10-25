#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <string>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* text){if(!ok){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main() {
    constexpr unsigned width=128,height=32;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,readback;device->CreateTexture2D(&d,nullptr,&output);
    d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=0;device->CreateTexture2D(&d,nullptr,&readback);
    std::vector<Pixel> input(width*height);
    auto render=[&](const ColorSettings& settings,bool enabled=true) {
        context->UpdateSubresource(output.Get(),0,nullptr,input.data(),width*4,0);renderer.Draw(output.Get(),settings,enabled);
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"Readback");
        std::vector<Pixel> result(input.size());
        for(unsigned y=0;y<height;++y)memcpy(result.data()+y*width,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*4);
        context->Unmap(readback.Get(),0);for(const auto& p:result)Check(p[3]==77,"Debanding preserves alpha");
        return result;
    };
    auto bands=[&] {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const auto value=static_cast<unsigned char>(80+(x/8)*4);input[y*width+x]={value,value,value,77};
        }
    };
    bands();ColorSettings settings;Check(render(settings)==input,"Default bypass");
    settings.debanding={true,1,4};const auto filtered=render(settings);
    unsigned changed=0,intermediate=0;
    for(unsigned i=0;i<input.size();++i) {
        if(filtered[i][0]!=input[i][0])++changed;
        if((filtered[i][0]-80)%4)++intermediate;
        Check(std::abs(int(filtered[i][0])-int(input[i][0]))<=4,"Debanding correction stays bounded");
        Check(filtered[i][0]==filtered[i][1] && filtered[i][1]==filtered[i][2],"Neutral gradient remains neutral");
    }
    Check(changed>width && intermediate>width,"Band steps gain intermediate shades");
    Check(filtered==render(settings),"Dither pattern is stable across frames");
    settings.debanding.radius=1;Check(render(settings)!=filtered,"Radius changes filtering footprint");
    settings.debanding={true,0.25f,4};const auto gentle=render(settings);
    unsigned strongChange=0,gentleChange=0;
    for(unsigned i=0;i<input.size();++i){strongChange+=unsigned(std::abs(int(filtered[i][0])-int(input[i][0])));gentleChange+=unsigned(std::abs(int(gentle[i][0])-int(input[i][0])));}
    Check(strongChange>gentleChange,"Strength changes smoothing amount");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const unsigned char v=x<64?30:220;input[y*width+x]={v,v,v,77};}
    settings.debanding={true,1,4};Check(render(settings)==input,"Hard edges preserved");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const unsigned char v=((x+y)&1)?126:130;input[y*width+x]={v,v,v,77};}
    const auto detail=render(settings);
    for(unsigned i=0;i<input.size();++i)Check(std::abs(int(detail[i][0])-int(input[i][0]))<=1,"Fine texture extrema preserved");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto v=static_cast<unsigned char>(40+(x/8)*8);input[y*width+x]={v,v,v,77};}
    settings.debanding={true,1,4,0.025f};const auto rejected=render(settings);
    settings.debanding.threshold=0.05f;const auto accepted=render(settings);
    Check(rejected==input && accepted!=input,"Adjustable threshold admits stronger bands without forcing scene blur");
    std::fill(input.begin(),input.end(),Pixel{128,128,128,77});Check(render(settings)==input,"Flat areas unchanged");
    bands();settings.debanding.strength=0;Check(settings.Neutral() && render(settings)==input,"Zero strength exact bypass");
    settings.debanding={false,1,4};Check(render(settings)==input,"Effect switch bypass");
    settings.debanding.enabled=true;Check(render(settings,false)==input,"Master switch bypass");
    const Pixel marker={17,231,93,77};D3D11_BOX box{0,0,0,1,1,1};context->UpdateSubresource(output.Get(),0,&box,marker.data(),4,0);
    renderer.Draw(output.Get(),settings,false);context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};
    Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"HUD readback");Check(memcmp(m.pData,marker.data(),4)==0,"HUD untouched");context->Unmap(readback.Get(),0);
    std::puts("PASS: debanding gradients, radius, strength, dither, hard edges, fine detail, alpha and bypasses.");
}
