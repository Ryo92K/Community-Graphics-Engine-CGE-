#include "fog.h"
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &device,nullptr,&context)),"WARP device");
    FogRenderer fog;std::string error;Check(fog.Initialize(device.Get(),error),error.c_str());
    unsigned width=64,height=16;
    ComPtr<ID3D11Texture2D> scene,depth,staging;
    ComPtr<ID3D11ShaderResourceView> sceneView;ComPtr<ID3D11RenderTargetView> target;
    FogInputs inputs;inputs.nearPlane=1;inputs.farPlane=10000;inputs.state=FogDataState::Ready;
    auto create=[&](unsigned w,unsigned h) {
        context->ClearState();width=w;height=h;scene.Reset();sceneView.Reset();depth.Reset();inputs.depth.Reset();staging.Reset();target.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
        device->CreateShaderResourceView(scene.Get(),nullptr,&sceneView);device->CreateRenderTargetView(scene.Get(),nullptr,&target);
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        device->CreateTexture2D(&d,nullptr,&staging);
        d.Format=DXGI_FORMAT_R32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;
        device->CreateTexture2D(&d,nullptr,&depth);device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    };
    create(width,height);
    auto encode=[&](float distance){return inputs.farPlane*(distance-inputs.nearPlane)/(distance*(inputs.farPlane-inputs.nearPlane));};
    auto upload=[&] {
        const float color[]={0.1f,0.2f,0.3f,0.35f};context->ClearRenderTargetView(target.Get(),color);
        std::vector<float> z(width*height,encode(5000));
        for(unsigned y=0;y<height;++y) {
            z[y*width]=encode(500);z[y*width+1]=encode(2000);z[y*width+2]=encode(8000);
            z[y*width+3]=1;z[y*width+4]=-0.5f;z[y*width+5]=std::numeric_limits<float>::quiet_NaN();
        }
        context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),width*4,0);
    };
    auto read=[&](unsigned x) {
        context->CopyResource(staging.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"HDR readback");
        const auto* p=static_cast<const float*>(map.pData)+4*x;
        const float value=p[0];Check(std::abs(p[3]-0.35f)<0.00001f,"Fog preserves source alpha");
        context->Unmap(staging.Get(),0);return value;
    };
    FogSettings settings{true,0.5f,1000};
    auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    upload();Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="HDR fog active","Fog pass active");
    Check(std::abs(read(0)-0.1f)<0.00001f,"No fog before start distance");
    const float middle=read(1),farPixel=read(2);
    const float expected=0.1f+(0.65f-0.1f)*(1-std::exp(-1000.0f*0.5f/5000));
    Check(std::abs(middle-expected)<0.0001f && farPixel>middle && middle>0.1f,"Native depth reconstructs distance and increases fog progressively");
    Check(read(3)==0.1f && read(4)==0.1f && read(5)==0.1f,"Sky and invalid depth remain unchanged");
    settings.density=1;upload();fog.Draw(sceneView.Get(),inputs,settings,true);Check(read(2)>farPixel,"Density increases fog");
    settings.startDistance=50000;upload();fog.Draw(sceneView.Get(),inputs,settings,true);Check(read(2)==0.1f,"Start distance delays fog");
    settings.startDistance=1000;settings.enabled=false;upload();fog.Draw(sceneView.Get(),inputs,settings,true);Check(read(2)==0.1f,"Per-effect switch bypasses fog");
    settings.enabled=true;upload();fog.Draw(sceneView.Get(),inputs,settings,false);Check(read(2)==0.1f,"Master switch bypasses fog");
    settings.density=0;Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="Off","Zero density bypasses fog");settings.density=0.5f;
    inputs.nearPlane=0;Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="Camera data unavailable","Invalid projection bypass");
    inputs.nearPlane=1;auto validDepth=inputs.depth;inputs.depth.Reset();
    Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="Depth data unavailable","Missing depth bypass");
    inputs.depth=validDepth;settings.density=std::numeric_limits<float>::quiet_NaN();
    Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="Invalid fog settings","Non-finite controls rejected");settings.density=0.5f;
    UINT count=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&count,&viewport);
    ComPtr<ID3D11RenderTargetView> actual;context->OMGetRenderTargets(1,&actual,nullptr);
    Check(count==1 && viewport.TopLeftX==3 && viewport.Width==12 && actual.Get()==target.Get(),"Restore native viewport and target");
    create(33,15);upload();fog.Draw(sceneView.Get(),inputs,settings,true);Check(read(2)>0.1f,"Odd resolution resize");
    D3D11_TEXTURE2D_DESC d{};depth->GetDesc(&d);d.Width=2;
    ComPtr<ID3D11Texture2D> smallDepth;device->CreateTexture2D(&d,nullptr,&smallDepth);
    inputs.depth.Reset();device->CreateShaderResourceView(smallDepth.Get(),nullptr,&inputs.depth);
    Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="Unsupported depth dimensions","Mismatched depth size bypass");
    inputs.depth.Reset();device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    // Exercise native 24-bit typeless depth separately from the R32 fixture.
    d.Width=width;d.Format=DXGI_FORMAT_R24G8_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> nativeDepth;ComPtr<ID3D11DepthStencilView> dsv;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&nativeDepth)),"Typeless depth");
    D3D11_DEPTH_STENCIL_VIEW_DESC ds{};ds.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;ds.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(nativeDepth.Get(),&ds,&dsv);context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,encode(8000),0);
    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=1;
    inputs.depth.Reset();device->CreateShaderResourceView(nativeDepth.Get(),&sv,&inputs.depth);
    upload();Check(fog.Draw(sceneView.Get(),inputs,settings,true)=="HDR fog active" && read(2)>0.1f,"Native D24 depth format");
    d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> half;ComPtr<ID3D11ShaderResourceView> halfView;
    device->CreateTexture2D(&d,nullptr,&half);device->CreateShaderResourceView(half.Get(),nullptr,&halfView);
    Check(fog.Draw(halfView.Get(),inputs,settings,true)=="HDR fog active","Native FP16 HDR format");
    std::puts("PASS: depth fog distance, density, sky, alpha, switches, projection, formats, resize and state.");
}
