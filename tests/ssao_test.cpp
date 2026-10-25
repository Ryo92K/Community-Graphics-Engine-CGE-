#include "ssao.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <algorithm>
using namespace cge;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &device,nullptr,&context)),"WARP device");
    SsaoRenderer ao;std::string error;Check(ao.Initialize(device.Get(),error),error.c_str());
    unsigned width=128,height=96;
    ComPtr<ID3D11Texture2D> scene,depth,staging;
    ComPtr<ID3D11ShaderResourceView> sceneView;ComPtr<ID3D11RenderTargetView> target;
    DepthInputs inputs;inputs.nearPlane=5;inputs.farPlane=20000;inputs.state=DepthState::Ready;
    inputs.left=-1;inputs.right=1;inputs.top=0.75f;inputs.bottom=-0.75f;
    std::vector<float> encoded;
    auto create=[&](unsigned w,unsigned h) {
        context->ClearState();width=w;height=h;scene.Reset();sceneView.Reset();depth.Reset();inputs.depth.Reset();staging.Reset();target.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"HDR scene");
        Check(SUCCEEDED(device->CreateShaderResourceView(scene.Get(),nullptr,&sceneView)),"HDR view");
        Check(SUCCEEDED(device->CreateRenderTargetView(scene.Get(),nullptr,&target)),"HDR target");
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Readback");
        d.Format=DXGI_FORMAT_R32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=0;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&depth)),"Depth");
        Check(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth)),"Depth view");
        encoded.resize(w*h);
    };
    auto encode=[&](float z){return inputs.farPlane*(z-inputs.nearPlane)/(z*(inputs.farPlane-inputs.nearPlane));};
    auto plane=[&](float distance,float slope=0) {
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const float ray=inputs.left+(float(x)+0.5f)/float(width)*(inputs.right-inputs.left);
            encoded[y*width+x]=encode(distance/(1-slope*ray));
        }
    };
    auto contact=[&](float distance=300) {
        plane(distance);
        for(unsigned y=height/4;y<height*3/4;++y)for(unsigned x=width/4;x<width/2;++x)encoded[y*width+x]=encode(distance-25);
    };
    auto upload=[&] {
        const float color[]={2,1,0.5f,0.35f};context->ClearRenderTargetView(target.Get(),color);
        context->UpdateSubresource(depth.Get(),0,nullptr,encoded.data(),width*4,0);
    };
    auto read=[&]() {
        context->CopyResource(staging.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"Readback map");
        std::vector<float> result(width*height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            auto* p=reinterpret_cast<const float*>(static_cast<const unsigned char*>(m.pData)+y*m.RowPitch)+x*4;
            Check(std::isfinite(p[0]) && p[0]>=0.29f && p[0]<=2.00001f,"Finite bounded HDR output");
            Check(std::abs(p[0]-2*p[1])<1e-5f && std::abs(p[0]-4*p[2])<1e-5f && p[3]==0.35f,"Preserve HDR hue and alpha");
            result[y*width+x]=p[0]/2;
        }
        context->Unmap(staging.Get(),0);return result;
    };
    auto at=[&](const std::vector<float>& image,unsigned x,unsigned y){return image[y*width+x];};
    create(width,height);SsaoSettings settings{true,1,80,0.05f,1.5f,8000,1};
    plane(300);upload();Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="HDR SSAO active (High)","SSAO active");
    const auto flat=read();Check(*std::min_element(flat.begin(),flat.end())>0.999f,"Flat plane has no self-occlusion");
    plane(300,0.4f);upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto slope=read();
    Check(*std::min_element(slope.begin(),slope.end())>0.995f,"Sloping plane has no self-occlusion");
    for(int quality=0;quality<3;++quality) {
        settings.quality=quality;contact();upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto shadow=read();
        const float nearContact=at(shadow,width/2+1,height/2);
        std::printf("Quality %d: contact %.5f, foreground %.5f, far %.5f\n",quality,nearContact,at(shadow,width/2-1,height/2),at(shadow,width*7/8,height/2));
        Check(nearContact<0.94f,"Contact geometry receives visible occlusion");
        Check(at(shadow,width/2-1,height/2)>0.995f,"Foreground silhouette does not receive a dark halo");
        Check(at(shadow,width*7/8,height/2)>0.999f,"Occlusion stops beyond world-space radius");
        upload();ao.Draw(sceneView.Get(),inputs,settings,true);Check(read()==shadow,"Static frame is deterministic without temporal shimmer");
        settings.strength=0.5f;upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto half=read();
        Check(std::abs(at(half,width/2+1,height/2)-(1+nearContact)/2)<0.0001f,"Strength blends predictably");settings.strength=1;
    }
    settings.quality=1;contact();
    const float bright[]={8,8,8,0.35f};upload();context->ClearRenderTargetView(target.Get(),bright);
    ao.Draw(sceneView.Get(),inputs,settings,true);
    context->CopyResource(staging.Get(),scene.Get());D3D11_MAPPED_SUBRESOURCE brightMap{};
    Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&brightMap)),"Highlight map");
    const auto* brightPixel=reinterpret_cast<const float*>(static_cast<const unsigned char*>(brightMap.pData)+height/2*brightMap.RowPitch)+4*(width/2+1);
    Check(brightPixel[0]==8 && brightPixel[1]==8 && brightPixel[2]==8 && brightPixel[3]==0.35f,"Light protection leaves bright contact pixels unchanged");context->Unmap(staging.Get(),0);
    settings.lightProtection=0;upload();context->ClearRenderTargetView(target.Get(),bright);ao.Draw(sceneView.Get(),inputs,settings,true);
    context->CopyResource(staging.Get(),scene.Get());Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&brightMap)),"Unprotected map");
    brightPixel=reinterpret_cast<const float*>(static_cast<const unsigned char*>(brightMap.pData)+height/2*brightMap.RowPitch)+4*(width/2+1);
    Check(brightPixel[0]<7.9f && brightPixel[0]>=4.79f,"Protection control is effective and occlusion stays bounded");context->Unmap(staging.Get(),0);
    settings.lightProtection=1;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const float ray=inputs.top+(float(y)+0.5f)/float(height)*(inputs.bottom-inputs.top);
        encoded[y*width+x]=encode(ray<0?std::min(300.0f,-80.0f/ray):300.0f);
    }
    upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto corner=read();
    Check(at(corner,width/2,64)<0.94f && at(corner,width/2,35)>0.999f,"Perpendicular wall-floor junction has localized contact shadow");
    contact();upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto nearFive=read();
    inputs.nearPlane=1;contact();upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto nearOne=read();
    float nearDifference=0;for(std::size_t i=0;i<nearOne.size();++i)nearDifference=std::max(nearDifference,std::abs(nearOne[i]-nearFive[i]));
    Check(nearDifference<0.003f,"Near plane does not change world-space occlusion radius");inputs.nearPlane=5;
    contact();settings.radius=15;upload();ao.Draw(sceneView.Get(),inputs,settings,true);
    Check(at(read(),width/2+1,height/2)>0.999f,"Small radius excludes distant occluder");settings.radius=80;
    plane(9000);upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto beyond=read();
    Check(*std::min_element(beyond.begin(),beyond.end())==1,"Distance cutoff leaves distant scene unchanged");
    contact();for(unsigned y=0;y<height;++y)for(unsigned x=width*3/4;x<width;++x)encoded[y*width+x]=1;
    encoded[width*height/2]=std::numeric_limits<float>::quiet_NaN();encoded[width*height/2+1]=-0.2f;
    upload();ao.Draw(sceneView.Get(),inputs,settings,true);const auto sky=read();
    Check(at(sky,width*7/8,height/2)==1 && sky[width*height/2]==1 && sky[width*height/2+1]==1,"Sky and invalid depth remain unchanged");
    contact();upload();auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT sentinel{3,4,19,20,0,1};context->RSSetViewports(1,&sentinel);
    ao.Draw(sceneView.Get(),inputs,settings,true);
    UINT count=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&count,&viewport);
    ComPtr<ID3D11RenderTargetView> restored;context->OMGetRenderTargets(1,&restored,nullptr);
    Check(count==1 && viewport.TopLeftX==3 && viewport.Width==19 && restored.Get()==target.Get(),"Native state restored");
    upload();settings.enabled=false;Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Off" && at(read(),65,48)==1,"Effect switch bypass");
    settings.enabled=true;Check(ao.Draw(sceneView.Get(),inputs,settings,false)=="Off","Master switch bypass");
    settings.strength=0;Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Off","Zero strength bypass");settings.strength=1;
    inputs.left=inputs.right;Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Camera projection unavailable","No guessed FOV on invalid frustum");inputs.left=-1;
    inputs.nearPlane=0;Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Camera projection unavailable","Invalid near plane bypass");inputs.nearPlane=5;
    settings.radius=std::numeric_limits<float>::quiet_NaN();Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Invalid SSAO settings","Non-finite controls rejected");settings.radius=80;
    settings.quality=3;Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Invalid SSAO settings","Invalid quality rejected");settings.quality=1;
    auto validDepth=inputs.depth;inputs.depth.Reset();Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Depth data unavailable","Missing depth bypass");inputs.depth=validDepth;
    create(65,49);contact();settings.quality=0;upload();ao.Draw(sceneView.Get(),inputs,settings,true);
    Check(at(read(),width/2+1,height/2)<0.98f,"Half resolution odd-size resize preserves contact");
    create(1,1);plane(300);upload();ao.Draw(sceneView.Get(),inputs,settings,true);Check(read()[0]==1,"One-pixel input remains neutral");
    create(96,72);inputs.left=-0.65f;inputs.right=0.65f;inputs.top=0.4875f;inputs.bottom=-0.4875f;
    contact();settings.quality=2;upload();ao.Draw(sceneView.Get(),inputs,settings,true);
    Check(at(read(),width/2+1,height/2)<0.95f,"Non-default FOV reconstructs contact");
    inputs.nearPlane=1;contact();upload();ao.Draw(sceneView.Get(),inputs,settings,true);
    Check(at(read(),width/2+1,height/2)<0.95f,"Changed near plane preserves world-space contact");
    D3D11_TEXTURE2D_DESC d{};depth->GetDesc(&d);d.Width=2;
    ComPtr<ID3D11Texture2D> mismatchedDepth;device->CreateTexture2D(&d,nullptr,&mismatchedDepth);inputs.depth.Reset();device->CreateShaderResourceView(mismatchedDepth.Get(),nullptr,&inputs.depth);
    Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Unsupported depth dimensions","Depth dimensions checked");
    inputs.depth.Reset();device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    for(auto format:{DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT}) {
        d.Width=width;d.Format=format;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> native;ComPtr<ID3D11ShaderResourceView> nativeView;ComPtr<ID3D11RenderTargetView> nativeTarget;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&native)),"Native HDR format");
        device->CreateShaderResourceView(native.Get(),nullptr,&nativeView);device->CreateRenderTargetView(native.Get(),nullptr,&nativeTarget);
        const float c[]={2,1,0.5f,0.35f};context->ClearRenderTargetView(nativeTarget.Get(),c);
        Check(ao.Draw(nativeView.Get(),inputs,settings,true)=="HDR SSAO active (Ultra)","Native HDR formats accepted");
    }
    ComPtr<ID3D11Device> otherDevice;ComPtr<ID3D11DeviceContext> otherContext;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&otherDevice,nullptr,&otherContext)),"Other device");
    d.Format=DXGI_FORMAT_R32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> foreignDepth;otherDevice->CreateTexture2D(&d,nullptr,&foreignDepth);
    inputs.depth.Reset();otherDevice->CreateShaderResourceView(foreignDepth.Get(),nullptr,&inputs.depth);
    Check(ao.Draw(sceneView.Get(),inputs,settings,true)=="Depth device mismatch","Foreign depth device rejected");
    SsaoRenderer restarted;Check(!restarted.Initialize(nullptr,error),"Missing device rejected");
    Check(ao.Initialize(device.Get(),error),"Renderer reinitialization releases allocations");
    std::puts("PASS: SSAO contacts, flat/sloping planes, silhouettes, sky, quality, resize, switches, HDR and state.");
}
