#include "renderer.h"
#include <d3d11.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main() {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;std::string error;Check(renderer.Initialize(device.Get(),error),error.c_str());
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        for(auto dims:{std::pair{101u,101u},std::pair{129u,65u},std::pair{65u,129u},std::pair{1u,1u}}) {
            const auto [width,height]=dims;D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=format;d.BindFlags=D3D11_BIND_RENDER_TARGET;
            ComPtr<ID3D11Texture2D> output,staging;ComPtr<ID3D11RenderTargetView> target;
            Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Scene");device->CreateRenderTargetView(output.Get(),nullptr,&target);
            d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Staging");
            std::vector<Pixel> input(width*height);
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)input[y*width+x]={static_cast<unsigned char>(width>1?255*x/(width-1):100),static_cast<unsigned char>(height>1?255*y/(height-1):150),static_cast<unsigned char>((x/4+y/4)%2?220:30),static_cast<unsigned char>(30+(x+17*y)%200)};
            auto upload=[&](const std::vector<Pixel>& image) {
                auto data=image;if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:data)std::swap(p[0],p[2]);
                context->UpdateSubresource(output.Get(),0,nullptr,data.data(),width*4,0);
            };
            auto read=[&]() {
                context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"Map");
                std::vector<Pixel> result(input.size());for(unsigned y=0;y<height;++y)memcpy(result.data()+y*width,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*4);context->Unmap(staging.Get(),0);
                if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:result)std::swap(p[0],p[2]);
                for(std::size_t i=0;i<result.size();++i)Check(result[i][3]==input[i][3],"Native alpha preserved at output coordinates");return result;
            };
            auto draw=[&](const ColorSettings& s,bool enabled=true,const MotionPass& motion=MotionPass{}){upload(input);renderer.Draw(output.Get(),s,enabled,motion);return read();};
            ColorSettings s;Check(draw(s)==input,"Old defaults bypass distortion");s.lensDistortion={true,0};Check(draw(s)==input,"Zero amount exact bypass");
            s.lensDistortion.amount=1;const auto barrel=draw(s);
            s.lensDistortion.amount=-1;const auto pincushion=draw(s);
            const auto center=(height/2)*width+width/2;
            Check(barrel[center]==input[center] && pincushion[center]==input[center],"Optical center remains fixed");
            if(width>1) {
                Check(barrel!=input && pincushion!=input && barrel!=pincushion,"Both signed distortions produce distinct warped images");
                for(unsigned x=1;x<width;++x)Check(barrel[(height/2)*width+x][0]>=barrel[(height/2)*width+x-1][0] && pincushion[(height/2)*width+x][0]>=pincushion[(height/2)*width+x-1][0],"Radial mapping has no folds");
                for(unsigned x=0;x<width;++x)Check(std::abs(int(barrel[(height/2)*width+x][0])+int(barrel[(height/2)*width+width-1-x][0])-255)<=2,"Horizontal symmetry");
                const auto x=width/4;const auto middle=(height/2)*width+x;
                Check(barrel[middle][0]>input[middle][0] && pincushion[middle][0]>input[middle][0],"Crop keeps sample positions inside the frame");
                if(width==height)Check(barrel[middle][0]!=pincushion[middle][0],"Signed lens curves differ away from corners");
            }
            s.lensDistortion.amount=1;Check(draw(s)==barrel,"Stable frame-independent warp");s.lensDistortion.enabled=false;Check(draw(s)==input,"Effect-off bypass");s.lensDistortion.enabled=true;Check(draw(s,false)==input,"Master-off bypass");
            s.lensDistortion.amount=std::numeric_limits<float>::quiet_NaN();Check(draw(s)==input,"Non-finite amount bypass");s.lensDistortion.amount=5;Check(draw(s)==barrel,"Positive range clamps");s.lensDistortion.amount=-5;Check(draw(s)==pincushion,"Negative range clamps");
            auto patterned=input;for(auto& p:input){p[0]=70;p[1]=120;p[2]=170;}
            s.lensDistortion.amount=1;Check(draw(s)==input,"Barrel warp keeps constant image and has no black borders");s.lensDistortion.amount=-1;Check(draw(s)==input,"Pincushion warp keeps constant image and has no black borders");input=patterned;
            s={};s.exposure=0.3f;s.contrast=1.1f;s.liftGammaGain.enabled=true;s.liftGammaGain.gainRed=0.8f;
            const auto graded=draw(s);ColorSettings warp;warp.lensDistortion={true,0.7f};upload(graded);renderer.Draw(output.Get(),warp,true);const auto separate=read();
            s.lensDistortion=warp.lensDistortion;Check(draw(s)==separate,"Distortion follows existing color grading");
            d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Format=DXGI_FORMAT_R32G32_FLOAT;
            ComPtr<ID3D11Texture2D> velocity,depth;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&velocity)),"Velocity");
            MotionPass motion;motion.settings={true,1,12,12};motion.seconds=1.0f/60;motion.inputs.state=MotionState::Ready;device->CreateShaderResourceView(velocity.Get(),nullptr,&motion.inputs.velocity);
            std::vector<float> vectors(width*height*2);for(std::size_t i=0;i<vectors.size();i+=2)vectors[i]=0.08f;
            context->UpdateSubresource(velocity.Get(),0,nullptr,vectors.data(),width*8,0);d.Format=DXGI_FORMAT_R32_FLOAT;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&depth)),"Depth");device->CreateShaderResourceView(depth.Get(),nullptr,&motion.inputs.depth);
            std::vector<float> z(width*height,0.5f);context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),width*4,0);
            s.lensDistortion.enabled=false;const auto moving=draw(s,true,motion);upload(moving);renderer.Draw(output.Get(),warp,true);const auto afterMotion=read();s.lensDistortion.enabled=true;
            Check(draw(s,true,motion)==afterMotion,"Warp follows motion without distorting velocity or depth coordinates");
            auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);D3D11_VIEWPORT sentinel{3,4,12,7,0,1};context->RSSetViewports(1,&sentinel);draw(warp);
            UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);ComPtr<ID3D11RenderTargetView> restoredTarget;context->OMGetRenderTargets(1,&restoredTarget,nullptr);
            Check(count==1 && restored.Width==12 && restored.TopLeftX==3 && restoredTarget.Get()==target.Get(),"Native render state restored");context->ClearState();
        }
    }
    D3D11_TEXTURE2D_DESC rejected{};rejected.Width=64;rejected.Height=32;
    rejected.MipLevels=rejected.ArraySize=rejected.SampleDesc.Count=1;
    rejected.Format=DXGI_FORMAT_R10G10B10A2_UNORM;rejected.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> tenBit;Check(SUCCEEDED(device->CreateTexture2D(&rejected,nullptr,&tenBit)),"10-bit diagnostic fixture");
    ColorSettings active;active.exposure=1;const auto diagnosis=renderer.Draw(tenBit.Get(),active,true);
    Check(diagnosis.find("format=24 (R10G10B10A2_UNORM)")!=std::string::npos && diagnosis.find("size=64x32, samples=1")!=std::string::npos,
        "Unsupported output reports actual format, dimensions and samples");
    std::puts("PASS: signed distortion, crop, center, symmetry, alpha, bounds, bypass, formats, resize and color/motion order.");
}
