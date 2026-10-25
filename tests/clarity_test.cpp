#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    Renderer renderer;std::string error;
    if(!renderer.Initialize(device.Get(),error)){std::printf("%s\n",error.c_str());Check(false,"Renderer initialization");}
    auto run=[&](unsigned height) {
        const unsigned width=160;
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Readback");
        std::vector<unsigned char> input(width*height*4);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned left=height==1080?74:77,right=height==1080?86:83;
            const unsigned char value=x>=left && x<right?145:120;
            const auto at=(y*width+x)*4;
            input[at]=input[at+1]=input[at+2]=value;input[at+3]=77;
        }
        auto render=[&](const ColorSettings& s,bool enabled=true) {
            context->UpdateSubresource(output.Get(),0,nullptr,input.data(),width*4,0);
            renderer.Draw(output.Get(),s,enabled);
            context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
            Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Map readback");
            std::vector<unsigned char> result(input.size());
            for(unsigned y=0;y<height;++y)memcpy(result.data()+y*width*4,static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,width*4);
            context->Unmap(staging.Get(),0);return result;
        };
        ColorSettings settings;
        Check(settings.Neutral() && render(settings)==input,"Neutral settings bypass");
        settings.clarity={true,0};
        Check(settings.Neutral() && render(settings)==input,"Zero strength is an exact bypass");
        settings.clarity={false,1};
        Check(render(settings)==input,"Effect switch bypass");
        settings.clarity={true,1};
        Check(!settings.Neutral() && render(settings,false)==input,"Master switch bypass");
        const auto strong=render(settings);
        settings.clarity.strength=0.25f;const auto gentle=render(settings);
        const auto center=(height/2*width+80)*4;
        Check(strong[center]>gentle[center] && gentle[center]>input[center],"Strength increases local contrast on the GPU");
        const auto outside=(height/2*width+(height==1080?70:75))*4;
        Check(strong[outside]<input[outside],"Local contrast darkens adjacent midtones");
        Check(strong[(height/2*width+10)*4]==120,"Flat areas stay unchanged");
        for(unsigned i=0;i<strong.size();i+=4) {
            Check(strong[i+3]==77,"Clarity preserves alpha");
            Check(strong[i]==strong[i+1] && strong[i]==strong[i+2],"Neutral colors stay neutral");
            Check(std::abs(int(strong[i])-int(input[i]))<=21,"Contrast adjustment is bounded");
        }
        settings.clarity.strength=1;settings.exposure=-1;
        const auto exposed=render(settings);
        Check(std::abs(int(exposed[center])*2-int(strong[center]))<=1,"Clarity composes with exposure");
        settings=ColorSettings{};settings.clarity={true,1};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            auto at=(y*width+x)*4;input[at]=input[at+1]=input[at+2]=x<80?0:255;
        }
        Check(render(settings)==input,"Black and white edges remain unchanged");
        D3D11_BOX hud{0,0,0,1,1,1};const unsigned char marker[]={17,231,93,77};
        context->UpdateSubresource(output.Get(),0,&hud,marker,4,0);
        renderer.Draw(output.Get(),settings,false);
        context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"HUD readback");
        Check(memcmp(m.pData,marker,4)==0,"Composed HUD remains untouched");
        context->Unmap(staging.Get(),0);
        return strong[center];
    };
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    const auto full=run(1080),half=run(540);
    Check(std::abs(int(full)-int(half))<=1,"Local contrast radius scales with render resolution");
    UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    Check(restored.TopLeftX==3 && restored.Width==12,"Game viewport restored");
    std::puts("PASS: clarity output, strength, bypasses, exposure, alpha, bounds, resolution and state.");
}
