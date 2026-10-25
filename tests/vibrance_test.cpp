#include "renderer.h"
#include <d3d11.h>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
float Luma(const Pixel& p){return p[0]*0.2126f+p[1]*0.7152f+p[2]*0.0722f;}
float Range(const Pixel& p){return float(std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]}));}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    Renderer renderer;std::string error;
    if(!renderer.Initialize(device.Get(),error)){std::printf("%s\n",error.c_str());Check(false,"Renderer initialization");}
    const std::vector<Pixel> input={{0,0,0,17},{255,255,255,31},{127,127,127,45},
        {255,0,0,59},{0,255,0,73},{0,0,255,87},{140,115,102,101},
        {230,26,26,115},{250,240,230,129},{25,20,18,143},{120,140,125,157},{80,90,120,171},{255,240,230,185}};
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        const auto width=static_cast<UINT>(input.size());
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=1;
        desc.ArraySize=desc.MipLevels=desc.SampleDesc.Count=1;desc.Format=format;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output,readback;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&output)),"Output texture");
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"Readback texture");
        auto render=[&](const ColorSettings& settings,bool enabled=true) {
            auto bytes=input;
            if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:bytes)std::swap(p[0],p[2]);
            context->UpdateSubresource(output.Get(),0,nullptr,bytes.data(),width*4,0);
            renderer.Draw(output.Get(),settings,enabled);
            context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE map{};
            Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"Map rendered image");
            std::vector<Pixel> result(input.size());memcpy(result.data(),map.pData,width*4);
            context->Unmap(readback.Get(),0);
            if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:result)std::swap(p[0],p[2]);
            return result;
        };
        ColorSettings settings;
        Check(settings.Neutral() && render(settings)==input,"Default settings preserve existing image");
        settings.vibrance={true,0};
        Check(settings.Neutral() && render(settings)==input,"Zero strength is exact bypass");
        settings.vibrance={false,1};
        Check(render(settings)==input,"Vibrance switch bypasses effect");
        settings.vibrance={true,1};
        Check(!settings.Neutral() && render(settings,false)==input,"Master switch bypasses Vibrance");
        const auto strong=render(settings);
        settings.vibrance.strength=0.25f;const auto gentle=render(settings);
        Check(Range(strong[6])>Range(gentle[6]) && Range(gentle[6])>Range(input[6]),"Strength increases muted color saturation");
        const float mutedBoost=(Range(strong[6])-Range(input[6]))/Range(input[6]);
        const float saturatedBoost=(Range(strong[7])-Range(input[7]))/Range(input[7]);
        Check(mutedBoost>saturatedBoost*4,"Already saturated colors receive less amplification");
        for(unsigned i=0;i<6;++i)Check(strong[i]==input[i],"Grays, endpoints and primary colors stay unchanged");
        Check(strong[12]==input[12],"Colors at a gamut boundary do not clip individual channels");
        Check(std::abs((int(strong[8][0])-int(strong[8][1]))-(int(strong[8][1])-int(strong[8][2])))<=1,"Gamut limiting retains chroma direction");
        for(unsigned i=0;i<input.size();++i) {
            Check(strong[i][3]==input[i][3],"Source alpha preserved");
            Check(std::abs(Luma(strong[i])-Luma(input[i]))<=1.1f,"Luminance retained within output precision");
            const auto inMax=std::max_element(input[i].begin(),input[i].begin()+3)-input[i].begin();
            const auto inMin=std::min_element(input[i].begin(),input[i].begin()+3)-input[i].begin();
            Check(strong[i][inMax]>=strong[i][inMin],"Color channel ordering preserved");
        }
        settings.vibrance.strength=1;settings.saturation=0;
        const auto grayscale=render(settings);
        for(const auto& p:grayscale)Check(p[0]==p[1] && p[1]==p[2],"Vibrance does not recolor desaturated output");
        settings.saturation=1;settings.exposure=-1;
        const auto exposed=render(settings);
        for(unsigned i=0;i<input.size();++i)Check(std::abs(Luma(exposed[i])-Luma(input[i])*0.5f)<=1.1f,"Exposure retains half luminance even at gamut boundaries");
        for(unsigned i:{6u,7u,9u,10u,11u})for(unsigned c=0;c<3;++c)
            Check(std::abs(int(exposed[i][c])*2-int(strong[i][c]))<=2,"Exposure and Vibrance compose without clipping");
        settings.exposure=0;settings.gamma=2;const auto gamma=render(settings);
        for(unsigned i=0;i<input.size();++i)for(unsigned c=0;c<3;++c)
            Check(std::abs(float(gamma[i][c])-255*std::sqrt(float(strong[i][c])/255))<=2,"Gamma follows Vibrance");
        D3D11_BOX hud{0,0,0,1,1,1};const Pixel marker={17,231,93,77};
        context->UpdateSubresource(output.Get(),0,&hud,marker.data(),4,0);
        renderer.Draw(output.Get(),settings,false);context->CopyResource(readback.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE map{};Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"HUD readback");
        Check(memcmp(map.pData,marker.data(),4)==0,"Composed HUD remains untouched");context->Unmap(readback.Get(),0);
    }
    std::puts("PASS: Vibrance strength, selectivity, luminance, alpha, neutral colors, format and grading composition.");
}
