#include "renderer.h"
#include "motion_blur.h"
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <cstdint>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main() {
    Check(std::abs(MotionExposure(0.5f,1.0f/60)-0.5f)<0.0001f,"60 FPS exposure");
    Check(std::abs(MotionExposure(0.5f,1.0f/120)-1)<0.0001f,"120 FPS exposure normalization");
    Check(MotionExposure(1,1)==0 && MotionExposure(1,0)==0 &&
        MotionExposure(std::numeric_limits<float>::quiet_NaN(),0.016f)==0,"Invalid frame time and strength bypass motion");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),"Renderer initialization");
    D3D11_TEXTURE2D_DESC d{};d.Width=32;d.Height=16;d.ArraySize=d.MipLevels=1;d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
    ComPtr<ID3D11RenderTargetView> rtv;device->CreateRenderTargetView(output.Get(),nullptr,&rtv);
    auto* rt=rtv.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);
    d.Format=DXGI_FORMAT_R32G32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> velocity;device->CreateTexture2D(&d,nullptr,&velocity);
    MotionPass motion;motion.settings.enabled=true;motion.settings.strength=0.5f;motion.settings.radius=2;motion.settings.samples=12;
    motion.inputs.state=MotionState::Ready;motion.seconds=1.0f/60;
    device->CreateShaderResourceView(velocity.Get(),nullptr,&motion.inputs.velocity);
    d.Format=DXGI_FORMAT_R32_FLOAT;ComPtr<ID3D11Texture2D> depth;device->CreateTexture2D(&d,nullptr,&depth);
    device->CreateShaderResourceView(depth.Get(),nullptr,&motion.inputs.depth);
    std::vector<float> vectors(32*16*2),z(32*16,0.5f);
    std::vector<unsigned char> image(32*16*4);
    auto stripe=[&]{
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x) {
            auto at=(y*32+x)*4;image[at]=image[at+1]=image[at+2]=x==16?255:0;image[at+3]=77;
        }
        context->UpdateSubresource(output.Get(),0,nullptr,image.data(),32*4,0);
    };
    auto upload=[&]{context->UpdateSubresource(velocity.Get(),0,nullptr,vectors.data(),32*8,0);context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),32*4,0);};
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;device->CreateTexture2D(&d,nullptr,&readback);
    auto pixel=[&](unsigned x,unsigned y=8) {
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"Readback");
        auto* p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;
        unsigned result=p[0];Check(p[3]==77,"Motion preserves source alpha");
        context->Unmap(readback.Get(),0);return result;
    };
    ColorSettings color;MotionState state{};
    stripe();upload();renderer.Draw(output.Get(),color,true,motion,&state);
    Check(state==MotionState::Ready && pixel(16)==255 && pixel(15)==0,"Stationary image remains sharp");
    const auto stationaryReport=renderer.TakeMotionDiagnostics();
    Check(stationaryReport.find("mean=0.00")!=std::string::npos,"Asynchronous diagnostics identify stationary vectors");
    for(unsigned i=0;i<32*16;++i)vectors[i*2]=0.25f;
    motion.settings.strength=1;
    stripe();upload();renderer.Draw(output.Get(),color,true,motion,&state);
    Check(pixel(16)>10 && pixel(16)<240 && pixel(15)>0 && pixel(10)==0,"Moving image blurs in velocity direction with a radius limit");
    const auto movingReport=renderer.TakeMotionDiagnostics();
    Check(movingReport.find("mean=8.00")!=std::string::npos && movingReport.find("mean span=4.00")!=std::string::npos,
        "Diagnostics measure native pixel motion and the radius-clamped blur span");
    const auto narrowPeak=pixel(16);
    motion.settings.strength=1.5f;motion.settings.radius=48;
    stripe();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)<narrowPeak && pixel(11)>0,"Strong settings produce a wider, visibly stronger blur");
    motion.settings.strength=1;motion.settings.radius=2;
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x)z[y*32+x]=0.1f+0.025f*float(x);
    stripe();upload();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)<240 && pixel(15)>0,"Sloping surfaces blur without rejecting same-plane samples");
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x)z[y*32+x]=x==16?0.1f:0.9f;
    stripe();upload();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)==255 && pixel(15)==0,"Depth rejection prevents foreground/background bleeding");
    for(auto& value:z)value=0.5f;
    for(unsigned i=0;i<32*16;++i)vectors[i*2]=1;
    stripe();upload();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)==255 && pixel(15)==0,"Camera-cut sized vectors are rejected");
    for(unsigned i=0;i<32*16;++i)vectors[i*2]=0.25f;
    motion.settings.strength=10;motion.settings.radius=48;stripe();upload();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)<240,"Intensity ten produces visible blur");motion.settings.strength=1;motion.settings.radius=2;
    motion.settings.enabled=false;stripe();upload();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)==255,"Effect toggle bypasses motion");
    motion.settings.enabled=true;stripe();renderer.Draw(output.Get(),color,false,motion);
    Check(pixel(16)==255,"Master switch bypasses motion");
    motion.inputs.depth.Reset();stripe();renderer.Draw(output.Get(),color,true,motion,&state);
    Check(state==MotionState::Unsupported && pixel(16)==255,"Missing depth is safely bypassed");
    device->CreateShaderResourceView(depth.Get(),nullptr,&motion.inputs.depth);
    color.exposure=-1;stripe();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)>5 && pixel(16)<120,"Motion follows color grading without overriding exposure");
    D3D11_TEXTURE2D_DESC halfDesc{};halfDesc.Width=32;halfDesc.Height=16;
    halfDesc.MipLevels=halfDesc.ArraySize=1;halfDesc.SampleDesc.Count=1;
    halfDesc.Format=DXGI_FORMAT_R16G16_FLOAT;halfDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::vector<std::uint16_t> halfData(32*16*2);
    for(unsigned i=0;i<32*16;++i)halfData[i*2]=0x3400;                           
    D3D11_SUBRESOURCE_DATA halfInitial{halfData.data(),32*4,0};
    ComPtr<ID3D11Texture2D> halfVelocity;
    Check(SUCCEEDED(device->CreateTexture2D(&halfDesc,&halfInitial,&halfVelocity)),"Native half-float velocity");
    motion.inputs.velocity.Reset();device->CreateShaderResourceView(halfVelocity.Get(),nullptr,&motion.inputs.velocity);
    motion.settings.radius=3;color=ColorSettings{};
    stripe();renderer.Draw(output.Get(),color,true,motion);
    Check(pixel(16)<255,"Half-float vectors drive the motion shader");
    Check(pixel(16)<255,"Repeated velocity sampling stays active");
    const auto halfReport=renderer.TakeMotionDiagnostics();
    Check(halfReport.find("mean=8.00")!=std::string::npos && halfReport.find("mean span=6.00")!=std::string::npos,
        "Diagnostics decode native half-float motion accurately");
    D3D11_BOX hud{0,0,0,1,1,1};const unsigned char marker[]={17,231,93,77};
    context->UpdateSubresource(output.Get(),0,&hud,marker,4,0);
    renderer.Draw(output.Get(),color,false);
    Check(pixel(0,0)==17,"Overlay-only pass never blurs the HUD");
    UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
    Check(restored.TopLeftX==3 && restored.Width==12,"Game viewport is restored");
    std::puts("PASS: native velocity blur, stationary bypass, depth edges, cuts, exposure, HUD, state and persistence inputs.");
}
