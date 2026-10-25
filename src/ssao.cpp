#include "ssao.h"
#include "ssao_shader_source.h"
#include <d3dcompiler.h>
#include <algorithm>
namespace cge {
bool SsaoRenderer::Initialize(ID3D11Device* device,std::string& error) {
    *this=SsaoRenderer{};
    if(!device){error="SSAO device unavailable";return false;}
    device_=device;
    Ptr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);Ptr<ID3D11Device1> device1;
    if(FAILED(context.As(&context_)) || FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) {
        error="SSAO context isolation unavailable";return false;
    }
    auto level=device->GetFeatureLevel();D3D_FEATURE_LEVEL chosen{};
    if(FAILED(device1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&isolated_))) {
        error="SSAO context state unavailable";return false;
    }
    auto compile=[&](const char* entry,const char* profile,Ptr<ID3DBlob>& blob) {
        Ptr<ID3DBlob> messages;
        if(SUCCEEDED(D3DCompile(CgeSsaoSource,sizeof(CgeSsaoSource)-1,"SSAO.hlsl",nullptr,nullptr,entry,profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&messages)))return true;
        error=messages?std::string(static_cast<const char*>(messages->GetBufferPointer()),messages->GetBufferSize()):"SSAO shader compilation failed";
        return false;
    };
    Ptr<ID3DBlob> vs,ps;
    if(!compile("SsaoVertex","vs_5_0",vs) || !compile("GeometryPixel","ps_5_0",ps) ||
        FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_)) ||
        FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&geometryPixel_)))return false;
    const char* entries[]={"OcclusionPixel","FilterPixel","CompositePixel"};
    Ptr<ID3D11PixelShader>* shaders[]={&aoPixel_,&filterPixel_,&compositePixel_};
    for(unsigned i=0;i<3;++i) {
        ps.Reset();if(!compile(entries[i],"ps_5_0",ps) || FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,shaders[i]->GetAddressOf())))return false;
    }
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=80;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants_)) ||
        FAILED(device->CreateRasterizerState(&raster,&raster_))) {
        error="SSAO pipeline allocation failed";return false;
    }
    ready_=true;return true;
}
void SsaoRenderer::ReleaseTargets() noexcept {
    output_.Reset();
    sourceView_.Reset();source_.Reset();geometry_=Surface{};ao_=Surface{};ping_=Surface{};
    width_=height_=aoWidth_=aoHeight_=0;format_=DXGI_FORMAT_UNKNOWN;
}
bool SsaoRenderer::MakeSurface(Surface& surface,unsigned w,unsigned h,DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&surface.texture)) &&
        SUCCEEDED(device_->CreateShaderResourceView(surface.texture.Get(),nullptr,&surface.view)) &&
        SUCCEEDED(device_->CreateRenderTargetView(surface.texture.Get(),nullptr,&surface.target));
}
std::string SsaoRenderer::Draw(ID3D11ShaderResourceView* scene,const DepthInputs& inputs,const SsaoSettings& settings,bool enabled) {
    if(!enabled || !settings.enabled || settings.strength<=0){ReleaseTargets();return "Off";}
    if(!std::isfinite(settings.strength) || !std::isfinite(settings.radius) || !std::isfinite(settings.bias) ||
        !std::isfinite(settings.lightProtection) || !std::isfinite(settings.power) || !std::isfinite(settings.maxDistance) || settings.quality<0 || settings.quality>2)
        return "Invalid SSAO settings";
    if(inputs.state==DepthState::CameraUnavailable || !ValidDepthProjection(inputs.nearPlane,inputs.farPlane) || !inputs.ValidFrustum())
        return "Camera projection unavailable";
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
    const unsigned aoWidth=settings.quality==0?(color.Width+1)/2:color.Width;
    const unsigned aoHeight=settings.quality==0?(color.Height+1)/2:color.Height;
    if(!source_ || width_!=color.Width || height_!=color.Height || format_!=colorView.Format || aoWidth_!=aoWidth || aoHeight_!=aoHeight) {
        ReleaseTargets();
        auto d=color;d.Format=colorView.Format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags=d.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&source_)) ||
            FAILED(device_->CreateShaderResourceView(source_.Get(),nullptr,&sourceView_)) ||
            !MakeSurface(geometry_,color.Width,color.Height,DXGI_FORMAT_R32G32B32A32_FLOAT) ||
            !MakeSurface(ao_,aoWidth,aoHeight,DXGI_FORMAT_R16_FLOAT) ||
            !MakeSurface(ping_,aoWidth,aoHeight,DXGI_FORMAT_R16_FLOAT)) {
            ReleaseTargets();return "SSAO allocation failed";
        }
        width_=color.Width;height_=color.Height;aoWidth_=aoWidth;aoHeight_=aoHeight;format_=colorView.Format;
    }
    auto* output=output_.Get(device_.Get(),colorTexture.Get(),colorView.Format);
    if(!output)return "HDR output unavailable";
    struct Restore {
        ID3D11DeviceContext1* context;Ptr<ID3DDeviceContextState> previous;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* state):context(c){c->SwapDeviceContextState(state,&previous);}
        ~Restore(){context->ClearState();if(previous)context->SwapDeviceContextState(previous.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();context_->CopyResource(source_.Get(),colorTexture.Get());
    struct Controls {
        float nearPlane,farPlane,strength,radius,bias,power,maxDistance,padding;
        float rays[4];unsigned sceneSize[2],aoSize[2],directions,steps;int axis[2];
    };
    static_assert(sizeof(Controls)==80);
    Controls controls{inputs.nearPlane,inputs.farPlane,std::clamp(settings.strength,0.0f,1.0f),
        std::clamp(settings.radius,1.0f,250.0f),std::clamp(settings.bias,0.01f,0.3f),std::clamp(settings.power,1.0f,4.0f),
        std::clamp(settings.maxDistance,500.0f,50000.0f),std::clamp(settings.lightProtection,0.0f,1.0f),
        {inputs.left,inputs.top,inputs.right-inputs.left,inputs.bottom-inputs.top},
        {width_,height_},{aoWidth_,aoHeight_},settings.quality==2?16u:settings.quality==1?12u:8u,
        settings.quality==2?8u:settings.quality==1?6u:4u,{0,0}};
    auto* buffer=constants_.Get();context_->PSSetConstantBuffers(0,1,&buffer);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->RSSetState(raster_.Get());context_->VSSetShader(vertex_.Get(),nullptr,0);
    int uploadedX=-1,uploadedY=-1;
    auto pass=[&](ID3D11PixelShader* shader,ID3D11RenderTargetView* target,unsigned w,unsigned h,ID3D11ShaderResourceView* guide,ID3D11ShaderResourceView* ao) {
        ID3D11ShaderResourceView* clear[4]{};context_->PSSetShaderResources(0,4,clear);context_->OMSetRenderTargets(0,nullptr,nullptr);
        context_->OMSetRenderTargets(1,&target,nullptr);
        const D3D11_VIEWPORT viewport{0,0,float(w),float(h),0,1};context_->RSSetViewports(1,&viewport);
        if(uploadedX!=controls.axis[0] || uploadedY!=controls.axis[1]) {
            context_->UpdateSubresource(constants_.Get(),0,nullptr,&controls,0,0);
            uploadedX=controls.axis[0];uploadedY=controls.axis[1];
        }
        context_->PSSetShader(shader,nullptr,0);
        ID3D11ShaderResourceView* views[]={sourceView_.Get(),inputs.depth.Get(),guide,ao};context_->PSSetShaderResources(0,4,views);
        context_->Draw(3,0);
    };
    pass(geometryPixel_.Get(),geometry_.target.Get(),width_,height_,nullptr,nullptr);
    pass(aoPixel_.Get(),ao_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),nullptr);
    controls.axis[0]=1;pass(filterPixel_.Get(),ping_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),ao_.view.Get());
    controls.axis[0]=0;controls.axis[1]=1;pass(filterPixel_.Get(),ao_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),ping_.view.Get());
    pass(compositePixel_.Get(),output,width_,height_,geometry_.view.Get(),ao_.view.Get());
    return settings.quality==0?"HDR SSAO active (Balanced)":settings.quality==1?"HDR SSAO active (High)":"HDR SSAO active (Ultra)";
}
}
