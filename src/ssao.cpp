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
    const char* entries[]={"OcclusionPixel","OcclusionPixelReduced","FilterPixel","AmbientPixel","FilterAmbientPixel"};
    Ptr<ID3D11PixelShader>* shaders[]={&aoPixel_,&aoReducedPixel_,&filterPixel_,&ambientPixel_,&filterAmbientPixel_};
    for(unsigned i=0;i<5;++i) {
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
    geometry_=Surface{};linearDepth_=Surface{};guide_=Surface{};ao_=Surface{};ping_=Surface{};ambient_=Surface{};
    width_=height_=aoWidth_=aoHeight_=0;
}
bool SsaoRenderer::MakeSurface(Surface& surface,unsigned w,unsigned h,DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&surface.texture)) &&
        SUCCEEDED(device_->CreateShaderResourceView(surface.texture.Get(),nullptr,&surface.view)) &&
        SUCCEEDED(device_->CreateRenderTargetView(surface.texture.Get(),nullptr,&surface.target));
}
std::string SsaoRenderer::PrepareOcclusion(const DepthInputs& inputs,const SsaoSettings& settings,bool enabled) {
    if(!enabled || !settings.enabled || settings.strength<=0){ReleaseTargets();return "Off";}
    if(!std::isfinite(settings.strength) || !std::isfinite(settings.radius) || !std::isfinite(settings.bias) ||
        !std::isfinite(settings.power) || !std::isfinite(settings.maxDistance) || settings.quality<0 || settings.quality>2)
        return "Invalid SSAO settings";
    if(inputs.state==DepthState::CameraUnavailable || !ValidDepthProjection(inputs.nearPlane,inputs.farPlane) || !inputs.ValidFrustum())
        return "Camera projection unavailable";
    if(!ready_ || !inputs.depth || inputs.state!=DepthState::Ready)return "Depth data unavailable";
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};inputs.depth->GetDesc(&view);
    if(view.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || view.Texture2D.MostDetailedMip!=0 ||
        (view.Format!=DXGI_FORMAT_R32_FLOAT && view.Format!=DXGI_FORMAT_R24_UNORM_X8_TYPELESS &&
        view.Format!=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS))return "Unsupported depth input";
    Ptr<ID3D11Device> owner;inputs.depth->GetDevice(&owner);
    if(owner.Get()!=device_.Get())return "Depth device mismatch";
    Ptr<ID3D11Resource> resource;inputs.depth->GetResource(&resource);Ptr<ID3D11Texture2D> texture;
    if(FAILED(resource.As(&texture)))return "Depth texture unavailable";
    D3D11_TEXTURE2D_DESC depth{};texture->GetDesc(&depth);
    if(!depth.Width || !depth.Height || depth.ArraySize!=1 || depth.SampleDesc.Count!=1)return "Unsupported depth dimensions";
    const unsigned aoWidth=settings.quality==0?(depth.Width+1)/2:depth.Width;
    const unsigned aoHeight=settings.quality==0?(depth.Height+1)/2:depth.Height;
    if(!geometry_.texture || width_!=depth.Width || height_!=depth.Height || aoWidth_!=aoWidth || aoHeight_!=aoHeight) {
        ReleaseTargets();
        if(!MakeSurface(geometry_,depth.Width,depth.Height,DXGI_FORMAT_R32G32B32A32_FLOAT) ||
            !MakeSurface(linearDepth_,depth.Width,depth.Height,DXGI_FORMAT_R32_FLOAT) ||
            (settings.quality==0 && !MakeSurface(guide_,aoWidth,aoHeight,DXGI_FORMAT_R32G32B32A32_FLOAT)) ||
            !MakeSurface(ao_,aoWidth,aoHeight,DXGI_FORMAT_R16_FLOAT) ||
            !MakeSurface(ping_,aoWidth,aoHeight,DXGI_FORMAT_R16_FLOAT) ||
            !MakeSurface(ambient_,depth.Width,depth.Height,DXGI_FORMAT_R16_FLOAT)) {
            ReleaseTargets();return "SSAO allocation failed";
        }
        width_=depth.Width;height_=depth.Height;aoWidth_=aoWidth;aoHeight_=aoHeight;
    }
    struct Restore {
        ID3D11DeviceContext1* context;Ptr<ID3DDeviceContextState> previous;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* state):context(c){c->SwapDeviceContextState(state,&previous);}
        ~Restore(){context->ClearState();if(previous)context->SwapDeviceContextState(previous.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();
    struct Controls {
        float nearPlane,farPlane,strength,radius,bias,power,maxDistance,padding;
        float rays[4];unsigned sceneSize[2],aoSize[2],directions,steps;int axis[2];
    };
    static_assert(sizeof(Controls)==80);
    Controls controls{inputs.nearPlane,inputs.farPlane,std::clamp(settings.strength,0.0f,1.0f),
        std::clamp(settings.radius,1.0f,250.0f),std::clamp(settings.bias,0.01f,0.3f),std::clamp(settings.power,1.0f,4.0f),
        std::clamp(settings.maxDistance,500.0f,50000.0f),0,
        {inputs.left,inputs.top,inputs.right-inputs.left,inputs.bottom-inputs.top},
        {width_,height_},{aoWidth_,aoHeight_},settings.quality==2?16u:settings.quality==1?12u:8u,
        settings.quality==2?8u:settings.quality==1?6u:4u,{0,0}};
    auto* buffer=constants_.Get();context_->PSSetConstantBuffers(0,1,&buffer);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->RSSetState(raster_.Get());context_->VSSetShader(vertex_.Get(),nullptr,0);
    int uploadedX=-1,uploadedY=-1;
    auto pass=[&](ID3D11PixelShader* shader,ID3D11RenderTargetView* target,unsigned w,unsigned h,ID3D11ShaderResourceView* guide,ID3D11ShaderResourceView* ao,ID3D11RenderTargetView* extra=nullptr) {
        ID3D11ShaderResourceView* clear[6]{};context_->PSSetShaderResources(0,6,clear);context_->OMSetRenderTargets(0,nullptr,nullptr);
        ID3D11RenderTargetView* targets[]={target,extra};
        context_->OMSetRenderTargets(extra?2u:1u,targets,nullptr);
        const D3D11_VIEWPORT viewport{0,0,float(w),float(h),0,1};context_->RSSetViewports(1,&viewport);
        if(uploadedX!=controls.axis[0] || uploadedY!=controls.axis[1]) {
            context_->UpdateSubresource(constants_.Get(),0,nullptr,&controls,0,0);
            uploadedX=controls.axis[0];uploadedY=controls.axis[1];
        }
        context_->PSSetShader(shader,nullptr,0);
        const bool calculating=shader==aoPixel_.Get() || shader==aoReducedPixel_.Get();
        ID3D11ShaderResourceView* views[]={nullptr,inputs.depth.Get(),guide,ao,
            !calculating?guide_.view.Get():nullptr,calculating?linearDepth_.view.Get():nullptr};
        context_->PSSetShaderResources(0,6,views);
        context_->Draw(3,0);
    };
    pass(geometryPixel_.Get(),geometry_.target.Get(),width_,height_,nullptr,nullptr,linearDepth_.target.Get());
    pass(settings.quality==0?aoReducedPixel_.Get():aoPixel_.Get(),ao_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),nullptr,guide_.target.Get());
    controls.axis[0]=1;pass(filterPixel_.Get(),ping_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),ao_.view.Get());
    controls.axis[0]=0;controls.axis[1]=1;
    if(settings.quality==0) {
        pass(filterPixel_.Get(),ao_.target.Get(),aoWidth_,aoHeight_,geometry_.view.Get(),ping_.view.Get());
        pass(ambientPixel_.Get(),ambient_.target.Get(),width_,height_,geometry_.view.Get(),ao_.view.Get());
    }else pass(filterAmbientPixel_.Get(),ambient_.target.Get(),width_,height_,geometry_.view.Get(),ping_.view.Get());
    return "Native ambient occlusion ready";
}
}
