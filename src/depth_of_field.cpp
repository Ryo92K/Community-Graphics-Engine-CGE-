#include "depth_of_field.h"
#include "depth_of_field_shader_source.h"
#include <d3dcompiler.h>
#include <algorithm>
namespace cge {
bool DepthOfFieldRenderer::Initialize(ID3D11Device* device,std::string& error) {
    ready_=false;device_=device;
    Ptr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);Ptr<ID3D11Device1> device1;
    if(FAILED(context.As(&context_)) || FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) {
        error="Depth-of-field context isolation unavailable";return false;
    }
    auto level=device->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
    if(FAILED(device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&isolated_))) {
        error="Depth-of-field context state unavailable";return false;
    }
    auto compile=[&](const char* entry,const char* profile,Ptr<ID3DBlob>& blob) {
        Ptr<ID3DBlob> messages;
        if(SUCCEEDED(D3DCompile(CgeDofSource,sizeof(CgeDofSource)-1,"DepthOfField.hlsl",nullptr,nullptr,entry,profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&messages)))return true;
        error=messages?std::string(static_cast<const char*>(messages->GetBufferPointer()),messages->GetBufferSize()):"Fog shader compilation failed";
        return false;
    };
    Ptr<ID3DBlob> vs,ps;
    if(!compile("DofVertex","vs_5_0",vs) || !compile("DofPixel","ps_5_0",ps) ||
        FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_)) ||
        FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_)))return false;
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=32;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants_)) ||
        FAILED(device->CreateRasterizerState(&raster,&raster_))) {
        error="Depth-of-field pipeline allocation failed";return false;
    }
    ready_=true;return true;
}
std::string DepthOfFieldRenderer::Draw(ID3D11ShaderResourceView* scene,const DepthInputs& inputs,const DofSettings& settings,bool enabled) {
    if(!enabled || !settings.enabled || settings.strength<=0) {
        sourceView_.Reset();source_.Reset();width_=height_=0;format_=DXGI_FORMAT_UNKNOWN;return "Off";
    }
    if(!std::isfinite(settings.strength) || !std::isfinite(settings.focusDistance))return "Invalid depth-of-field settings";
    if(inputs.state==DepthState::CameraUnavailable || !ValidDepthProjection(inputs.nearPlane,inputs.farPlane))
        return "Camera data unavailable";
    if(!ready_ || !scene || !inputs.depth || inputs.state!=DepthState::Ready)return "Depth data unavailable";
    D3D11_SHADER_RESOURCE_VIEW_DESC colorView{},depthView{};scene->GetDesc(&colorView);inputs.depth->GetDesc(&depthView);
    if(colorView.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || colorView.Texture2D.MostDetailedMip!=0 ||
        (colorView.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT && colorView.Format!=DXGI_FORMAT_R11G11B10_FLOAT &&
        colorView.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT))return "Unsupported HDR input";
    if(depthView.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || depthView.Texture2D.MostDetailedMip!=0 ||
        (depthView.Format!=DXGI_FORMAT_R32_FLOAT && depthView.Format!=DXGI_FORMAT_R24_UNORM_X8_TYPELESS &&
        depthView.Format!=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS))return "Unsupported depth input";
    Ptr<ID3D11Device> owner;scene->GetDevice(&owner);if(owner.Get()!=device_.Get())return "HDR device mismatch";
    owner.Reset();inputs.depth->GetDevice(&owner);if(owner.Get()!=device_.Get())return "Depth device mismatch";
    Ptr<ID3D11Resource> colorResource,depthResource;scene->GetResource(&colorResource);inputs.depth->GetResource(&depthResource);
    Ptr<ID3D11Texture2D> colorTexture,depthTexture;
    if(FAILED(colorResource.As(&colorTexture)) || FAILED(depthResource.As(&depthTexture)))return "Texture data unavailable";
    D3D11_TEXTURE2D_DESC color{},depth{};colorTexture->GetDesc(&color);depthTexture->GetDesc(&depth);
    if(color.ArraySize!=1 || color.MipLevels!=1 || color.SampleDesc.Count!=1 || !color.Width || !color.Height ||
        !(color.BindFlags&D3D11_BIND_RENDER_TARGET))return "Unsupported HDR target";
    if(depth.Width!=color.Width || depth.Height!=color.Height || depth.ArraySize!=1 ||
        depth.SampleDesc.Count!=1 || colorTexture.Get()==depthTexture.Get())return "Unsupported depth dimensions";
    if(!source_ || width_!=color.Width || height_!=color.Height || format_!=colorView.Format) {
        sourceView_.Reset();source_.Reset();width_=height_=0;format_=DXGI_FORMAT_UNKNOWN;
        auto d=color;d.Format=colorView.Format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags=d.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&source_)) ||
            FAILED(device_->CreateShaderResourceView(source_.Get(),nullptr,&sourceView_)))return "Depth-of-field allocation failed";
        width_=color.Width;height_=color.Height;format_=colorView.Format;
    }
    D3D11_RENDER_TARGET_VIEW_DESC view{};view.Format=colorView.Format;view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    Ptr<ID3D11RenderTargetView> output;
    if(FAILED(device_->CreateRenderTargetView(colorTexture.Get(),&view,&output)))return "HDR output unavailable";
    struct Restore {
        ID3D11DeviceContext1* context;Ptr<ID3DDeviceContextState> previous;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* state):context(c){c->SwapDeviceContextState(state,&previous);}
        ~Restore(){context->ClearState();if(previous)context->SwapDeviceContextState(previous.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();context_->CopyResource(source_.Get(),colorTexture.Get());
    auto* target=output.Get();context_->OMSetRenderTargets(1,&target,nullptr);
    const D3D11_VIEWPORT viewport{0,0,float(color.Width),float(color.Height),0,1};context_->RSSetViewports(1,&viewport);
    struct Controls {float nearPlane,farPlane,focusDistance,strength,padding[4];};
    const Controls controls{inputs.nearPlane,inputs.farPlane,std::clamp(settings.focusDistance,1.0f,50000.0f),
        std::clamp(settings.strength,0.0f,1.0f),{}};
    static_assert(sizeof(Controls)==32);
    context_->UpdateSubresource(constants_.Get(),0,nullptr,&controls,0,0);
    auto* buffer=constants_.Get();context_->PSSetConstantBuffers(0,1,&buffer);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->RSSetState(raster_.Get());context_->VSSetShader(vertex_.Get(),nullptr,0);context_->PSSetShader(pixel_.Get(),nullptr,0);
    ID3D11ShaderResourceView* views[]={sourceView_.Get(),inputs.depth.Get()};context_->PSSetShaderResources(0,2,views);
    context_->Draw(3,0);
    return "HDR depth of field active";
}
}
