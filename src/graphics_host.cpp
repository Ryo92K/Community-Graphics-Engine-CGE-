#include "graphics_host.h"
#include "renderer.h"
#include "bloom.h"
#include "fog.h"
#include "ssao.h"
#include "ambient_native.h"
#include "depth_of_field.h"
#include "menu_framework.h"
#include <mutex>
#include "profiles.h"
#include "fps_counter.h"
#include <chrono>
namespace cge::graphics {
namespace {
struct Host {
    std::recursive_mutex mutex;
    IDXGISwapChain* chain{};
    Settings settings;
    FpsCounter fps;
    Renderer renderer;
    BloomRenderer bloom;
    SsaoRenderer ssao;
    std::string ssaoStatus;
    FogRenderer fog;
    DepthOfFieldRenderer dof;
    std::string dofStatus;
    std::string fogStatus;
    std::string bloomStatus;
    bool prepared{},hdrProcessed{},changedLocation{},ambientAvailable{};
    unsigned ambientFrames{};
    float frameSeconds{},hour{-1};
    ColorSettings color;
    std::filesystem::path settingsPath;
    Reporter report;
    unsigned width{},height{};
    std::string status,lutStatus;
    bool sceneProcessed{},ssaoPrepared{};
    bool ssaoTemporal{},ssaoTemporalReported{};
    float ssaoHour{-1};
    ProfileTransition transition;
    WeatherTransition weatherTransition;
    WeatherObservation weather;
    float weatherPollSeconds{};
    WeatherObservation(*weatherProbe)() noexcept{};
    Environment(*probe)() noexcept{};
    float(*hourProbe)() noexcept{};
    MotionInputs(*motionProbe)() noexcept{};
    DepthInputs(*depthProbe)() noexcept{};
    MotionState motionState{MotionState::Off};
    bool motionWasEnabled{};
    const char* motionProbeStatus{};
    unsigned char motionTaa{255};
    bool motionWorldTaa{};
    MotionSettings reportedMotion;
    bool motionSettingsReported{};
    unsigned motionWarmup{2};
    float previousHour{-1};
    Environment environment{Environment::Unknown};
    std::chrono::steady_clock::time_point lastScene;
};
void Prepare(Host& host) {
    if(host.prepared)return;
    host.prepared=true;
    const auto environment=host.probe?host.probe():Environment::Unknown;
    const bool changedLocation=environment!=host.environment;
    if(changedLocation) {
        host.environment=environment;
        if(host.report)host.report(environment==Environment::Interior?"CGE environment: Interior":
            environment==Environment::Exterior?"CGE environment: Exterior":"CGE environment: unavailable");
    }
    const float hour=host.hourProbe?host.hourProbe():-1;
    const auto now=std::chrono::steady_clock::now();
    const float frameSeconds=std::chrono::duration<float>(now-host.lastScene).count();
    const float seconds=std::clamp(frameSeconds,0.0f,0.1f);
    host.lastScene=now;
    host.color=host.transition.Update(host.settings,environment,seconds,hour);
    host.weatherPollSeconds-=seconds;
    if(changedLocation)host.weatherPollSeconds=0;
    if(host.settings.weather.enabled && environment==Environment::Exterior && host.weatherProbe) {
        if(host.weatherPollSeconds<=0){host.weather=host.weatherProbe();host.weatherPollSeconds=0.1f;}
    }else {host.weather={};host.weatherPollSeconds=0;}
    host.color=host.weatherTransition.Update(host.color,host.settings.weather,environment,host.weather,seconds);
    host.frameSeconds=frameSeconds;host.hour=hour;host.changedLocation=changedLocation;
}
                                                                                       
Host& Data(){static auto* host=new Host;return *host;}
}
bool Attach(IDXGISwapChain* chain,ID3D11Device* device,const std::filesystem::path& directory,Reporter report) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain)return false;
    host.motionProbeStatus=nullptr;host.motionTaa=255;host.motionWorldTaa=false;
    host.fps=FpsCounter{};host.motionWarmup=2;host.motionWasEnabled=false;host.previousHour=-1;host.motionState=MotionState::Off;
    host.transition.Reset();host.weatherTransition.Reset();host.weather={};host.environment=Environment::Unknown;host.lastScene=std::chrono::steady_clock::now();
    host.settingsPath=directory/"CGE.ini";host.settings=Settings::Load(host.settingsPath);host.report=std::move(report);
    std::string error;
    if(host.report)host.report("CGE startup stage: initializing SDR shaders");
    if(!host.renderer.Initialize(device,error)) {
        if(host.report)host.report("CGE renderer initialization failed: "+error);
        host.renderer=Renderer{};return false;
    }
    host.renderer.SetLutDirectory(directory/"CGE"/"LUTs");
    if(host.report)host.report("CGE startup stage: initializing HDR effects");
    if(!host.ssao.Initialize(device,error) && host.report)host.report("CGE SSAO initialization failed: "+error);
    if(!host.dof.Initialize(device,error) && host.report)host.report("CGE depth of field initialization failed: "+error);
    if(!host.fog.Initialize(device,error) && host.report)host.report("CGE fog initialization failed: "+error);
    if(!host.bloom.Initialize(device,error) && host.report)host.report("CGE bloom initialization failed: "+error);
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc))){host.renderer=Renderer{};return false;}
    host.prepared=host.hdrProcessed=host.ssaoPrepared=host.ssaoTemporal=host.ssaoTemporalReported=false;host.ssaoHour=-1;
    host.ambientAvailable=ambient::Attach(device,error);
    if(host.report)host.report(host.ambientAvailable?"CGE native ambient shader hooks ready":"CGE native ambient unavailable: "+error);
    host.sceneProcessed=false;host.chain=chain;host.width=desc.BufferDesc.Width;host.height=desc.BufferDesc.Height;
    return true;
}
void Detach(IDXGISwapChain* chain) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
    ambient::Detach();host.ambientAvailable=false;host.ssao=SsaoRenderer{};host.ssaoStatus.clear();host.renderer=Renderer{};host.bloom=BloomRenderer{};host.fog=FogRenderer{};host.dof=DepthOfFieldRenderer{};host.dofStatus.clear();host.fogStatus.clear();host.bloomStatus.clear();host.chain=nullptr;host.status.clear();host.lutStatus.clear();
}
void BeforeResize(IDXGISwapChain* chain) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain==chain){host.renderer.ReleaseOutput();host.ssao.ResetHistory();host.ssaoPrepared=false;}
}
void SetOutputColorSpace(IDXGISwapChain* chain,DXGI_COLOR_SPACE_TYPE color) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
    host.renderer.SetColorSpace(color);
    if(host.report)host.report("CGE output color space="+std::to_string(static_cast<unsigned>(color)));
}
void UpdateDimensions(IDXGISwapChain* chain) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
    DXGI_SWAP_CHAIN_DESC desc{};if(SUCCEEDED(chain->GetDesc(&desc))){host.width=desc.BufferDesc.Width;host.height=desc.BufferDesc.Height;}
    host.motionWarmup=2;
}
void SetEnvironmentProbe(Environment(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.probe=probe;
}
void SetGameHourProbe(float(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.hourProbe=probe;
}
void SetWeatherProbe(WeatherObservation(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.weatherProbe=probe;
}
void SetMotionProbe(MotionInputs(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.motionProbe=probe;
}
void SetDepthProbe(DepthInputs(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.depthProbe=probe;
}
void BeginOpaque() noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        ambient::End();
        if(!host.chain || !host.ambientAvailable)return;
        Prepare(host);
        const bool active=host.settings.enabled && host.environment!=Environment::Unknown;
        if(host.ssaoPrepared) {
            if(host.ssaoStatus=="Native ambient occlusion ready")ambient::Begin(host.ssao.Occlusion());
            return;
        }
        host.ssaoPrepared=true;
        float hourDifference=host.hour>=0 && host.ssaoHour>=0?std::abs(host.hour-host.ssaoHour):0;
        hourDifference=std::min(hourDifference,24-hourDifference);
        if(host.changedLocation || host.frameSeconds<=0 || host.frameSeconds>0.1f || hourDifference>0.5f)host.ssao.ResetHistory();
        host.ssaoHour=host.hour;
        DepthInputs depth;
        if(active && host.color.ssao.enabled && host.color.ssao.strength>0 && host.depthProbe)depth=host.depthProbe();
        auto status=host.ssao.PrepareOcclusion(depth,host.color.ssao,active);
        const bool temporal=host.ssao.TemporalActive();
        if((!host.ssaoTemporalReported || temporal!=host.ssaoTemporal) && host.report)
            host.report(temporal?"CGE SSAO temporal: camera reprojection available":"CGE SSAO temporal: spatial fallback");
        host.ssaoTemporal=temporal;host.ssaoTemporalReported=true;
        if(status=="Native ambient occlusion ready")ambient::Begin(host.ssao.Occlusion());
        if(status!=host.ssaoStatus) {
            host.ssaoStatus=status;if(host.report)host.report("CGE SSAO: "+status);
        }
        if(++host.ambientFrames%120==1 && host.report && active && host.color.ssao.enabled)
            host.report("CGE SSAO: "+ambient::Diagnostics());
    }catch(...) {ambient::End();}
}
void EndOpaque() noexcept {ambient::End();}
void HDR(ID3D11ShaderResourceView* source) noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        if(!host.chain || host.hdrProcessed)return;
        host.hdrProcessed=true;Prepare(host);
        const bool active=host.settings.enabled && host.environment!=Environment::Unknown;
        DepthInputs depth;
        if(active && host.depthProbe && ((host.color.fog.enabled && host.color.fog.density>0) ||
            (host.settings.dofAllowed && host.color.dof.enabled && host.color.dof.strength>0)))depth=host.depthProbe();
        auto fogStatus=host.fog.Draw(source,depth,host.color.fog,active);
        if(fogStatus!=host.fogStatus) {
            host.fogStatus=fogStatus;if(host.report)host.report("CGE distance fog: "+fogStatus);
        }
        if(host.changedLocation)host.dof.ResetAutofocus();
        auto dofStatus=host.dof.Draw(source,depth,host.color.dof,active && host.settings.dofAllowed,host.frameSeconds);
        if(dofStatus!=host.dofStatus) {
            host.dofStatus=dofStatus;if(host.report)host.report("CGE depth of field: "+dofStatus);
        }
        auto status=host.bloom.Draw(source,host.color.bloom,active,host.color.lensDirt);
        
        if(status!=host.bloomStatus) {
            host.bloomStatus=status;if(host.report)host.report("CGE bloom: "+status);
        }
    }catch(const std::exception& e){auto& host=Data();if(host.report)host.report(std::string("CGE HDR effects skipped: ")+e.what());}
    catch(...){}
}
                                                                
void Scene() noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        if(!host.chain || host.sceneProcessed)return;
        host.sceneProcessed=true;
        Prepare(host);
        const auto environment=host.environment;
        const auto frameSeconds=host.frameSeconds;
        const auto hour=host.hour;
        const bool changedLocation=host.changedLocation;
        const auto& color=host.color;
        MotionPass motion;motion.settings=host.settings.motion;motion.seconds=frameSeconds;
        const bool motionEnabled=host.settings.enabled && host.settings.motion.enabled;
        if(!host.motionSettingsReported || !(host.reportedMotion==host.settings.motion)) {
            host.reportedMotion=host.settings.motion;host.motionSettingsReported=true;
            if(host.report)host.report(std::string("CGE motion settings: ")+(host.settings.motion.enabled?"On":"Off")+
                ", strength="+std::to_string(host.settings.motion.strength)+", radius="+std::to_string(host.settings.motion.radius)+
                ", samples="+std::to_string(host.settings.motion.samples));
        }
        float hourDifference=ValidGameHour(hour) && ValidGameHour(host.previousHour)?std::abs(hour-host.previousHour):0;
        hourDifference=std::min(hourDifference,24-hourDifference);
        if(changedLocation || hourDifference>0.25f || !host.motionWasEnabled || frameSeconds>0.1f || menu::IsBlocking())
            host.motionWarmup=2;
        if(ValidGameHour(hour))host.previousHour=hour;
        host.motionWasEnabled=motionEnabled;
        if(motionEnabled && environment!=Environment::Unknown && host.motionProbe)motion.inputs=host.motionProbe();
        else motion.inputs.state=motionEnabled?MotionState::Unavailable:MotionState::Off;
        if(motionEnabled && (host.motionProbeStatus!=motion.inputs.probeStatus ||
            host.motionTaa!=motion.inputs.taa || host.motionWorldTaa!=motion.inputs.worldTaa)) {
            host.motionProbeStatus=motion.inputs.probeStatus;host.motionTaa=motion.inputs.taa;
            host.motionWorldTaa=motion.inputs.worldTaa;
            if(host.report)host.report(std::string("CGE motion probe: ")+motion.inputs.probeStatus+
                ", TAA="+std::to_string(motion.inputs.taa)+", source="+(motion.inputs.worldTaa?"world":"interface"));
        }
        if(motion.inputs.state==MotionState::Ready) {
            if(menu::IsBlocking() || frameSeconds>0.1f)motion.inputs.state=MotionState::Paused;
            else if(host.motionWarmup){--host.motionWarmup;motion.inputs.state=MotionState::Warming;}
        }else if(motionEnabled)host.motionWarmup=2;
        ComPtr<ID3D11Texture2D> buffer;
        if(FAILED(host.chain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return;
        auto status=host.renderer.Draw(buffer.Get(),color,host.settings.enabled,motion,&motion.inputs.state);
        const auto lutStatus=host.settings.enabled && color.lut.enabled?host.renderer.LutStatus():"LUT off";
        if(lutStatus!=host.lutStatus){host.lutStatus=lutStatus;if(host.report)host.report("CGE "+lutStatus);}
        if(status!=host.status){host.status=status;if(host.report)host.report("CGE scene rendering: "+status);}
        const auto diagnostics=host.renderer.TakeMotionDiagnostics();
        if(!diagnostics.empty() && host.report)host.report(diagnostics);
        if(motion.inputs.state!=host.motionState) {
            host.motionState=motion.inputs.state;
            if(host.report)host.report(std::string("CGE motion blur: ")+MotionLabel(host.motionState));
        }
    }catch(const std::exception& e){auto& host=Data();if(host.report)host.report(std::string("CGE scene skipped: ")+e.what());}
    catch(...){}
}
float OverlayFps() noexcept {
    auto& host=Data();std::lock_guard lock(host.mutex);
    return host.chain && host.settings.showFps?host.fps.Value():0;
}
bool ReadSettings(SettingsView& view) {
    auto& host=Data();std::lock_guard lock(host.mutex);
    if(!host.chain)return false;
    view={host.settings,host.environment,host.hour,host.motionState,host.bloomStatus,host.fogStatus,host.dofStatus,host.renderer.LutStatus(),host.ssaoStatus,host.weather};return true;
}
void ApplySettings(const Settings& settings,bool resetProfiles) {
    auto& host=Data();std::lock_guard lock(host.mutex);
    if(host.chain) {
        host.settings=settings;
        if(resetProfiles){host.transition.Reset();host.weatherTransition.Reset();host.weatherPollSeconds=0;host.motionWarmup=2;host.ssao.ResetHistory();}
    }
}
std::filesystem::path LutDirectory() {
    auto& host=Data();std::lock_guard lock(host.mutex);return host.settingsPath.parent_path()/"CGE"/"LUTs";
}
void ReloadLuts() {
    auto& host=Data();std::lock_guard lock(host.mutex);host.renderer.ReloadLut();
}
void HandleToggleKey(unsigned key) noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        if(!host.chain || !key || key!=host.settings.toggleScanCode || menu::IsBlocking())return;
        host.settings.enabled=!host.settings.enabled;host.motionWarmup=2;host.ssao.ResetHistory();
        if(host.report)host.report(std::string("CGE hotkey: effects ")+(host.settings.enabled?"On":"Off"));
        if(!host.settings.Save(host.settingsPath) && host.report)host.report("CGE hotkey: settings save failed");
    }catch(...){}
}
bool SaveSettings() {
    auto& host=Data();std::lock_guard lock(host.mutex);
    return host.chain && host.settings.Save(host.settingsPath);
}
void ResetSsaoHistory() noexcept {
    auto& host=Data();std::lock_guard lock(host.mutex);host.ssao.ResetHistory();host.ssaoPrepared=false;host.weatherTransition.Reset();host.weatherPollSeconds=0;host.weather={};
}
void Frame(IDXGISwapChain* chain) noexcept {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
                                                                                     
    host.fps.Tick(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    ambient::End();if(!host.ssaoPrepared)host.ssao.ResetHistory();
    host.sceneProcessed=host.prepared=host.hdrProcessed=host.ssaoPrepared=false;
}
}
