#include "depth_of_field.h"
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool ok,const char* text){if(!ok){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main() {
    constexpr unsigned width=64,height=32;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    DepthOfFieldRenderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> scene,depth,colorRead,focusRead;ComPtr<ID3D11ShaderResourceView> sceneView;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
    device->CreateShaderResourceView(scene.Get(),nullptr,&sceneView);
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    device->CreateTexture2D(&d,nullptr,&colorRead);
    d.Format=DXGI_FORMAT_R32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&d,nullptr,&depth);
    DepthInputs inputs;inputs.state=DepthState::Ready;inputs.nearPlane=1;inputs.farPlane=20000;
    device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    d.Width=d.Height=1;d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    device->CreateTexture2D(&d,nullptr,&focusRead);
    std::vector<float> colors(width*height*4),z(width*height);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const auto at=(y*width+x)*4;colors[at]=colors[at+1]=colors[at+2]=x==40?1.0f:0;colors[at+3]=0.3f;
    }
    auto encode=[&](float distance){return inputs.farPlane*(distance-inputs.nearPlane)/(distance*(inputs.farPlane-inputs.nearPlane));};
    auto fillDepth=[&](float distance){std::fill(z.begin(),z.end(),encode(distance));};
    auto upload=[&] {
        context->UpdateSubresource(scene.Get(),0,nullptr,colors.data(),width*16,0);
        context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),width*4,0);
    };
    auto focus=[&] {
        Check(renderer.FocusTexture()!=nullptr,"Focus state available");
        context->CopyResource(focusRead.Get(),renderer.FocusTexture());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(focusRead.Get(),0,D3D11_MAP_READ,0,&map)),"Focus readback");
        const float value=*static_cast<const float*>(map.pData);context->Unmap(focusRead.Get(),0);
        Check(std::isfinite(value) && value>=1 && value<=50000,"Finite bounded focus");return value;
    };
    auto peak=[&] {
        context->CopyResource(colorRead.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(colorRead.Get(),0,D3D11_MAP_READ,0,&map)),"HDR readback");
        const auto* row=reinterpret_cast<const float*>(static_cast<unsigned char*>(map.pData)+height/2*map.RowPitch);
        const float value=row[40*4];Check(std::abs(row[40*4+3]-0.3f)<0.00001f,"Autofocus preserves alpha");
        context->Unmap(colorRead.Get(),0);return value;
    };
    DofSettings settings{true,2000,1,true,5,0};
    auto draw=[&](float seconds=0.1f){upload();Check(renderer.Draw(sceneView.Get(),inputs,settings,true,seconds)=="HDR depth of field active (autofocus)","Auto DOF active");};
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    fillDepth(1000);draw(0);Check(std::abs(focus()-2000)<0.1f,"First auto frame seeds manual distance");
    draw();const float oneStep=focus();
    Check(std::abs(oneStep-(1000+1000*std::exp(-0.5f)))<1,"Exponential autofocus smoothing");
    renderer.ResetAutofocus();draw(0.05f);draw(0.05f);
    Check(std::abs(focus()-oneStep)<1,"Focus smoothing is frame-rate independent");
    const auto previous=focus();std::fill(z.begin(),z.end(),1);draw();
    Check(focus()==previous,"Sky preserves last valid focus");
    std::fill(z.begin(),z.end(),std::numeric_limits<float>::quiet_NaN());draw();
    Check(focus()==previous,"Invalid depth preserves last valid focus");
    fillDepth(1000);z[height/2*width+width/2]=encode(50);settings.focusDistance=1000;settings.focusRadius=0.1f;settings.focusSpeed=20;
    renderer.ResetAutofocus();draw();Check(std::abs(focus()-1000)<2,"Median zone rejects a tiny foreground outlier");
    settings.focusRadius=0;renderer.ResetAutofocus();draw();Check(focus()<300,"Point focus follows the center object");
    std::fill(z.begin(),z.end(),1);z[height/2*width+width/2]=encode(500);
    settings.focusDistance=1500;settings.focusRadius=0.1f;renderer.ResetAutofocus();draw();
    Check(std::abs(focus()-1500)<1,"Insufficient valid zone retains fallback distance");
    fillDepth(5000);settings.focusDistance=500;settings.focusRadius=0;settings.focusSpeed=1;
    renderer.ResetAutofocus();draw();const float slow=focus();
    settings.focusSpeed=20;renderer.ResetAutofocus();draw();Check(focus()>slow+1000,"Focus speed changes response");
    fillDepth(10000);settings.autofocus=false;settings.focusDistance=500;upload();renderer.Draw(sceneView.Get(),inputs,settings,true);
    const float manualPeak=peak();Check(manualPeak<0.5f && renderer.FocusTexture()==nullptr,"Manual mode retains manual blur");
    settings.autofocus=true;renderer.ResetAutofocus();draw();draw();draw();
    Check(peak()>0.99f && focus()>9900,"Autofocus changes actual DOF rendering");
    upload();renderer.Draw(sceneView.Get(),inputs,settings,false);
    Check(renderer.FocusTexture()==nullptr && peak()==1,"Master off releases focus state without rendering");
    settings.autofocus=false;settings.focusDistance=50000;upload();renderer.Draw(sceneView.Get(),inputs,settings,true);
    Check(peak()==1,"Manual focus restored after automatic mode");
    UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    Check(restored.TopLeftX==3 && restored.Width==12,"Autofocus restores game viewport");
    std::puts("PASS: autofocus median, smoothing, sky, invalid input, focus area, speed, manual mode and DOF output.");
}
