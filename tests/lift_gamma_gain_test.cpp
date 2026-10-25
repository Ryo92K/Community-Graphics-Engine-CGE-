#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    constexpr unsigned width=256,height=4;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=format;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output,readback;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"Readback");
        std::vector<Pixel> input(width*height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)input[y*width+x]={static_cast<unsigned char>(x),static_cast<unsigned char>(x),static_cast<unsigned char>(x),77};
        auto render=[&](const ColorSettings& settings,bool enabled=true) {
            auto bytes=input;if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:bytes)std::swap(p[0],p[2]);
            context->UpdateSubresource(output.Get(),0,nullptr,bytes.data(),width*4,0);renderer.Draw(output.Get(),settings,enabled);
            context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};
            Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"Map output");
            std::vector<Pixel> pixels(input.size());
            for(unsigned y=0;y<height;++y)memcpy(pixels.data()+y*width,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*4);
            context->Unmap(readback.Get(),0);
            if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:pixels)std::swap(p[0],p[2]);
            for(const auto& p:pixels)Check(p[3]==77,"LGG preserves alpha");
            return pixels;
        };
        ColorSettings settings;
        Check(render(settings)==input,"Old defaults preserve the image");
        settings.liftGammaGain.enabled=true;
        Check(settings.Neutral() && render(settings)==input,"Enabled neutral controls are an exact bypass");
        settings.exposure=0.5f;settings.contrast=1.1f;settings.gamma=1.2f;
        const auto identity=render(settings);settings.liftGammaGain.enabled=false;
        Check(render(settings)==identity,"Neutral LGG preserves other color grading exactly");
        settings=ColorSettings{};auto& lgg=settings.liftGammaGain;lgg.enabled=true;
        lgg.liftRed=lgg.liftGreen=lgg.liftBlue=0.1f;
        const auto lifted=render(settings);
        Check(lifted[0][0]>20 && lifted[255][0]==255,"Lift raises blacks and preserves white");
        Check(int(lifted[16][0])-16>int(lifted[128][0])-128 &&
            int(lifted[128][0])-128>int(lifted[240][0])-240,"Lift weights shadows more strongly");
        lgg.liftRed=lgg.liftGreen=lgg.liftBlue=-0.1f;
        const auto lowered=render(settings);Check(lowered[0][0]==0 && lowered[128][0]<128 && lowered[255][0]==255,"Negative lift deepens shadows");
        lgg=LiftGammaGainSettings{};lgg.enabled=true;lgg.gammaRed=lgg.gammaGreen=lgg.gammaBlue=2;
        const auto brightMid=render(settings);
        Check(brightMid[0][0]==0 && brightMid[255][0]==255 && brightMid[128][0]>170,"Gamma adjusts midtones and retains endpoints");
        lgg.gammaRed=lgg.gammaGreen=lgg.gammaBlue=0.5f;
        Check(render(settings)[128][0]<70,"Lower gamma darkens midtones");
        lgg=LiftGammaGainSettings{};lgg.enabled=true;lgg.gainRed=lgg.gainGreen=lgg.gainBlue=0.5f;
        const auto gained=render(settings);
        Check(gained[0][0]==0 && 240-int(gained[240][0])>128-int(gained[128][0]),"Gain changes highlights more strongly in absolute brightness");
        lgg=LiftGammaGainSettings{};lgg.enabled=true;lgg.gainRed=0.5f;
        const auto red=render(settings);
        Check(red[128][0]<70 && red[128][1]==128 && red[128][2]==128,"Gain RGB channels are independent");
        lgg=LiftGammaGainSettings{};lgg.enabled=true;lgg.gammaGreen=2;
        const auto green=render(settings);Check(green[128][0]==128 && green[128][1]>170 && green[128][2]==128,"Gamma RGB channels are independent");
        lgg=LiftGammaGainSettings{};lgg.enabled=true;lgg.liftBlue=0.1f;
        const auto blue=render(settings);Check(blue[32][0]==32 && blue[32][1]==32 && blue[32][2]>32,"Lift RGB channels are independent");
        lgg.liftRed=0.15f;lgg.gammaGreen=1.6f;lgg.gainBlue=0.7f;lgg.strength=1;
        const auto full=render(settings);lgg.strength=0.5f;const auto half=render(settings);
        for(unsigned i=0;i<half.size();++i)for(unsigned c=0;c<3;++c)
            Check(std::abs(int(half[i][c])*2-int(full[i][c])-int(input[i][c]))<=2,"Strength blends toward the ungraded image");
        lgg.strength=0;Check(settings.Neutral() && render(settings)==input,"Zero strength exact bypass");
        lgg.strength=1;lgg.enabled=false;Check(render(settings)==input,"Effect switch bypass");
        lgg.enabled=true;Check(render(settings,false)==input,"Master switch bypass");
        lgg.gainRed=0;Check(render(settings)[128][0]==0,"Zero channel gain is bounded");
        lgg.liftRed=-0.25f;lgg.gammaRed=0.5f;lgg.gainRed=2;
        Check(render(settings)[16][0]==0,"Extreme negative lift remains finite and clamped");
        const Pixel marker={17,231,93,77};D3D11_BOX box{0,0,0,1,1,1};
        context->UpdateSubresource(output.Get(),0,&box,marker.data(),4,0);renderer.Draw(output.Get(),settings,false);
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"HUD readback");
        Check(memcmp(m.pData,marker.data(),4)==0,"Composed HUD untouched");context->Unmap(readback.Get(),0);
    }
    std::puts("PASS: RGB Lift/Gamma/Gain, neutral identity, tone ranges, strength, alpha, formats and HUD.");
}
