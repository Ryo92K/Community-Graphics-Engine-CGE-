#include "renderer.h"
#include "shader_source.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>
#include <limits>
namespace cge {
namespace {
float HalfValue(std::uint16_t bits) {
    const float sign=(bits&0x8000)?-1.0f:1.0f;
    const unsigned exponent=(bits>>10)&31,mantissa=bits&1023;
    if(exponent==31)return std::numeric_limits<float>::quiet_NaN();
    if(!exponent)return sign*std::ldexp(static_cast<float>(mantissa),-24);
    return sign*std::ldexp(static_cast<float>(1024+mantissa),static_cast<int>(exponent)-25);
}
}
void Renderer::PollMotionReadback() {
    if(!motionReadPending_ || !context_)return;
    BOOL complete{};
    if(context_->GetData(motionReadbackDone_.Get(),&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK || !complete)return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const auto result=context_->Map(motionReadback_.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
    if(result==DXGI_ERROR_WAS_STILL_DRAWING)return;
    motionReadPending_=false;
    if(FAILED(result))return;
    const unsigned stride=sampledFormat_==DXGI_FORMAT_R16G16_FLOAT?4:8;
    double sum=0,span=0;float peak=0;unsigned count=0,eligible=0;
    for(unsigned y=0;y<18;++y)for(unsigned x=0;x<32;++x) {
        const unsigned px=std::min(sampledWidth_-1,(2*x+1)*sampledWidth_/64);
        const unsigned py=std::min(sampledHeight_-1,(2*y+1)*sampledHeight_/36);
        const auto* bytes=static_cast<const unsigned char*>(mapped.pData)+mapped.RowPitch*py+stride*px;
        float vx{},vy{};
        if(sampledFormat_==DXGI_FORMAT_R32G32_FLOAT){memcpy(&vx,bytes,4);memcpy(&vy,bytes+4,4);}
        else {std::uint16_t hx{},hy{};memcpy(&hx,bytes,2);memcpy(&hy,bytes+2,2);vx=HalfValue(hx);vy=HalfValue(hy);}
        if(!std::isfinite(vx)||!std::isfinite(vy))continue;
        const float dx=vx*static_cast<float>(sampledWidth_),dy=vy*static_cast<float>(sampledHeight_);
        const float pixels=std::sqrt(dx*dx+dy*dy);sum+=pixels;peak=std::max(peak,pixels);++count;
        if(vx*vx+vy*vy<=0.09f) {
            const float length=std::min(pixels*sampledExposure_,2*sampledRadius_);
            span+=length;if(length>=0.5f)++eligible;
        }
    }
    context_->Unmap(motionReadback_.Get(),0);
    if(!motionDiagnosticBudget_){motionReadback_.Reset();motionReadbackDone_.Reset();}
    if(count) {
        char text[256]{};
        sprintf_s(text,"CGE motion sample: mean=%.2f px/frame, peak=%.2f px/frame, mean span=%.2f px, velocity eligible=%.1f%%, exposure=%.3f",
            sum/count,static_cast<double>(peak),span/count,100.0*eligible/count,static_cast<double>(sampledExposure_));
        motionDiagnostic_=text;
    }
}
std::string Renderer::TakeMotionDiagnostics() {
    PollMotionReadback();return std::exchange(motionDiagnostic_,{});
}
void Renderer::CaptureMotionReadback(const MotionPass& motion,float exposure) {
    const auto now=std::chrono::steady_clock::now();
    if(motionReadPending_ || !motionDiagnosticBudget_ || now<nextMotionSample_)return;
    ComPtr<ID3D11Resource> resource;motion.inputs.velocity->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture;if(FAILED(resource.As(&texture)))return;
    D3D11_TEXTURE2D_DESC source{};texture->GetDesc(&source);
    if(source.Format!=DXGI_FORMAT_R16G16_FLOAT && source.Format!=DXGI_FORMAT_R32G32_FLOAT &&
        source.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT)return;
    if(!motionReadback_ || sampledWidth_!=source.Width || sampledHeight_!=source.Height || sampledFormat_!=source.Format) {
        motionReadback_.Reset();motionReadbackDone_.Reset();
        auto d=source;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&motionReadback_)))return;
        D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        if(FAILED(device_->CreateQuery(&query,&motionReadbackDone_))){motionReadback_.Reset();return;}
    }
    sampledWidth_=source.Width;sampledHeight_=source.Height;sampledFormat_=source.Format;
    sampledExposure_=exposure;sampledRadius_=motion.settings.radius;
    context_->CopyResource(motionReadback_.Get(),texture.Get());context_->End(motionReadbackDone_.Get());
    motionReadPending_=true;--motionDiagnosticBudget_;nextMotionSample_=now+std::chrono::milliseconds(750);
}
bool Renderer::PrepareLut(const LutSettings& settings) {
    if(!settings.enabled || !std::isfinite(settings.strength) || settings.strength<=0 || settings.file.empty()) {
        lutStatus_=settings.enabled && settings.file.empty()?"LUT: no file selected":"LUT off";return false;
    }
    if(lutLoaded_ && lutFile_==settings.file) {
        const auto& status=lutView_?lutActiveStatus_:lutFailure_;
        if(lutStatus_!=status)lutStatus_=status;
        return lutView_!=nullptr;
    }
    lutLoaded_=true;lutFile_=settings.file;lutView_.Reset();lutData_={};
    std::string error;
    auto fail=[&](std::string message){lutStatus_=lutFailure_=std::move(message);return false;};
    if(!ValidLutName(settings.file) || lutDirectory_.empty()) return fail("LUT unavailable: invalid name or directory");
    if(!LoadCube(lutDirectory_/settings.file,lutData_,error)) return fail("LUT "+settings.file+": "+error);
    D3D11_TEXTURE3D_DESC d{};d.Width=d.Height=d.Depth=lutData_.size;d.MipLevels=1;
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{lutData_.values.data(),lutData_.size*16,lutData_.size*lutData_.size*16};
    ComPtr<ID3D11Texture3D> texture;
    if(FAILED(device_->CreateTexture3D(&d,&data,&texture)) || FAILED(device_->CreateShaderResourceView(texture.Get(),nullptr,&lutView_))) {
        return fail("LUT "+settings.file+": GPU upload failed");
    }
    lutData_.values.clear();lutData_.values.shrink_to_fit();
    lutStatus_=lutActiveStatus_="LUT active: "+settings.file;return true;
}
bool Renderer::Initialize(ID3D11Device* device,std::string& error) {
    output_.Reset();
    grainStart_=std::chrono::steady_clock::now();
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
    ComPtr<ID3DBlob> vs,ps,mb,distortion;
    if(!compile("FullscreenVertex","vs_5_0",vs) || !compile("ColorPixel","ps_5_0",ps) || !compile("MotionPixel","ps_5_0",mb) || !compile("DistortionPixel","ps_5_0",distortion))return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_)) ||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_)) ||
       FAILED(device->CreatePixelShader(mb->GetBufferPointer(),mb->GetBufferSize(),nullptr,&motionPixel_)) ||
       FAILED(device->CreatePixelShader(distortion->GetBufferPointer(),distortion->GetBufferSize(),nullptr,&distortionPixel_)))return false;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=256;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&constants_)))return false;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    return SUCCEEDED(device->CreateRasterizerState(&raster,&raster_));
}
std::string Renderer::Draw(ID3D11Texture2D* buffer,const ColorSettings& settings,bool enabled,const MotionPass& motion,MotionState* motionState,std::optional<std::uint32_t> grainFrame) {
    PollMotionReadback();
    const bool diagnosticActive=enabled && motion.settings.enabled;
    if(diagnosticActive && (!motionDiagnosticActive_ || !(motion.settings==motionDiagnosticSettings_))) {
        motionDiagnosticBudget_=6;nextMotionSample_={};motionDiagnosticSettings_=motion.settings;
    }
    motionDiagnosticActive_=diagnosticActive;
    if(!diagnosticActive && !motionReadPending_){motionReadback_.Reset();motionReadbackDone_.Reset();}
    MotionState state=enabled && motion.settings.enabled?motion.inputs.state:MotionState::Off;
    const bool lutActive=enabled && PrepareLut(settings.lut);
    auto effective=settings;effective.lensDistortion.enabled=false;if(!lutActive)effective.lut.enabled=false;
    const bool color=enabled && !effective.Neutral();
    D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);
    auto compatible=[&](ID3D11ShaderResourceView* view,bool velocity) {
        if(!view)return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};view->GetDesc(&v);
        if(v.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || v.Texture2D.MostDetailedMip!=0)return false;
        const bool format=velocity?(v.Format==DXGI_FORMAT_R16G16_FLOAT || v.Format==DXGI_FORMAT_R32G32_FLOAT ||
            v.Format==DXGI_FORMAT_R16G16B16A16_FLOAT):
            (v.Format==DXGI_FORMAT_R32_FLOAT || v.Format==DXGI_FORMAT_R24_UNORM_X8_TYPELESS || v.Format==DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
        if(!format)return false;
        ComPtr<ID3D11Device> owner;view->GetDevice(&owner);if(owner.Get()!=device_.Get())return false;
        ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;
        if(FAILED(resource.As(&texture)))return false;
        D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
        return d.Width==desc.Width && d.Height==desc.Height && d.ArraySize==1 && d.SampleDesc.Count==1 && texture.Get()!=buffer;
    };
    float exposure=MotionExposure(motion.settings.strength,motion.seconds);
    bool blur=enabled && motion.settings.enabled && state==MotionState::Ready && exposure>0;
    if(blur && (!compatible(motion.inputs.velocity.Get(),true) || !compatible(motion.inputs.depth.Get(),false))) {
        blur=false;state=MotionState::Unsupported;
        auto describe=[](ID3D11ShaderResourceView* view) {
            if(!view)return std::string("missing");
            D3D11_SHADER_RESOURCE_VIEW_DESC v{};view->GetDesc(&v);
            ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;
            D3D11_TEXTURE2D_DESC d{};if(SUCCEEDED(resource.As(&texture)))texture->GetDesc(&d);
            return std::string("format=")+std::to_string(v.Format)+", view="+std::to_string(v.ViewDimension)+
                ", mip="+std::to_string(v.Texture2D.MostDetailedMip)+", size="+std::to_string(d.Width)+"x"+std::to_string(d.Height)+
                ", samples="+std::to_string(d.SampleDesc.Count)+", array="+std::to_string(d.ArraySize);
        };
        const auto rejection=std::string("CGE motion buffers rejected: velocity {")+describe(motion.inputs.velocity.Get())+
            "}, depth {"+describe(motion.inputs.depth.Get())+"}, output="+std::to_string(desc.Width)+"x"+std::to_string(desc.Height);
        if(rejection!=motionRejection_){motionRejection_=rejection;motionDiagnostic_=rejection;}
    }else if(blur)motionRejection_.clear();
    if(motionState)*motionState=state;
    const bool distortion=enabled && settings.lensDistortion.enabled && std::isfinite(settings.lensDistortion.amount) && settings.lensDistortion.amount!=0;
    const bool process=color || blur || distortion;
    if(!process){output_.Reset();return "idle";}
    if(!context_ || !isolated_)return "renderer unavailable";
    if(colorSpace_!=DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) {
        output_.Reset();return "unsupported output color space="+std::to_string(static_cast<unsigned>(colorSpace_))+"; SDR G22 P709 required";
    }
    if(desc.SampleDesc.Count!=1 || desc.ArraySize!=1 || desc.MipLevels!=1 ||
        (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_R10G10B10A2_UNORM))
    {
        const char* format=desc.Format==DXGI_FORMAT_R10G10B10A2_UNORM?"R10G10B10A2_UNORM":
            desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT?"R16G16B16A16_FLOAT":
            desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB?"R8G8B8A8_UNORM_SRGB":
            desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB?"B8G8R8A8_UNORM_SRGB":"other";
        char detail[320]{};
        std::snprintf(detail,sizeof(detail),"unsupported output; SDR UNORM single-sample required; format=%u (%s), size=%ux%u, samples=%u, sampleQuality=%u, mips=%u, array=%u",
            static_cast<unsigned>(desc.Format),format,desc.Width,desc.Height,desc.SampleDesc.Count,desc.SampleDesc.Quality,desc.MipLevels,desc.ArraySize);
        return detail;
    }
    auto* target=output_.Get(device_.Get(),buffer,desc.Format);
    if(!target)return "output view unavailable";
    if(process && (!source_ || width_!=desc.Width || height_!=desc.Height || format_!=desc.Format)) {
        sourceTarget_.Reset();sourceView_.Reset();source_.Reset();gradedView_.Reset();gradedTarget_.Reset();graded_.Reset();width_=height_=0;
        auto sourceDesc=desc;sourceDesc.Usage=D3D11_USAGE_DEFAULT;sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        sourceDesc.CPUAccessFlags=sourceDesc.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&sourceDesc,nullptr,&source_)) ||
            FAILED(device_->CreateShaderResourceView(source_.Get(),nullptr,&sourceView_)))return "input copy unavailable";
        width_=desc.Width;height_=desc.Height;format_=desc.Format;
    }
    if(((blur && color) || (distortion && (blur || color))) && !graded_) {
        auto d=desc;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags=d.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&graded_)) ||
            FAILED(device_->CreateRenderTargetView(graded_.Get(),nullptr,&gradedTarget_)) ||
            FAILED(device_->CreateShaderResourceView(graded_.Get(),nullptr,&gradedView_))) {
            graded_.Reset();gradedTarget_.Reset();gradedView_.Reset();
            state=MotionState::Unavailable;if(motionState)*motionState=state;
            return "intermediate texture unavailable";
        }
    }
    if(distortion && blur && color && !sourceTarget_ && FAILED(device_->CreateRenderTargetView(source_.Get(),nullptr,&sourceTarget_)))
        return "motion intermediate unavailable";
    struct Restore {
        ID3D11DeviceContext1* context;ComPtr<ID3DDeviceContextState> old;
        Restore(ID3D11DeviceContext1* c,ID3DDeviceContextState* next):context(c){context->SwapDeviceContextState(next,&old);}
        ~Restore(){context->ClearState();if(old)context->SwapDeviceContextState(old.Get(),nullptr);}
    } restore(context_.Get(),isolated_.Get());
    context_->ClearState();
    if(process)context_->CopyResource(source_.Get(),buffer);
    auto* output=target;context_->OMSetRenderTargets(1,&output,nullptr);
    D3D11_VIEWPORT viewport={0,0,float(desc.Width),float(desc.Height),0,1};context_->RSSetViewports(1,&viewport);
    if(process) {
        auto bound=[](float value,float low,float high,float fallback){return std::isfinite(value)?std::clamp(value,low,high):fallback;};
        auto amount=[&](bool enabled,float value){return enabled?bound(value,0,1,0):0;};
        const auto tick=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-grainStart_).count())*24/1000;
        struct Controls {
            float exposure,contrast,saturation,sharpen,gamma,temperature,motionScale,radius;
            UINT samples;float clarity,vibrance;UINT grainFrame;
            float vignette[4],grain[4],split[4],shadows[4],highlights[4],aberration[4],debanding[4],liftStrength[4],gammaRGB[4],gainRGB[4],lutMinimum[4],lutInverse[4],distortion[4];
        } parameters{settings.exposure,settings.contrast,settings.saturation,settings.sharpnessEnabled?settings.sharpen:0,settings.gamma,
            settings.temperature*0.01f,blur?exposure:0,std::clamp(motion.settings.radius,1.0f,64.0f),
            static_cast<UINT>(MotionSettings::ValidSamples(motion.settings.samples)?motion.settings.samples:12),
            settings.clarity.enabled && std::isfinite(settings.clarity.strength)?std::clamp(settings.clarity.strength,0.0f,1.0f):0,
            settings.vibrance.enabled && std::isfinite(settings.vibrance.strength)?std::clamp(settings.vibrance.strength,0.0f,1.0f):0,grainFrame.value_or(static_cast<UINT>(tick)),
            {amount(settings.vignette.enabled,settings.vignette.strength),bound(settings.vignette.radius,0,1,0.35f),bound(settings.vignette.softness,0.01f,1,0.5f),0},
            {amount(settings.filmGrain.enabled,settings.filmGrain.strength),bound(settings.filmGrain.size,1,4,1),0,0},
            {amount(settings.splitToning.enabled,settings.splitToning.strength),bound(settings.splitToning.balance,0,1,0.5f),0,0},
            {bound(settings.splitToning.shadowRed,0,1,0.25f),bound(settings.splitToning.shadowGreen,0,1,0.4f),bound(settings.splitToning.shadowBlue,0,1,0.65f),0},
            {bound(settings.splitToning.highlightRed,0,1,1),bound(settings.splitToning.highlightGreen,0,1,0.72f),bound(settings.splitToning.highlightBlue,0,1,0.45f),0},
            {amount(settings.chromaticAberration.enabled,settings.chromaticAberration.strength),bound(settings.chromaticAberration.radius,0,1,0.5f),0,0},
            {amount(settings.debanding.enabled,settings.debanding.strength),bound(settings.debanding.radius,1,16,4),bound(settings.debanding.threshold,0.005f,0.1f,0.025f),0},
            {bound(settings.liftGammaGain.liftRed,-0.25f,0.25f,0),bound(settings.liftGammaGain.liftGreen,-0.25f,0.25f,0),
                bound(settings.liftGammaGain.liftBlue,-0.25f,0.25f,0),
                settings.liftGammaGain.Neutral()?0:amount(settings.liftGammaGain.enabled,settings.liftGammaGain.strength)},
            {bound(settings.liftGammaGain.gammaRed,0.5f,2.5f,1),bound(settings.liftGammaGain.gammaGreen,0.5f,2.5f,1),
                bound(settings.liftGammaGain.gammaBlue,0.5f,2.5f,1),0},
            {bound(settings.liftGammaGain.gainRed,0,2,1),bound(settings.liftGammaGain.gainGreen,0,2,1),
                bound(settings.liftGammaGain.gainBlue,0,2,1),0},
            {lutData_.minimum[0],lutData_.minimum[1],lutData_.minimum[2],lutActive?amount(true,settings.lut.strength):0},
            {1/(lutData_.maximum[0]-lutData_.minimum[0]),1/(lutData_.maximum[1]-lutData_.minimum[1]),
                1/(lutData_.maximum[2]-lutData_.minimum[2]),static_cast<float>(lutData_.size)},
            {distortion?bound(settings.lensDistortion.amount,-1,1,0):0,0,0,0}};
        static_assert(sizeof(Controls)==256);
        context_->UpdateSubresource(constants_.Get(),0,nullptr,&parameters,0,0);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->RSSetState(raster_.Get());context_->VSSetShader(vertex_.Get(),nullptr,0);
        auto* constants=constants_.Get();context_->PSSetConstantBuffers(0,1,&constants);
        if(color) {
            auto* destination=(blur || distortion)?gradedTarget_.Get():target;
            context_->OMSetRenderTargets(1,&destination,nullptr);context_->PSSetShader(pixel_.Get(),nullptr,0);
            auto* source=sourceView_.Get();context_->PSSetShaderResources(0,1,&source);
            auto* lut=lutActive?lutView_.Get():nullptr;context_->PSSetShaderResources(3,1,&lut);context_->Draw(3,0);
            ID3D11ShaderResourceView* noLut{};context_->PSSetShaderResources(3,1,&noLut);
            ID3D11ShaderResourceView* empty{};context_->PSSetShaderResources(0,1,&empty);
        }
        if(blur) {
            auto* destination=distortion?(color?sourceTarget_.Get():gradedTarget_.Get()):target;context_->OMSetRenderTargets(1,&destination,nullptr);
            ID3D11ShaderResourceView* inputs[]={color?gradedView_.Get():sourceView_.Get(),motion.inputs.velocity.Get(),motion.inputs.depth.Get()};
            context_->PSSetShaderResources(0,3,inputs);
            context_->PSSetShader(motionPixel_.Get(),nullptr,0);context_->Draw(3,0);
            CaptureMotionReadback(motion,exposure);
        }
        if(distortion) {
            ID3D11ShaderResourceView* empty[4]{};context_->PSSetShaderResources(0,4,empty);
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            auto* destination=target;context_->OMSetRenderTargets(1,&destination,nullptr);
            auto* source=(color!=blur)?gradedView_.Get():sourceView_.Get();context_->PSSetShaderResources(0,1,&source);
            context_->PSSetShader(distortionPixel_.Get(),nullptr,0);context_->Draw(3,0);
        }
    }
    if(distortion)return blur?"native motion and lens distortion active":color?"native color and lens distortion active":"native lens distortion active";
    return blur?"native motion pass active":color?"native color pass active":"idle";
}
}
