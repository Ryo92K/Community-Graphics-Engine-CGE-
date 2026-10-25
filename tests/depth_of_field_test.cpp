#include "depth_of_field.h"
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
    DepthOfFieldRenderer dof;std::string error;Check(dof.Initialize(device.Get(),error),error.c_str());
    unsigned width=64,height=32;
    ComPtr<ID3D11Texture2D> scene,depth,readback;
    ComPtr<ID3D11ShaderResourceView> sceneView;ComPtr<ID3D11RenderTargetView> target;
    DepthInputs inputs;inputs.nearPlane=1;inputs.farPlane=20000;inputs.state=DepthState::Ready;
    auto create=[&](unsigned w,unsigned h) {
        context->ClearState();width=w;height=h;scene.Reset();depth.Reset();sceneView.Reset();target.Reset();readback.Reset();inputs.depth.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
        device->CreateShaderResourceView(scene.Get(),nullptr,&sceneView);device->CreateRenderTargetView(scene.Get(),nullptr,&target);
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        device->CreateTexture2D(&d,nullptr,&readback);
        d.Format=DXGI_FORMAT_R32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        device->CreateTexture2D(&d,nullptr,&depth);device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    };
    create(width,height);
    auto encode=[&](float distance){return inputs.farPlane*(distance-inputs.nearPlane)/(distance*(inputs.farPlane-inputs.nearPlane));};
    auto upload=[&](float backgroundDistance=10000) {
        std::vector<float> color(width*height*4),z(width*height,encode(backgroundDistance));
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            auto at=(y*width+x)*4;color[at+3]=0.3f;
            if(x==40)color[at]=color[at+1]=color[at+2]=1;
            if(x==16){color[at]=8;color[at+1]=color[at+2]=0;z[y*width+x]=encode(500);}
        }
        context->UpdateSubresource(scene.Get(),0,nullptr,color.data(),width*16,0);
        context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),width*4,0);
    };
    auto read=[&](unsigned x,unsigned channel=0) {
        context->CopyResource(readback.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"HDR readback");
        auto* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(map.pData)+(height/2)*map.RowPitch);
        float result=row[x*4+channel];Check(std::abs(row[x*4+3]-0.3f)<0.00001f,"Depth of field preserves alpha");
        context->Unmap(readback.Get(),0);return result;
    };
    DofSettings settings{true,1000,1};
    auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    const D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    upload();Check(dof.Draw(sceneView.Get(),inputs,settings,true)=="HDR depth of field active","Depth of field active");
    const float widePeak=read(40);
    Check(widePeak>0 && widePeak<0.5f && read(38)>0,"Distant texture blurs progressively");
    Check(read(16)==8 && read(16,1)==0,"Focused foreground remains sharp and HDR");
    Check(read(17)==0 && read(17,1)==0,"Foreground does not bleed into adjacent background");
    settings.strength=0.25f;upload();dof.Draw(sceneView.Get(),inputs,settings,true);
    Check(read(40)>widePeak,"Blur strength controls the blur extent");
    settings.strength=1;settings.focusDistance=50000;upload();dof.Draw(sceneView.Get(),inputs,settings,true);
    Check(read(40)==1 && read(38)==0,"Moving focus behind the scene restores sharp output");
    settings.focusDistance=1000;upload(800);dof.Draw(sceneView.Get(),inputs,settings,true);
    Check(read(40)==1,"In-focus scene stays sharp");
    settings.enabled=false;upload();dof.Draw(sceneView.Get(),inputs,settings,true);Check(read(40)==1,"Effect switch bypasses depth of field");
    settings.enabled=true;dof.Draw(sceneView.Get(),inputs,settings,false);Check(read(40)==1,"Master switch bypasses depth of field");
    settings.strength=0;Check(dof.Draw(sceneView.Get(),inputs,settings,true)=="Off","Zero strength bypass");settings.strength=1;
    auto validDepth=inputs.depth;inputs.depth.Reset();
    Check(dof.Draw(sceneView.Get(),inputs,settings,true)=="Depth data unavailable","Missing depth bypass");
    inputs.depth=validDepth;inputs.nearPlane=0;
    Check(dof.Draw(sceneView.Get(),inputs,settings,true)=="Camera data unavailable","Invalid projection bypass");inputs.nearPlane=1;
    settings.focusDistance=std::numeric_limits<float>::quiet_NaN();
    Check(dof.Draw(sceneView.Get(),inputs,settings,true)=="Invalid depth-of-field settings","Non-finite focus rejected");settings.focusDistance=1000;
    UINT count=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&count,&viewport);
    ComPtr<ID3D11RenderTargetView> actual;context->OMGetRenderTargets(1,&actual,nullptr);
    Check(count==1 && viewport.TopLeftX==3 && viewport.Width==12 && actual.Get()==target.Get(),"Restore native output and viewport");
    create(65,33);upload();dof.Draw(sceneView.Get(),inputs,settings,true);Check(read(40)<0.5f,"Odd resolution resize");
    D3D11_TEXTURE2D_DESC d{};scene->GetDesc(&d);d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    ComPtr<ID3D11Texture2D> half;ComPtr<ID3D11ShaderResourceView> halfView;
    device->CreateTexture2D(&d,nullptr,&half);device->CreateShaderResourceView(half.Get(),nullptr,&halfView);
    Check(dof.Draw(halfView.Get(),inputs,settings,true)=="HDR depth of field active","Native FP16 HDR format");
    std::puts("PASS: manual focus, distant blur, foreground protection, strength, HDR, alpha, switches, resize and state.");
}
