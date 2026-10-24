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
    Environment environment{Environment::Unknown};
    std::chrono::steady_clock::time_point lastScene;
};
// Graphics teardown is explicit; no COM work runs from CRT shutdown under loader lock.
Host& Data(){static auto* host=new Host;return *host;}
}
bool Attach(IDXGISwapChain* chain,ID3D11Device* device,const std::filesystem::path& directory,Reporter report) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain)return false;
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
}
void SetEnvironmentProbe(Environment(*probe)() noexcept) {
    auto& host=Data();std::lock_guard lock(host.mutex);host.probe=probe;
}
// Skyrim calls this before drawing Scaleform menus and the HUD.
void Scene() noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);
        if(!host.chain || host.sceneProcessed)return;
        host.sceneProcessed=true;
        const auto environment=host.probe?host.probe():Environment::Unknown;
        if(environment!=host.environment) {
            host.environment=environment;
            if(host.report)host.report(environment==Environment::Interior?"CGE environment: Interior":
                environment==Environment::Exterior?"CGE environment: Exterior":"CGE environment: unavailable");
        }
        host.editor.SetEnvironment(environment);
        host.editor.Pump(host.width,host.height);
        const auto now=std::chrono::steady_clock::now();
        const float seconds=std::clamp(std::chrono::duration<float>(now-host.lastScene).count(),0.0f,0.1f);
        host.lastScene=now;
        const auto color=host.transition.Update(host.settings,environment,seconds);
        ComPtr<ID3D11Texture2D> buffer;
        if(FAILED(host.chain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return;
        auto status=host.renderer.Draw(buffer.Get(),color,host.settings.enabled,{});
        if(status!=host.status){host.status=status;if(host.report)host.report("CGE scene rendering: "+status);}
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
