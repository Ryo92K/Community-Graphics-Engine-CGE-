#include "renderer.h"
#include "shader_source.h"
#include <d3dcompiler.h>
namespace cge {
bool Renderer::Initialize(ID3D11Device* device,std::string& error) {
    device_=device;ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    ComPtr<ID3D11Device1> device1;
    if(FAILED(context.As(&context_)) || FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) {
        error="Direct3D 11.1 context isolation unavailable";return false;
    }
    auto level=device->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
    if(FAILED(device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&isolated_)))return false;
    auto compile=[&](const char* entry,const char* target,ComPtr<ID3DBlob>& blob) {
        ComPtr<ID3DBlob> errors;
        const auto result=D3DCompile(CgeShaderSource,sizeof(CgeShaderSource)-1,"CGE.hlsl",nullptr,nullptr,entry,target,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
        if(FAILED(result))error=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"CGE shader compilation failed";
        return SUCCEEDED(result);
    };
    ComPtr<ID3DBlob> vs,ps;
    if(!compile("FullscreenVertex","vs_5_0",vs) || !compile("ColorPixel","ps_5_0",ps))return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_)) ||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_)))return false;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=32;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&constants_)))return false;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    return SUCCEEDED(device->CreateRasterizerState(&raster,&raster_));
}
std::string Renderer::Draw(ID3D11Texture2D* buffer,const Settings& settings,const std::function<void()>& menu) {
    const bool process=settings.enabled && !settings.Neutral();
    if(!process && !menu)return "idle";
    if(!context_ || !isolated_)return "renderer unavailable";
    D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);
    if(desc.SampleDesc.Count!=1 || desc.ArraySize!=1 || desc.MipLevels!=1 ||
        (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM))
        return "unsupported output; SDR UNORM single-sample required";
    ComPtr<ID3D11RenderTargetView> target;
    if(FAILED(device_->CreateRenderTargetView(buffer,nullptr,&target)))return "output view unavailable";
    if(process && (!source_ || width_!=desc.Width || height_!=desc.Height || format_!=desc.Format)) {
        sourceView_.Reset();source_.Reset();width_=height_=0;
        auto sourceDesc=desc;sourceDesc.Usage=D3D11_USAGE_DEFAULT;sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        sourceDesc.CPUAccessFlags=sourceDesc.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&sourceDesc,nullptr,&source_)) ||
            FAILED(device_->CreateShaderResourceView(source_.Get(),nullptr,&sourceView_)))return "input copy unavailable";
        width_=desc.Width;height_=desc.Height;format_=desc.Format;
    }
    struct Restore {
        ID3D11DeviceContext1* context;ComPtr<ID3DDeviceContextState> old;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* next):context(c){context->SwapDeviceContextState(next,&old);}
        ~Restore(){context->ClearState();if(old)context->SwapDeviceContextState(old.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();
    if(process)context_->CopyResource(source_.Get(),buffer);
    auto* output=target.Get();context_->OMSetRenderTargets(1,&output,nullptr);
    D3D11_VIEWPORT viewport={0,0,float(desc.Width),float(desc.Height),0,1};context_->RSSetViewports(1,&viewport);
    if(process) {
        const float parameters[]={settings.exposure,settings.contrast,settings.saturation,settings.sharpen,settings.gamma,settings.temperature*0.01f,0,0};
        context_->UpdateSubresource(constants_.Get(),0,nullptr,parameters,0,0);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->RSSetState(raster_.Get());context_->VSSetShader(vertex_.Get(),nullptr,0);context_->PSSetShader(pixel_.Get(),nullptr,0);
        auto* source=sourceView_.Get();context_->PSSetShaderResources(0,1,&source);
        auto* constants=constants_.Get();context_->PSSetConstantBuffers(0,1,&constants);
        context_->Draw(3,0);
    }
    if(menu)menu();
    return process?"native color pass active":"menu only";
}
}
