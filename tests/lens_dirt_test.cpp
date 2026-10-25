#include "bloom.h"
#include <d3d11.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    BloomRenderer bloom;std::string error;Check(bloom.Initialize(device.Get(),error),error.c_str());
    unsigned width{},height{};ComPtr<ID3D11Texture2D> scene,staging;ComPtr<ID3D11ShaderResourceView> input;ComPtr<ID3D11RenderTargetView> target;
    std::vector<float> pixels;
    auto resize=[&](unsigned w,unsigned h) {
        width=w;height=h;scene.Reset();staging.Reset();input.Reset();target.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
        Check(SUCCEEDED(device->CreateShaderResourceView(scene.Get(),nullptr,&input)),"HDR input");
        Check(SUCCEEDED(device->CreateRenderTargetView(scene.Get(),nullptr,&target)),"HDR target");
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"HDR readback");pixels.resize(w*h*4);
    };
    BloomSettings settings{true,0.5f,1,32};LensDirtSettings dirt{true,1};resize(128,64);
    auto upload=[&](float brightness) {
        for(std::size_t i=0;i<pixels.size();i+=4){const auto x=(i/4)%width,y=(i/4)/width;const float light=brightness>1 && (x<width*3/8 || x>=width*5/8 || y<height*3/8 || y>=height*5/8)?0.05f:brightness;pixels[i]=light;pixels[i+1]=light*0.5f;pixels[i+2]=light*0.25f;pixels[i+3]=0.3f;}
        context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);
    };
    auto read=[&]() {
        context->CopyResource(staging.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"HDR map");std::vector<float> result(pixels.size());
        for(unsigned y=0;y<height;++y)memcpy(result.data()+y*width*4,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*16);
        context->Unmap(staging.Get(),0);
        for(std::size_t i=0;i<result.size();i+=4){for(unsigned c=0;c<3;++c)Check(std::isfinite(result[i+c]) && result[i+c]>=0 && result[i+c]<=65000,"Bounded finite output");Check(std::abs(result[i+3]-0.3f)<0.00001f,"Alpha preserved");}
        return result;
    };
    auto render=[&](LensDirtSettings effect,bool master=true){upload(8);bloom.Draw(input.Get(),settings,master,effect);return read();};
    const auto baseline=render({false,1});const auto full=render(dirt);
    Check(full!=baseline,"Dirt changes illuminated bloom");
    float smallest=100,largest=0;
    for(std::size_t i=0;i<full.size();i+=4) {
        const float delta=full[i]-baseline[i];smallest=std::min(smallest,delta);largest=std::max(largest,delta);
        Check(delta>=-0.00001f,"Dirt adds scattered light without darkening the source");
        Check(std::abs(delta-2*(full[i+1]-baseline[i+1]))<0.0001f && std::abs(delta-4*(full[i+2]-baseline[i+2]))<0.0001f,"Dirt preserves bloom hue");
    }
    Check(largest>0.5f && largest-smallest>0.5f,"Pattern has visible smudges and clean regions");
    Check(render(dirt)==full,"Dirt pattern is temporally stable");
    BloomRenderer second;Check(second.Initialize(device.Get(),error),error.c_str());upload(8);second.Draw(input.Get(),settings,true,dirt);
    Check(read()==full,"Mask is deterministic across renderer recreation");
    auto half=render({true,0.5f});
    for(std::size_t i=0;i<half.size();i+=4)Check(std::abs((half[i]-baseline[i])*2-(full[i]-baseline[i]))<0.0001f,"Strength blends lens dirt linearly");
    Check(render({true,0})==baseline && render({false,1})==baseline,"Zero strength and effect switch preserve prior bloom exactly");
    Check(render({true,-1})==baseline,"Negative strength bypasses dirt");
    Check(render({true,2})==full,"Strength clamps at one");
    Check(render({true,std::numeric_limits<float>::quiet_NaN()})==baseline,"Non-finite dirt strength preserves bloom");
    Check(render(dirt,false)==pixels,"Master switch bypasses both effects");
    settings.enabled=false;Check(render(dirt)==pixels,"Dirt cannot run when bloom is disabled");settings.enabled=true;
    settings.intensity=0;Check(render(dirt)==pixels,"Dirt cannot run with zero bloom intensity");settings.intensity=0.5f;
    upload(0.05f);bloom.Draw(input.Get(),settings,true,dirt);Check(read()==pixels,"Dark image does not show an overlay");
    for(std::size_t i=0;i<pixels.size();i+=4){pixels[i]=8;pixels[i+1]=4;pixels[i+2]=2;pixels[i+3]=0.3f;}
    context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);bloom.Draw(input.Get(),settings,true,{});const auto ambient=read();
    context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);bloom.Draw(input.Get(),settings,true,dirt);
    Check(read()==ambient,"Uniform bright ambient illumination does not reveal dirt");
    auto sideResponse=[&](unsigned centerX) {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const float value=std::abs(int(x)-int(centerX))<int(width/12) && std::abs(int(y)-int(height/2))<int(height/8)?32.0f:0.05f;
            const auto i=(y*width+x)*4;pixels[i]=value;pixels[i+1]=value*0.5f;pixels[i+2]=value*0.25f;pixels[i+3]=0.3f;
        }
        context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);bloom.Draw(input.Get(),settings,true,{});const auto clean=read();
        context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);bloom.Draw(input.Get(),settings,true,dirt);const auto lit=read();
        double weight=0,moment=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto i=(y*width+x)*4;const double delta=std::max(0.0f,lit[i]-clean[i]);weight+=delta;moment+=delta*x;}
        Check(weight>0.01,"Visible light produces a dirt response");return moment/weight;
    };
    const auto left=sideResponse(width/4),right=sideResponse(width*3/4);
    Check(left<width*0.45 && right>width*0.55,"Dirt response follows the visible light from left to right");
    auto gated=dirt;gated.threshold=8;upload(8);bloom.Draw(input.Get(),settings,true,gated);const auto highThreshold=read();
    upload(8);bloom.Draw(input.Get(),settings,true,{});Check(read()==highThreshold,"Light Threshold can reject weaker visible sources");
    auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);D3D11_VIEWPORT sentinel{3,4,20,12,0,1};context->RSSetViewports(1,&sentinel);
    upload(8);bloom.Draw(input.Get(),settings,true,dirt);UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    ComPtr<ID3D11RenderTargetView> restoredTarget;context->OMGetRenderTargets(1,&restoredTarget,nullptr);
    Check(count==1 && restored.Width==20 && restored.TopLeftX==3 && restoredTarget.Get()==target.Get(),"Complete native output state restored");
    context->ClearState();
    for(auto dims:{std::pair{65u,33u},std::pair{64u,128u},std::pair{256u,64u},std::pair{1u,1u}}) {
        resize(dims.first,dims.second);const auto result=render(dirt);Check(!result.empty(),"Resize and aspect ratio changes are safe");
    }
    D3D11_TEXTURE2D_DESC d{};scene->GetDesc(&d);d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    ComPtr<ID3D11Texture2D> ldr;ComPtr<ID3D11ShaderResourceView> ldrView;device->CreateTexture2D(&d,nullptr,&ldr);device->CreateShaderResourceView(ldr.Get(),nullptr,&ldrView);
    Check(bloom.Draw(ldrView.Get(),settings,true,dirt)=="Unsupported HDR input","Composited SDR HUD cannot receive lens dirt");
    std::puts("PASS: lens dirt pattern, light response, hue, strength, bypasses, stability, alpha, resize and native state.");
}
