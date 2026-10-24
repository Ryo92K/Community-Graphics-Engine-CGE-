#include "bloom.h"
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &device,nullptr,&context)),"WARP device");
    BloomRenderer bloom;std::string error;Check(bloom.Initialize(device.Get(),error),error.c_str());
    unsigned width=128,height=64;
    ComPtr<ID3D11Texture2D> scene,staging;
    ComPtr<ID3D11ShaderResourceView> input;ComPtr<ID3D11RenderTargetView> target;
    std::vector<float> pixels;
    auto resize=[&](unsigned w,unsigned h) {
        width=w;height=h;scene.Reset();input.Reset();target.Reset();staging.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
        Check(SUCCEEDED(device->CreateShaderResourceView(scene.Get(),nullptr,&input)),"HDR input");
        Check(SUCCEEDED(device->CreateRenderTargetView(scene.Get(),nullptr,&target)),"HDR output");
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"HDR readback");
        pixels.assign(w*h*4,0);
    };
    resize(width,height);
    BloomSettings settings{true,1,1,32};
    auto upload=[&](bool light=true,float brightness=8) {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            auto at=(y*width+x)*4;const bool bright=light && x>=width/2-2 && x<width/2+2 && y>=height/2-2 && y<height/2+2;
            pixels[at]=bright?brightness:0.05f;pixels[at+1]=bright?brightness*0.5f:0.05f;pixels[at+2]=bright?brightness*0.25f:0.05f;pixels[at+3]=0.3f;
        }
        context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),width*16,0);
    };
    auto read=[&](unsigned x,unsigned y,unsigned channel=0) {
        context->CopyResource(staging.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"Map HDR pixels");
        const float* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(map.pData)+map.RowPitch*y);
        const float value=row[x*4+channel];Check(std::abs(row[x*4+3]-0.3f)<0.00001f,"Preserve native HDR alpha");
        context->Unmap(staging.Get(),0);return value;
    };
    auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    upload();Check(bloom.Draw(input.Get(),settings,true)=="HDR bloom active","HDR pass active");
    Check(read(width/2,height/2)>8,"Source highlights retain HDR values above one");
    Check(read(width/2+5,height/2)>0.05f,"Bright source produces a halo on neighboring dark pixels");
    Check(read(width/2+5,height/2)>read(width/2+5,height/2,1),"Halo preserves source hue");
    const float broad=read(width/2+12,height/2);
    settings.radius=1;upload();bloom.Draw(input.Get(),settings,true);
    Check(broad>read(width/2+12,height/2)+0.00001f,"Radius changes halo spread");
    settings.radius=64;upload();bloom.Draw(input.Get(),settings,true);
    const float strongest=read(width/2+12,height/2);
    settings.intensity=0.25f;upload();bloom.Draw(input.Get(),settings,true);
    Check(strongest>read(width/2+12,height/2),"Intensity controls halo strength");
    settings.threshold=1;upload(true,0.5f);bloom.Draw(input.Get(),settings,true);
    Check(read(width/2+5,height/2)==0.05f && read(width/2,height/2)==0.5f,"Threshold excludes highlights below the knee");
    settings.threshold=1;settings.intensity=1;upload(false);bloom.Draw(input.Get(),settings,true);
    Check(read(width/2,height/2)==0.05f,"Dark image remains unchanged");
    settings.enabled=false;upload();bloom.Draw(input.Get(),settings,true);
    Check(read(width/2,height/2)==8 && read(width/2+5,height/2)==0.05f,"Effect switch bypasses bloom");
    settings.enabled=true;upload();bloom.Draw(input.Get(),settings,false);
    Check(read(width/2,height/2)==8,"Master switch bypasses bloom");
    Check(bloom.Draw(nullptr,settings,true)=="HDR input unavailable","Missing HDR input bypasses safely");
    settings.radius=std::numeric_limits<float>::quiet_NaN();
    Check(bloom.Draw(input.Get(),settings,true)=="Invalid settings","Non-finite settings bypass");
    settings.radius=32;
    UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    ComPtr<ID3D11RenderTargetView> restoredTarget;context->OMGetRenderTargets(1,&restoredTarget,nullptr);
    Check(count==1 && restored.TopLeftX==3 && restored.Width==12 && restoredTarget.Get()==target.Get(),"Restore complete game output state");
    context->ClearState();resize(65,33);upload();bloom.Draw(input.Get(),settings,true);
    Check(read(width/2+5,height/2)>0.05f,"Odd-resolution resize rebuilds resources");
                                                                           
    D3D11_TEXTURE2D_DESC d{};scene->GetDesc(&d);d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    ComPtr<ID3D11Texture2D> half;ComPtr<ID3D11ShaderResourceView> halfView;ComPtr<ID3D11RenderTargetView> halfTarget;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&half)),"FP16 target");
    device->CreateShaderResourceView(half.Get(),nullptr,&halfView);device->CreateRenderTargetView(half.Get(),nullptr,&halfTarget);
    const float hdr[4]={8,4,2,0.3f};context->ClearRenderTargetView(halfTarget.Get(),hdr);
    Check(bloom.Draw(halfView.Get(),settings,true)=="HDR bloom active","Native FP16 format");
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;ComPtr<ID3D11Texture2D> ldr;ComPtr<ID3D11ShaderResourceView> ldrView;
    device->CreateTexture2D(&d,nullptr,&ldr);device->CreateShaderResourceView(ldr.Get(),nullptr,&ldrView);
    Check(bloom.Draw(ldrView.Get(),settings,true)=="Unsupported HDR input","Never applies HDR bloom to composited LDR UI");
    std::puts("PASS: HDR bloom, threshold, radius, intensity, hue, alpha, switches, resizing and state restoration.");
}
