#include "skse_api.h"
#include "startup_hook.h"
#include "graphics_host.h"
#include "menu_framework.h"
#include <cstdio>
#include <cstdarg>
#include <mutex>
namespace {
HMODULE module{};
std::filesystem::path directory,logPath;
std::mutex logMutex;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN original{};
bool loaded{};
void Log(const char* format,...) {
    if(logPath.empty())return;
    char text[2048]{};va_list args;va_start(args,format);vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);va_end(args);
    std::lock_guard lock(logMutex);
    auto file=CreateFileW(logPath.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written{};WriteFile(file,text,static_cast<DWORD>(strnlen_s(text,sizeof(text))),&written,nullptr);
    WriteFile(file,"\r\n",2,&written,nullptr);CloseHandle(file);
}
void OnSKSEMessage(cge::skse::Message* message) {
    if(!message || !message->sender || strcmp(message->sender,"SKSE") || message->type!=1)return;
    try {
        std::string status;cge::menu::Register(status);Log("%s",status.c_str());
    }catch(...){Log("CGE SKSE Menu Framework registration failed");}
}
HRESULT WINAPI OnCreation(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** chain,ID3D11Device** device,D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    if(!original)return E_UNEXPECTED;
    const auto result=original(adapter,driver,software,flags,levels,count,sdk,desc,chain,device,selected,context);
    if(SUCCEEDED(result) && chain && *chain) {
        try {
            cge::graphics::ComPtr<ID3D11Device> actual;
            if(device && *device)actual=*device;else (*chain)->GetDevice(IID_PPV_ARGS(&actual));
            if(actual && cge::graphics::Attach(*chain,actual.Get(),directory,[](const std::string& s){Log("%s",s.c_str());})) {
                auto* raw=*chain;*chain=cge::graphics::Wrap(raw);
                if(*chain==raw){cge::graphics::Detach(raw);Log("CGE swapchain wrapper unavailable");}
                else Log("CGE SKSE renderer attached");
            }
        }catch(const std::exception& e){Log("CGE graphics setup skipped: %s",e.what());}catch(...){Log("CGE graphics setup skipped");}
    }return result;
}
}
extern "C" {
__declspec(dllexport) constinit cge::skse::Metadata SKSEPlugin_Version={
    1,cge::skse::Version,"Community Graphics Engine","CGE contributors","",
    0,0,{cge::skse::Runtime,0},0
};
__declspec(dllexport) bool SKSEPlugin_Query(const cge::skse::Interface* api,cge::skse::Info* info) {
    if(!info)return false;
    *info={1,"Community Graphics Engine",cge::skse::Version};
    return cge::skse::Compatible(api);
}
__declspec(dllexport) bool SKSEPlugin_Load(const cge::skse::Interface* api) {
    if(!cge::skse::Compatible(api))return false;
    if(loaded)return true;
    try {
        wchar_t path[32768]{};auto length=GetModuleFileNameW(module,path,ARRAYSIZE(path));
        if(!length || length>=ARRAYSIZE(path))return false;
        directory=std::filesystem::path(path).parent_path();logPath=directory/"CGE.log";
        Log("Community Graphics Engine (CGE) 0.5.0 | SKSE plugin | runtime=0x%08X",api->runtimeVersion);
        // A root injector would also see device creation and may install a second editor.
        // Reject that migration conflict before modifying any executable import.
        if(auto runtime=GetModuleHandleW(L"d3d11.dll")) {
            wchar_t runtimePath[32768]{},systemPath[MAX_PATH]{};
            const auto runtimeLength=GetModuleFileNameW(runtime,runtimePath,32768);
            const auto systemLength=GetSystemDirectoryW(systemPath,MAX_PATH);
            if(!runtimeLength || runtimeLength>=32768 || !systemLength || systemLength>=MAX_PATH ||
                _wcsicmp(runtimePath,(std::wstring(systemPath)+L"\\d3d11.dll").c_str())) {
                Log("CGE initialization refused: non-system d3d11.dll is already loaded. Remove the root injector and restart Skyrim.");
                return false;
            }
        }
        std::string error;
        if(!cge::startup::HookCreation(GetModuleHandleW(nullptr),OnCreation,original,error)) {
            Log("CGE initialization failed: %s",error.c_str());return false;
        }
        wchar_t executablePath[32768]{};
        const auto executableLength=GetModuleFileNameW(nullptr,executablePath,ARRAYSIZE(executablePath));
        auto library=directory/"versionlib-1-7-104-0.bin";
        if(executableLength && executableLength<ARRAYSIZE(executablePath)) {
            auto virtualLibrary=std::filesystem::path(executablePath).parent_path()/
                "Data"/"SKSE"/"Plugins"/"versionlib-1-7-104-0.bin";
            // Open through the game's Data path so MO2 can supply another mod's file.
            std::error_code ec;
            if(std::filesystem::exists(virtualLibrary,ec))library=virtualLibrary;
        }
        if(cge::startup::HookInterface(GetModuleHandleW(nullptr),library,cge::graphics::Scene,error))
            Log("CGE scene hook ready: DrawInterfaceStart (82084); effects run before HUD and menus");
        else Log("CGE scene hook unavailable: %s. Effects will not run at Present.",error.c_str());
        if(cge::startup::InitializeEnvironment(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetEnvironmentProbe(cge::startup::PollEnvironment);
            Log("CGE automatic profiles ready: player cell (403521)");
        }else Log("CGE automatic profiles unavailable: %s",error.c_str());
        if(cge::startup::InitializeClock(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetGameHourProbe(cge::startup::PollGameHour);
            Log("CGE day/night clock ready: Calendar (400447)");
        }else Log("CGE day/night clock unavailable: %s",error.c_str());
        if(cge::startup::InitializeMotion(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetMotionProbe(cge::startup::PollMotion);
            Log("CGE native motion resources ready: Renderer (411393), ImageSpaceManager (414660), UI (400327)");
        }else Log("CGE motion resources unavailable: %s",error.c_str());
        if(cge::startup::InitializeFog(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetFogProbe(cge::startup::PollFog);
            Log("CGE distance fog resources ready: main depth, WorldRootCamera (36609), world root (403513)");
        }else Log("CGE fog resources unavailable: %s",error.c_str());
        if(cge::startup::HookTonemap(GetModuleHandleW(nullptr),library,cge::graphics::HDR,error))
            Log("CGE HDR hook ready: main cinematic tone mapping (105674 + 0x178); fog and bloom run before tone mapping");
        else Log("CGE HDR hook unavailable: %s",error.c_str());
        if(api->queryInterface && api->getPluginHandle) {
            auto* messaging=static_cast<cge::skse::Messaging*>(api->queryInterface(5));
            if(!messaging || messaging->version<2 || !messaging->registerListener ||
                !messaging->registerListener(api->getPluginHandle(),"SKSE",OnSKSEMessage))
                Log("CGE menu unavailable: SKSE messaging registration failed");
        }else Log("CGE menu unavailable: SKSE messaging API missing");
        loaded=true;
        Log("CGE SKSE loaded; graphics creation hook ready; configuration=%ls",(directory/"CGE.ini").c_str());
        return true;
    }catch(...){cge::startup::Shutdown();Log("CGE SKSE initialization failed");return false;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH)module=instance;
    if(reason==DLL_PROCESS_DETACH){cge::startup::Shutdown();}
    return TRUE;
}

