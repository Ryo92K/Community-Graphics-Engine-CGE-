#include "skse_api.h"
#include "commonlib_bridge.h"
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
void InitializeLogPath() {
    if(!logPath.empty())return;
    wchar_t path[32768]{};const auto length=GetModuleFileNameW(module,path,ARRAYSIZE(path));
    if(!length || length>=ARRAYSIZE(path))return;
    directory=std::filesystem::path(path).parent_path();logPath=directory/"CGE.log";
}
void BootstrapTrace(HINSTANCE instance) noexcept {
    wchar_t path[32768]{};
    const DWORD count=GetModuleFileNameW(instance,path,32768);
    if(!count || count>=32768)return;
    DWORD end=count;while(end && path[end-1]!=L'\\')--end;
    constexpr wchar_t name[]=L"CGE-loader.log";
    if(!end || end+sizeof(name)/sizeof(wchar_t)>32768)return;
    for(DWORD i=0;i<sizeof(name)/sizeof(wchar_t);++i)path[end+i]=name[i];
    HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    constexpr char text[]="CGE 0.8.0: DLL process attach reached; SKSE Load has not run yet.\r\n";
    DWORD written{};WriteFile(file,text,sizeof(text)-1,&written,nullptr);CloseHandle(file);
}
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
    if(!message || !message->sender || strcmp(message->sender,"SKSE"))return;
    if(message->type==2 || message->type==3 || message->type==7)cge::graphics::ResetSsaoHistory();
    if(message->type!=1)return;
    try {
        std::string status;cge::menu::Register(status);Log("%s",status.c_str());
    }catch(...){Log("CGE SKSE Menu Framework registration failed");}
}
HRESULT WINAPI OnCreation(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** chain,ID3D11Device** device,D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    if(!original)return E_UNEXPECTED;
    Log("CGE startup stage: native D3D device creation");
    const auto result=original(adapter,driver,software,flags,levels,count,sdk,desc,chain,device,selected,context);
    Log("CGE D3D device creation returned 0x%08X",static_cast<unsigned>(result));
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
__declspec(dllexport) constinit cge::skse::Metadata SKSEPlugin_Version=cge::skse::BuildMetadata();
__declspec(dllexport) bool SKSEPlugin_Query(const cge::skse::Interface* api,cge::skse::Info* info) {
    InitializeLogPath();
    const bool accepted=info && cge::commonlib::Compatible(api);
    Log("CGE SKSE query: runtime=0x%08X; SKSE=0x%08X; accepted=%u",api?api->runtimeVersion:0,api?api->skseVersion:0,accepted?1u:0u);
    if(!info)return false;
    *info={1,"Community Graphics Engine",cge::skse::Version};
    return accepted;
}
__declspec(dllexport) bool SKSEPlugin_Load(const cge::skse::Interface* api) {
    try {
        wchar_t path[32768]{};auto length=GetModuleFileNameW(module,path,ARRAYSIZE(path));
        if(!length || length>=ARRAYSIZE(path))return false;
        directory=std::filesystem::path(path).parent_path();logPath=directory/"CGE.log";
        Log("CGE SKSE load request: runtime=0x%08X; SKSE=0x%08X",api?api->runtimeVersion:0,api?api->skseVersion:0);
        if(!cge::commonlib::Compatible(api)){Log("CGE load refused: unsupported SKSE interface or executable");return false;}
        if(loaded)return api->runtimeVersion==cge::runtime::selected.version;
        cge::runtime::selected=*cge::runtime::Select(api->runtimeVersion);
        Log("Community Graphics Engine (CGE) 0.8.0 | SKSE plugin | runtime=0x%08X",api->runtimeVersion);
                                                                                          
                                                                                 
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
        Log("CGE compiled runtime family: %s",cge::runtime::BuildFamily);
        Log("CGE CommonLibSSE-NG bridge ready; validated per-runtime graphics adapters retained");
        std::string error;
        Log("CGE startup stage: creation import");
        if(!cge::startup::HookCreation(GetModuleHandleW(nullptr),OnCreation,original,error)) {
            Log("CGE initialization failed: %s",error.c_str());return false;
        }
        wchar_t executablePath[32768]{};
        const auto executableLength=GetModuleFileNameW(nullptr,executablePath,ARRAYSIZE(executablePath));
        auto library=directory/cge::runtime::selected.LibraryName();
        if(executableLength && executableLength<ARRAYSIZE(executablePath)) {
            auto virtualLibrary=std::filesystem::path(executablePath).parent_path()/
                "Data"/"SKSE"/"Plugins"/cge::runtime::selected.LibraryName();
                                                                                      
            std::error_code ec;
            if(std::filesystem::exists(virtualLibrary,ec))library=virtualLibrary;
        }
        Log("CGE runtime adapter: %s; family=%s; Address Library: %ls; SKSE=0x%08X",cge::runtime::selected.VersionString().c_str(),cge::runtime::selected.AdapterName(),library.c_str(),api->skseVersion);
        Log("CGE executable variant: %s",(api->runtimeVersion&15)==1?"GOG":"standard");
        Log("CGE startup stage: scene hook");
        if(cge::startup::HookInterface(GetModuleHandleW(nullptr),library,cge::graphics::Scene,error))
            Log("CGE scene hook ready: DrawInterfaceStart; effects run before HUD and menus");
        else Log("CGE scene hook unavailable: %s. Effects will not run at Present.",error.c_str());
        Log("CGE startup stage: environment adapter");
        if(cge::startup::InitializeEnvironment(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetEnvironmentProbe(cge::startup::PollEnvironment);
            Log("CGE automatic profiles ready: player cell");
        }else Log("CGE automatic profiles unavailable: %s",error.c_str());
        Log("CGE startup stage: clock adapter");
        if(cge::startup::InitializeClock(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetGameHourProbe(cge::startup::PollGameHour);
            Log("CGE day/night clock ready: Calendar");
        }else Log("CGE day/night clock unavailable: %s",error.c_str());
        if(cge::startup::InitializeWeather(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetWeatherProbe(cge::startup::PollWeather);
            Log("CGE weather profiles ready: Sky weather blend");
        }else Log("CGE weather profiles unavailable: %s",error.c_str());
        Log("CGE startup stage: motion adapter");
        if(cge::startup::InitializeMotion(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetMotionProbe(cge::startup::PollMotion);
            Log("CGE native motion resources ready: Renderer, ImageSpaceManager, UI");
        }else Log("CGE motion resources unavailable: %s",error.c_str());
        if(cge::startup::HookInput(GetModuleHandleW(nullptr),library,cge::graphics::HandleToggleKey,error))
            Log("CGE native keyboard toggle ready: default F8; settings in General");
        else Log("CGE native keyboard toggle unavailable: %s",error.c_str());
        Log("CGE startup stage: depth adapter");
        if(cge::startup::InitializeDepth(GetModuleHandleW(nullptr),library,error)) {
            cge::graphics::SetDepthProbe(cge::startup::PollDepth);
            Log("CGE native depth resources ready: main depth, WorldRootCamera, world root");
        }else Log("CGE depth resources unavailable: %s",error.c_str());
        Log("CGE startup stage: native ambient lighting hooks");
        if(cge::startup::HookOpaque(GetModuleHandleW(nullptr),library,cge::graphics::BeginOpaque,cge::graphics::EndOpaque,error))
            Log("CGE opaque hooks ready: visibility before material lighting; ambient occlusion ends before blended decals");
        else Log("CGE native SSAO unavailable: %s",error.c_str());
        Log("CGE startup stage: HDR hook");
        if(cge::startup::HookTonemap(GetModuleHandleW(nullptr),library,cge::graphics::HDR,error))
            Log("CGE HDR hook ready: main cinematic tone mapping; fog and bloom run before tone mapping");
        else Log("CGE HDR hook unavailable: %s",error.c_str());
        if(!cge::commonlib::RegisterListener(api,OnSKSEMessage))
            Log("CGE menu unavailable: SKSE messaging registration failed");
        loaded=true;
        Log("CGE SKSE loaded; graphics creation hook ready; configuration=%ls",(directory/"CGE.ini").c_str());
        return true;
    }catch(...){cge::startup::Shutdown();Log("CGE SKSE initialization failed");return false;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){module=instance;BootstrapTrace(instance);}
    if(reason==DLL_PROCESS_DETACH){cge::startup::Shutdown();}
    return TRUE;
}

