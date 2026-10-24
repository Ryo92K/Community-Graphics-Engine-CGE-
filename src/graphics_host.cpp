#include "graphics_host.h"
#include "renderer.h"
#include "editor.h"
#include "input_capture.h"
#include <mutex>
#include "profiles.h"
#include <chrono>
namespace cge::graphics {
namespace {
struct Host {
    std::recursive_mutex mutex;
    IDXGISwapChain* chain{};
    Settings settings;
    Renderer renderer;
    Editor editor;
    Reporter report;
    unsigned width{},height{};
    std::string status;
    bool sceneProcessed{};
    ProfileTransition transition;
    Environment(*probe)() noexcept{};
    float(*hourProbe)() noexcept{};
    MotionInputs(*motionProbe)() noexcept{};
    MotionState motionState{MotionState::Off};
    bool motionWasEnabled{};
    MotionSettings reportedMotion;
    bool motionSettingsReported{};
    unsigned motionWarmup{2};
    float previousHour{-1};
    Environment environment{Environment::Unknown};
    std::chrono::steady_clock::time_point lastScene;
};
// Graphics teardown is explicit; no COM work runs from CRT shutdown under loader lock.
Host& Data(){static auto* host=new Host;return *host;}
}
bool Attach(IDXGISwapChain* chain,ID3D11Device* device,const std::filesystem::path& directory,Reporter report) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain)return false;
    host.motionWarmup=2;host.motionWasEnabled=false;host.previousHour=-1;host.motionState=MotionState::Off;
    host.transition.Reset();host.environment=Environment::Unknown;host.lastScene=std::chrono::steady_clock::now();
    host.settings=Settings::Load(directory/"CGE.ini");host.report=std::move(report);
    std::string error;
    if(!host.renderer.Initialize(device,error)) {
        if(host.report)host.report("CGE renderer initialization failed: "+error);
        host.renderer=Renderer{};return false;
    }
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc))){host.renderer=Renderer{};return false;}
    host.sceneProcessed=false;host.chain=chain;host.width=desc.BufferDesc.Width;host.height=desc.BufferDesc.Height;
    const bool menu=host.editor.Initialize(desc.OutputWindow,host.settings,directory/"CGE.ini",device);
    if(menu && host.report)host.report(host.editor.UsesInterFont()?"CGE menu font: Inter 16px":"CGE menu font: embedded fallback 16px");
    std::string inputStatus;input::Install([]{return Data().editor.IsOpen();},inputStatus);
    if(host.report){host.report(inputStatus);host.report(menu?"CGE menu ready: F10":"CGE menu unavailable");}
    return true;
}
void Detach(IDXGISwapChain* chain) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
    host.editor.Shutdown();host.renderer=Renderer{};host.chain=nullptr;host.status.clear();
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
void SetMotionProbe(MotionInputs(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.motionProbe=probe;
}
// Skyrim calls this before drawing Scaleform menus and the HUD.
void Scene() noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        if(!host.chain || host.sceneProcessed)return;
        host.sceneProcessed=true;
        const auto environment=host.probe?host.probe():Environment::Unknown;
        const bool changedLocation=environment!=host.environment;
        if(changedLocation) {
            host.environment=environment;
            if(host.report)host.report(environment==Environment::Interior?"CGE environment: Interior":
                environment==Environment::Exterior?"CGE environment: Exterior":"CGE environment: unavailable");
        }
        const float hour=host.hourProbe?host.hourProbe():-1;
        host.editor.SetEnvironment(environment,hour);
        host.editor.Pump(host.width,host.height);
        const auto now=std::chrono::steady_clock::now();
        const float frameSeconds=std::chrono::duration<float>(now-host.lastScene).count();
        const float seconds=std::clamp(frameSeconds,0.0f,0.1f);
        host.lastScene=now;
        const auto color=host.transition.Update(host.settings,environment,seconds,hour);
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
        if(changedLocation || hourDifference>0.25f || !host.motionWasEnabled || frameSeconds>0.1f || host.editor.IsOpen())
            host.motionWarmup=2;
        if(ValidGameHour(hour))host.previousHour=hour;
        host.motionWasEnabled=motionEnabled;
        if(motionEnabled && environment!=Environment::Unknown && host.motionProbe)motion.inputs=host.motionProbe();
        else motion.inputs.state=motionEnabled?MotionState::Unavailable:MotionState::Off;
        if(motion.inputs.state==MotionState::Ready) {
            if(host.editor.IsOpen() || frameSeconds>0.1f)motion.inputs.state=MotionState::Paused;
            else if(host.motionWarmup){--host.motionWarmup;motion.inputs.state=MotionState::Warming;}
        }else if(motionEnabled)host.motionWarmup=2;
        ComPtr<ID3D11Texture2D> buffer;
        if(FAILED(host.chain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return;
        auto status=host.renderer.Draw(buffer.Get(),color,host.settings.enabled,{},motion,&motion.inputs.state);
        if(status!=host.status){host.status=status;if(host.report)host.report("CGE scene rendering: "+status);}
        const auto diagnostics=host.renderer.TakeMotionDiagnostics();
        if(!diagnostics.empty() && host.report)host.report(diagnostics);
        host.editor.SetMotionState(motion.inputs.state);
        if(motion.inputs.state!=host.motionState) {
            host.motionState=motion.inputs.state;
            if(host.report)host.report(std::string("CGE motion blur: ")+MotionLabel(host.motionState));
        }
    }catch(const std::exception& e){auto& host=Data();if(host.report)host.report(std::string("CGE scene skipped: ")+e.what());}
    catch(...){}
}
void Frame(IDXGISwapChain* chain) noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
        if(!host.sceneProcessed)host.editor.Pump(host.width,host.height);
        host.sceneProcessed=false;
        if(!host.editor.IsOpen())return;
        ComPtr<ID3D11Texture2D> buffer;if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return;
        // Present is only used for CGE's editor. Never process the composited HUD.
        auto menuSettings=host.settings;menuSettings.enabled=false;
        host.renderer.Draw(buffer.Get(),menuSettings,false,[&]{host.editor.Draw();});
    }catch(const std::exception& e){auto& host=Data();if(host.report)host.report(std::string("CGE frame skipped: ")+e.what());}
    catch(...){}
}
}
