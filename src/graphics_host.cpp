#include "graphics_host.h"
#include "renderer.h"
#include "editor.h"
#include "input_capture.h"
#include <mutex>
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
};
// Graphics teardown is explicit; no COM work runs from CRT shutdown under loader lock.
Host& Data(){static auto* host=new Host;return *host;}
}
bool Attach(IDXGISwapChain* chain,ID3D11Device* device,const std::filesystem::path& directory,Reporter report) {
    auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain)return false;
    host.settings=Settings::Load(directory/"CGE.ini");host.report=std::move(report);
    std::string error;
    if(!host.renderer.Initialize(device,error)) {
        if(host.report)host.report("CGE renderer initialization failed: "+error);
        host.renderer=Renderer{};return false;
    }
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc))){host.renderer=Renderer{};return false;}
    host.chain=chain;host.width=desc.BufferDesc.Width;host.height=desc.BufferDesc.Height;
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
void Frame(IDXGISwapChain* chain) noexcept {
    try {
        auto& host=Data();std::lock_guard lock(host.mutex);if(host.chain!=chain)return;
        host.editor.Pump(host.width,host.height);
        ComPtr<ID3D11Texture2D> buffer;if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return;
        auto status=host.renderer.Draw(buffer.Get(),host.settings,host.editor.IsOpen()?std::function<void()>([&]{host.editor.Draw();}):std::function<void()>{});
        if(status!=host.status){host.status=status;if(host.report)host.report("CGE rendering: "+status);}
    }catch(const std::exception& e){auto& host=Data();if(host.report)host.report(std::string("CGE frame skipped: ")+e.what());}
    catch(...){}
}
}
