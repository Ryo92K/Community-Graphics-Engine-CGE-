#include "renderer.h"
#include "bloom.h"
#include "fog.h"
#include "depth_of_field.h"
#include "ssao.h"
#include <d3d11.h>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>
using namespace cge;
void Check(bool value,const char* text){if(!value){std::printf("FAIL: %s\n",text);std::exit(1);}}
int wmain(int argc,wchar_t** argv) {
    Check(argc==3,"Capture directory and LUT directory");
    const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;BloomRenderer bloom;FogRenderer fog;DepthOfFieldRenderer dof;SsaoRenderer ao;std::string error;
    Check(renderer.Initialize(device.Get(),error) && bloom.Initialize(device.Get(),error) && fog.Initialize(device.Get(),error) && dof.Initialize(device.Get(),error) && ao.Initialize(device.Get(),error),error.c_str());
    renderer.SetLutDirectory(argv[2]);
    constexpr unsigned width=96,height=72;
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> sdr,hdr,readSdr,readHdr,depth,velocity;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&sdr)),"SDR target");
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readSdr)),"SDR readback");
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readHdr)),"HDR readback");
    d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;
    Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&hdr)),"HDR target");
    ComPtr<ID3D11ShaderResourceView> hdrView;device->CreateShaderResourceView(hdr.Get(),nullptr,&hdrView);
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Format=DXGI_FORMAT_R32_FLOAT;device->CreateTexture2D(&d,nullptr,&depth);
    d.Format=DXGI_FORMAT_R32G32_FLOAT;device->CreateTexture2D(&d,nullptr,&velocity);
    DepthInputs inputs;inputs.state=DepthState::Ready;inputs.nearPlane=5;inputs.farPlane=20000;
    inputs.left=-1;inputs.right=1;inputs.top=0.75f;inputs.bottom=-0.75f;
    device->CreateShaderResourceView(depth.Get(),nullptr,&inputs.depth);
    MotionPass motion;motion.inputs.depth=inputs.depth;motion.inputs.state=MotionState::Ready;
    device->CreateShaderResourceView(velocity.Get(),nullptr,&motion.inputs.velocity);
    std::vector<float> z(width*height),vectors(width*height*2),colors(width*height*4);
    std::vector<unsigned char> pixels(width*height*4);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const auto p=y*width+x;const float distance=x<width/2?275.0f:300.0f;
        z[p]=20000*(distance-5)/(distance*19995);vectors[p*2]=0.035f;
        pixels[p*4]=static_cast<unsigned char>((x*23+y*3)%256);pixels[p*4+1]=static_cast<unsigned char>((y*17+x*3)%256);
        pixels[p*4+2]=static_cast<unsigned char>((x*11+y*7)%256);pixels[p*4+3]=77;
        for(unsigned c=0;c<3;++c)colors[p*4+c]=float(pixels[p*4+c])/100;
        colors[p*4+3]=0.35f;
    }
    context->UpdateSubresource(depth.Get(),0,nullptr,z.data(),width*4,0);context->UpdateSubresource(velocity.Get(),0,nullptr,vectors.data(),width*8,0);
    auto capture=[&](ID3D11Texture2D* texture,ID3D11Texture2D* readback,unsigned stride,const std::string& name) {
        context->CopyResource(readback,texture);D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&m)),"Capture map");
        std::ofstream file(directory/name,std::ios::binary);
        for(unsigned y=0;y<height;++y)file.write(static_cast<const char*>(m.pData)+y*m.RowPitch,width*stride);
        context->Unmap(readback,0);Check(bool(file),"Write capture");
    };
    for(unsigned mode=0;mode<8;++mode) {
        ColorSettings settings;
        if(mode&1) {
            settings.exposure=-0.2f;settings.contrast=1.2f;settings.saturation=0.8f;settings.sharpen=0.1f;
            settings.clarity={true,0.25f};settings.vibrance={true,0.2f};settings.debanding={true,0.5f,4,0.05f};
            settings.vignette.enabled=true;settings.filmGrain.enabled=true;settings.chromaticAberration.enabled=true;
            settings.splitToning.enabled=true;settings.liftGammaGain.enabled=true;settings.liftGammaGain.liftBlue=0.02f;
            settings.lut={true,0.3f,"Warm.cube"};
        }
        motion.settings.enabled=(mode&2)!=0;motion.settings.strength=1;motion.seconds=1.0f/60;
        settings.lensDistortion={bool(mode&4),0.3f};
        for(unsigned frame=0;frame<2;++frame) {
            context->UpdateSubresource(sdr.Get(),0,nullptr,pixels.data(),width*4,0);
            renderer.Draw(sdr.Get(),settings,true,motion,nullptr,11);
            capture(sdr.Get(),readSdr.Get(),4,"sdr-"+std::to_string(mode)+"-"+std::to_string(frame)+".bin");
        }
    }
    for(unsigned mode=0;mode<8;++mode) {
        for(unsigned frame=0;frame<2;++frame) {
            context->UpdateSubresource(hdr.Get(),0,nullptr,colors.data(),width*16,0);
            SsaoSettings ssao{true,0.65f,60,0.05f,1.5f,8000,int(mode%3)};
            if(mode&1)ao.Draw(hdrView.Get(),inputs,ssao,true);
            if(mode&2)fog.Draw(hdrView.Get(),inputs,FogSettings{true,0.4f,10},true);
            if(mode&4)dof.Draw(hdrView.Get(),inputs,DofSettings{true,100,0.6f},true);
            bloom.Draw(hdrView.Get(),BloomSettings{true,0.2f,0.6f,32},true,LensDirtSettings{bool(mode&1),0.5f,0.25f});
            capture(hdr.Get(),readHdr.Get(),16,"hdr-"+std::to_string(mode)+"-"+std::to_string(frame)+".bin");
        }
    }
    for(unsigned mode=0;mode<8;++mode)for(const char* prefix:{"sdr-","hdr-"}) {
        const auto stem=std::string(prefix)+std::to_string(mode);
        std::ifstream first(directory/(stem+"-0.bin"),std::ios::binary),second(directory/(stem+"-1.bin"),std::ios::binary);
        const std::vector<char> a((std::istreambuf_iterator<char>(first)),{}),b((std::istreambuf_iterator<char>(second)),{});
        Check(!a.empty() && a==b,"Repeated frames remain pixel-identical with cached resources");
    }
    std::puts("PASS: deterministic SDR combinations and HDR chain captures.");
}
