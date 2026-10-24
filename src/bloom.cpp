#include "bloom.h"
#include "bloom_shader_source.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
namespace cge {
bool BloomRenderer::CreateSurface(Surface& result,unsigned width,unsigned height,DXGI_FORMAT format,bool target) {
    result={};result.width=width;result.height=height;
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;
    d.SampleDesc.Count=1;d.Format=format;d.Usage=D3D11_USAGE_DEFAULT;
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(target?D3D11_BIND_RENDER_TARGET:0);
    return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&result.texture)) &&
        SUCCEEDED(device_->CreateShaderResourceView(result.texture.Get(),nullptr,&result.view)) &&
        (!target || SUCCEEDED(device_->CreateRenderTargetView(result.texture.Get(),nullptr,&result.target)));
}
bool BloomRenderer::Resize(unsigned width,unsigned height,DXGI_FORMAT format) {
    source_={};down_={};up_={};sourceFormat_=DXGI_FORMAT_UNKNOWN;
    if(!CreateSurface(source_,width,height,format,false))return false;
    for(unsigned i=0;i<down_.size();++i) {
        width=std::max(1u,(width+1)/2);height=std::max(1u,(height+1)/2);
        if(!CreateSurface(down_[i],width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,true) ||
            !CreateSurface(up_[i],width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,true))return false;
    }
    sourceFormat_=format;return true;
}
bool BloomRenderer::Initialize(ID3D11Device* device,std::string& error) {
    ready_=false;device_=device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    Microsoft::WRL::ComPtr<ID3D11Device1> device1;
    if(FAILED(context.As(&context_)) || FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) {
        error="HDR context isolation unavailable";return false;
    }
    auto level=device->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
    if(FAILED(device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&isolated_))) {
        error="HDR context state unavailable";return false;
    }
    auto compile=[&](const char* entry,const char* profile,Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
        Microsoft::WRL::ComPtr<ID3DBlob> messages;
        if(SUCCEEDED(D3DCompile(CgeBloomSource,sizeof(CgeBloomSource)-1,"Bloom.hlsl",nullptr,nullptr,entry,profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&messages)))return true;
        error=messages?std::string(static_cast<const char*>(messages->GetBufferPointer()),messages->GetBufferSize()):"Bloom shader compilation failed";
        return false;
    };
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    if(!compile("BloomVertex","vs_5_0",blob) ||
        FAILED(device->CreateVertexShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&vertex_)))return false;
    const char* entries[]={"BrightPixel","DownPixel","UpPixel","CompositePixel"};
    Ptr<ID3D11PixelShader>* shaders[]={&bright_,&downsample_,&upsample_,&composite_};
    for(unsigned i=0;i<4;++i) {
        blob.Reset();
        if(!compile(entries[i],"ps_5_0",blob) ||
            FAILED(device->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,shaders[i]->GetAddressOf())))return false;
    }
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
    D3D11_BUFFER_DESC constants{};constants.ByteWidth=32;constants.Usage=D3D11_USAGE_DEFAULT;constants.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    if(FAILED(device->CreateSamplerState(&sampler,&sampler_)) ||
        FAILED(device->CreateBuffer(&constants,nullptr,&constants_)) ||
        FAILED(device->CreateRasterizerState(&raster,&raster_))) {
        error="Bloom pipeline allocation failed";return false;
    }
    ready_=true;return true;
}
std::string BloomRenderer::Draw(ID3D11ShaderResourceView* input,const BloomSettings& settings,bool enabled) {
    if(!enabled || !settings.enabled || settings.intensity<=0) {
        source_={};down_={};up_={};sourceFormat_=DXGI_FORMAT_UNKNOWN;return "Off";
    }
    if(!std::isfinite(settings.intensity)||!std::isfinite(settings.threshold)||!std::isfinite(settings.radius))return "Invalid settings";
    if(!ready_ || !input || !context_ || !isolated_)return "HDR input unavailable";
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};input->GetDesc(&view);
    if(view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || view.Texture2D.MostDetailedMip!=0 ||
        (view.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT && view.Format!=DXGI_FORMAT_R11G11B10_FLOAT &&
         view.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT))return "Unsupported HDR input";
    Ptr<ID3D11Device> owner;input->GetDevice(&owner);if(owner.Get()!=device_.Get())return "HDR device mismatch";
    Ptr<ID3D11Resource> resource;input->GetResource(&resource);Ptr<ID3D11Texture2D> texture;
    if(FAILED(resource.As(&texture)))return "HDR texture unavailable";
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    if(desc.ArraySize!=1 || desc.MipLevels!=1 || desc.SampleDesc.Count!=1 || !desc.Width || !desc.Height ||
        !(desc.BindFlags&D3D11_BIND_RENDER_TARGET))return "Unsupported HDR target";
                                                                            
    if(!source_.texture || source_.width!=desc.Width || source_.height!=desc.Height || sourceFormat_!=view.Format) {
        if(!Resize(desc.Width,desc.Height,view.Format))return "Bloom allocation failed";
    }
    D3D11_RENDER_TARGET_VIEW_DESC output{};output.Format=view.Format;output.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    Ptr<ID3D11RenderTargetView> target;
    if(FAILED(device_->CreateRenderTargetView(texture.Get(),&output,&target)))return "HDR output unavailable";
    struct Restore {
        ID3D11DeviceContext1* context;Ptr<ID3DDeviceContextState> previous;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* state):context(c){c->SwapDeviceContextState(state,&previous);}
        ~Restore(){context->ClearState();if(previous)context->SwapDeviceContextState(previous.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();context_->CopyResource(source_.texture.Get(),texture.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertex_.Get(),nullptr,0);context_->RSSetState(raster_.Get());
    auto* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
    auto* constants=constants_.Get();context_->PSSetConstantBuffers(0,1,&constants);
    auto pass=[&](ID3D11RenderTargetView* destination,unsigned width,unsigned height,
        ID3D11PixelShader* shader,ID3D11ShaderResourceView* a,ID3D11ShaderResourceView* b,float blend) {
        ID3D11ShaderResourceView* empty[2]{};context_->PSSetShaderResources(0,2,empty);
        context_->OMSetRenderTargets(1,&destination,nullptr);
        const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context_->RSSetViewports(1,&viewport);
        struct Controls {float size[2],threshold,intensity,blend,padding[3];};
        const Controls controls{{float(width),float(height)},std::clamp(settings.threshold,0.0f,1.0f),
            std::clamp(settings.intensity,0.0f,2.0f),blend,{}};
        static_assert(sizeof(Controls)==32);
        context_->UpdateSubresource(constants_.Get(),0,nullptr,&controls,0,0);
        ID3D11ShaderResourceView* inputs[]={a,b};context_->PSSetShaderResources(0,2,inputs);
        context_->PSSetShader(shader,nullptr,0);context_->Draw(3,0);
    };
    pass(down_[0].target.Get(),down_[0].width,down_[0].height,bright_.Get(),source_.view.Get(),nullptr,0);
    const float spread=std::clamp(std::log2(std::clamp(settings.radius,1.0f,64.0f))*(5.0f/6.0f),0.0f,5.0f);
    const unsigned levels=std::min(6u,1u+static_cast<unsigned>(std::ceil(spread)));
    for(unsigned i=1;i<levels;++i)
        pass(down_[i].target.Get(),down_[i].width,down_[i].height,downsample_.Get(),down_[i-1].view.Get(),nullptr,0);
    auto* blurred=down_[levels-1].view.Get();
    for(unsigned i=levels-1;i>0;--i) {
        const float weight=0.5f*std::clamp(spread-float(i-1),0.0f,1.0f);
        pass(up_[i-1].target.Get(),up_[i-1].width,up_[i-1].height,upsample_.Get(),blurred,down_[i-1].view.Get(),weight);
        blurred=up_[i-1].view.Get();
    }
    pass(target.Get(),desc.Width,desc.Height,composite_.Get(),blurred,source_.view.Get(),0);
    return "HDR bloom active";
}
}
