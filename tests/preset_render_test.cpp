#include "renderer.h"
#include "presets.h"
#include <d3d11.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
double Difference(const std::vector<Pixel>& a,const std::vector<Pixel>& b) {
    double sum=0;for(std::size_t i=0;i<a.size();++i)for(unsigned c=0;c<3;++c)sum+=std::abs(int(a[i][c])-int(b[i][c]));
    return sum/(a.size()*3);
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"LUT asset directory");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());renderer.SetLutDirectory(argv[1]);
    constexpr unsigned width=128,height=64;D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,staging;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Staging");
    const std::array<Pixel,8> palette={Pixel{140,140,140,77},Pixel{180,125,90,77},Pixel{55,110,170,77},Pixel{65,130,70,77},Pixel{160,75,60,77},Pixel{80,65,130,77},Pixel{210,195,160,77},Pixel{50,65,75,77}};
    std::vector<Pixel> input(width*height);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        auto p=palette[x/16];const float light=0.45f+0.55f*float(y)/float(height-1);
        for(unsigned c=0;c<3;++c)p[c]=static_cast<unsigned char>(p[c]*light);input[y*width+x]=p;
    }
    auto render=[&](const ColorSettings& settings) {
        context->UpdateSubresource(output.Get(),0,nullptr,input.data(),width*4,0);renderer.Draw(output.Get(),settings,true,{},nullptr,17);
        if(settings.lut.enabled)Check(renderer.LutStatus().find("LUT active:")!=std::string::npos,"Preset LUT asset loads");
        context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"Map");
        std::vector<Pixel> pixels(input.size());for(unsigned y=0;y<height;++y)memcpy(pixels.data()+y*width,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*4);context->Unmap(staging.Get(),0);
        for(const auto& p:pixels)Check(p[3]==77,"Preset preserves alpha");return pixels;
    };
    std::array<Settings,5> settings;for(unsigned i=0;i<5;++i)Check(presets::Apply(i,settings[i]),"Apply preset");
    for(unsigned scope=0;scope<3;++scope) {
        std::array<std::vector<Pixel>,5> images;
        for(unsigned i=0;i<5;++i) {
            const ColorSettings& p=scope==0?settings[i].exterior:scope==1?settings[i].interior:settings[i].exteriorNight;
            images[i]=render(p);Check(Difference(images[i],input)>1,"Every preset produces a visible color change");
        }
        for(unsigned a=0;a<5;++a)for(unsigned b=a+1;b<5;++b) {
            const auto difference=Difference(images[a],images[b]);std::printf("scope=%u pair=%u/%u mean difference=%.2f\n",scope,a,b,difference);
            Check(difference>3,"Preset styles remain visibly distinguishable");
        }
        const auto cool=images[1][48*width+8],warm=images[2][48*width+8];
        Check(int(cool[2])-int(cool[0])<=15,"Nordic neutral gray has no heavy blue cast");
        Check(warm[0]>cool[0] && warm[2]<cool[2],"Nordic and Fantasy keep distinct cool and warm balances");
        double chromaBefore=0,chromaAfter=0;
        for(unsigned stripe=1;stripe<8;++stripe) {
            const auto at=48*width+stripe*16+8;const auto a=input[at],b=images[3][at];
            chromaBefore+=int(std::max({a[0],a[1],a[2]}))-int(std::min({a[0],a[1],a[2]}));
            chromaAfter+=int(std::max({b[0],b[1],b[2]}))-int(std::min({b[0],b[1],b[2]}));
        }
        Check(chromaAfter>=0.9*chromaBefore,"Cinematic preserves scene color instead of approaching grayscale");
        Check(Difference(images[0],input)<Difference(images[3],input),"Natural keeps a lighter treatment than Cinematic");
    }
    std::puts("PASS: visible preset differences across day, interior and night, supplied LUTs and alpha.");
}
