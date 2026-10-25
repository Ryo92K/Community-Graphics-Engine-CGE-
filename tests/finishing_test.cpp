#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* text){if(!ok){std::printf("FAIL: %s\n",text);std::exit(1);}}
float Luma(const Pixel& p){return p[0]*0.2126f+p[1]*0.7152f+p[2]*0.0722f;}
int main(int argc,char** argv) {
    const std::string mode=argc>1?argv[1]:"all";constexpr unsigned width=128,height=64;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    Renderer renderer;std::string error;
    if(!renderer.Initialize(device.Get(),error)){std::printf("%s\n",error.c_str());Check(false,"Renderer initialization");}
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,readback;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"Readback");
    std::vector<Pixel> input(width*height,Pixel{128,128,128,77});
    auto at=[](unsigned x,unsigned y){return y*width+x;};
    auto render=[&](const ColorSettings& settings,bool enabled=true,unsigned frame=11,MotionPass motion=MotionPass{}) {
        context->UpdateSubresource(output.Get(),0,nullptr,input.data(),width*4,0);
        renderer.Draw(output.Get(),settings,enabled,motion,nullptr,frame);
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"Map pixels");
        std::vector<Pixel> pixels(input.size());
        for(unsigned y=0;y<height;++y)memcpy(pixels.data()+y*width,static_cast<unsigned char*>(map.pData)+y*map.RowPitch,width*4);
        context->Unmap(readback.Get(),0);
        for(const auto& p:pixels)Check(p[3]==77,"Source alpha preserved");
        return pixels;
    };
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    Check(render(ColorSettings{})==input,"Old defaults preserve source");
    if(mode=="vignette") {
        ColorSettings settings;settings.vignette={true,1,0.35f,0.5f};
        const auto strong=render(settings);
        Check(strong[at(64,32)][0]==128 && strong[0][0]<5,"Vignette center and corners");
        settings.vignette.strength=0.25f;const auto gentle=render(settings);
        Check(gentle[0][0]>strong[0][0] && gentle[0][0]<128,"Vignette intensity");
        settings.vignette={true,0.7f,0.2f,0.1f};const auto hard=render(settings);
        settings.vignette.softness=0.8f;const auto soft=render(settings);
        Check(hard[at(100,32)][0]<soft[at(100,32)][0],"Vignette softness");
        settings.vignette.radius=0.9f;Check(render(settings)[at(100,32)][0]==128,"Vignette radius");
        settings.vignette.radius=1;Check(settings.Neutral() && render(settings)==input,"Full radius bypass");
        settings.vignette.radius=0.35f;settings.vignette.strength=0;
        Check(settings.Neutral() && render(settings)==input,"Zero vignette bypass");
        settings.vignette={false,1,0.35f,0.5f};Check(render(settings)==input,"Vignette switch bypass");
        settings.vignette.enabled=true;Check(render(settings,false)==input,"Vignette master bypass");
    }
    if(mode=="grain") {
        ColorSettings settings;settings.filmGrain={true,1,1};
        const auto first=render(settings,true,11),same=render(settings,true,11),next=render(settings,true,12);
        Check(first==same && first!=next,"Grain seed stable within tick, animated across ticks");
        int low=255,high=0;double mean=0;
        for(const auto& p:first){low=std::min(low,int(p[0]));high=std::max(high,int(p[0]));mean+=p[0];Check(p[0]==p[1] && p[1]==p[2],"Monochrome grain");}
        Check(low<124 && high>132 && std::abs(mean/first.size()-128)<1,"Visible grain with near-zero mean");
        settings.filmGrain.size=2;const auto large=render(settings);
        Check(large[0]==large[1] && large[0]==large[width],"Grain cell size");
        settings.filmGrain.size=1;
        for(unsigned char endpoint:{0,255}){std::fill(input.begin(),input.end(),Pixel{endpoint,endpoint,endpoint,77});Check(render(settings)==input,"Grain endpoints");}
        std::fill(input.begin(),input.end(),Pixel{128,128,128,77});
        d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Format=DXGI_FORMAT_R32G32_FLOAT;
        ComPtr<ID3D11Texture2D> velocity,depth;std::vector<float> vectors(width*height*2,0),z(width*height,0.5f);
        D3D11_SUBRESOURCE_DATA v{vectors.data(),width*8,0};Check(SUCCEEDED(device->CreateTexture2D(&d,&v,&velocity)),"Velocity");
        d.Format=DXGI_FORMAT_R32_FLOAT;D3D11_SUBRESOURCE_DATA dz{z.data(),width*4,0};
        Check(SUCCEEDED(device->CreateTexture2D(&d,&dz,&depth)),"Depth");
        MotionPass motion;motion.settings={true,0.5f,24,12};motion.seconds=1.0f/60;motion.inputs.state=MotionState::Ready;
        device->CreateShaderResourceView(velocity.Get(),nullptr,&motion.inputs.velocity);device->CreateShaderResourceView(depth.Get(),nullptr,&motion.inputs.depth);
        Check(render(settings,true,11,motion)==first,"Grain follows stationary motion without loss or duplication");
        for(unsigned i=0;i<width*height;++i)vectors[i*2]=0.02f;
        context->UpdateSubresource(velocity.Get(),0,nullptr,vectors.data(),width*8,0);
        const auto moving=render(settings,true,11,motion);
        for(unsigned i=0;i<moving.size();++i)Check(std::abs(int(moving[i][0])-int(first[i][0]))<=1,"Moving uniform scene retains post-blur grain");
        settings.filmGrain.strength=0;Check(settings.Neutral() && render(settings)==input,"Zero grain bypass");
        settings.filmGrain={false,1,1};Check(render(settings)==input,"Grain switch bypass");
        settings.filmGrain.enabled=true;Check(render(settings,false)==input,"Grain master bypass");
    }
    if(mode=="split") {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const unsigned char v=x<64?51:204;input[at(x,y)]={v,v,v,77};}
        ColorSettings settings;settings.splitToning.enabled=true;settings.splitToning.strength=1;
        settings.splitToning.shadowRed=0;settings.splitToning.shadowGreen=0;settings.splitToning.shadowBlue=1;
        settings.splitToning.highlightRed=1;settings.splitToning.highlightGreen=0.5f;settings.splitToning.highlightBlue=0;
        const auto toned=render(settings);
        Check(toned[at(16,32)][2]>toned[at(16,32)][0] && toned[at(100,32)][0]>toned[at(100,32)][2],"Shadow and highlight palettes");
        for(unsigned i=0;i<input.size();++i)Check(std::abs(Luma(toned[i])-Luma(input[i]))<=1,"Toning luminance");
        std::fill(input.begin(),input.end(),Pixel{128,128,128,77});
        settings.splitToning.balance=0.25f;const auto warm=render(settings);
        settings.splitToning.balance=0.75f;const auto cool=render(settings);
        Check(warm[at(64,32)][0]>cool[at(64,32)][0] && warm[at(64,32)][2]<cool[at(64,32)][2],"Split balance");
        settings.splitToning.shadowRed=settings.splitToning.shadowGreen=settings.splitToning.shadowBlue=1;
        settings.splitToning.highlightRed=settings.splitToning.highlightGreen=settings.splitToning.highlightBlue=1;
        Check(render(settings)==input,"Neutral palettes");
        settings.splitToning.strength=0;Check(settings.Neutral() && render(settings)==input,"Zero split bypass");
        settings.splitToning={};Check(render(settings)==input,"Split switch bypass");
        settings.splitToning.enabled=true;Check(render(settings,false)==input,"Split master bypass");
    }
    if(mode=="aberration") {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto v=static_cast<unsigned char>(x*2);input[at(x,y)]={v,v,v,77};}
        ColorSettings settings;settings.chromaticAberration={true,1,0.2f};const auto shifted=render(settings);
        Check(shifted[at(64,32)]==input[at(64,32)],"Aberration center protection");
        const auto edge=at(6,32);
        Check(shifted[edge][0]<input[edge][0] && shifted[edge][2]>input[edge][2] && shifted[edge][1]==input[edge][1],"Opposite red/blue shifts and unchanged green");
        settings.chromaticAberration.strength=0.25f;Check(render(settings)[edge][0]>=shifted[edge][0],"Aberration intensity");
        settings.chromaticAberration.radius=1;Check(settings.Neutral() && render(settings)==input,"Full center radius bypass");
        settings.chromaticAberration.radius=0.2f;settings.chromaticAberration.strength=0;Check(settings.Neutral() && render(settings)==input,"Zero aberration bypass");
        settings.chromaticAberration={false,1,0.2f};Check(render(settings)==input,"Aberration switch bypass");
        settings.chromaticAberration.enabled=true;Check(render(settings,false)==input,"Aberration master bypass");
    }
    ColorSettings combined;
    combined.vignette={true,0.4f,0.35f,0.5f};combined.filmGrain={true,0.3f,1};
    combined.splitToning.enabled=true;combined.chromaticAberration={true,0.5f,0.2f};
    const auto combination=render(combined,true,17);
    Check(combination!=input && combination==render(combined,true,17),"All four effects compose deterministically at a fixed grain tick");
    Check(render(combined,false)==input,"Master bypass preserves source with all four effects enabled");
    const Pixel hud={17,231,93,77};D3D11_BOX box{0,0,0,1,1,1};context->UpdateSubresource(output.Get(),0,&box,hud.data(),4,0);
    ColorSettings all;all.vignette.enabled=all.filmGrain.enabled=all.splitToning.enabled=all.chromaticAberration.enabled=true;
    renderer.Draw(output.Get(),all,false);context->CopyResource(readback.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"HUD readback");
    Check(memcmp(m.pData,hud.data(),4)==0,"HUD untouched");context->Unmap(readback.Get(),0);
    UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    Check(restored.TopLeftX==3 && restored.Width==12,"Viewport restored");
    std::puts("PASS: finishing output, alpha, bypasses, HUD and state.");
}
