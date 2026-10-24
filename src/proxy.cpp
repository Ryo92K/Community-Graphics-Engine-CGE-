#include "graphics_host.h"
#include "input_capture.h"
#include <windows.h>
#include <mutex>
#include <cstdio>
#include <cstdarg>
namespace {
HMODULE selfModule{},systemModule{};
std::once_flag initialize;
std::mutex logMutex;
std::filesystem::path directory,logPath;
PFN_D3D11_CREATE_DEVICE createDevice{};
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createChain{};
DWORD loadError=ERROR_PROC_NOT_FOUND;
void Log(const char* format,...) {
    if(logPath.empty())return;
    char message[2048]{};va_list args;va_start(args,format);vsnprintf_s(message,sizeof(message),_TRUNCATE,format,args);va_end(args);
    std::lock_guard lock(logMutex);
    auto file=CreateFileW(logPath.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written{};WriteFile(file,message,static_cast<DWORD>(strnlen_s(message,sizeof(message))),&written,nullptr);
    WriteFile(file,"\r\n",2,&written,nullptr);CloseHandle(file);
}
void Initialize() {
    wchar_t path[32768]{};auto length=GetModuleFileNameW(selfModule,path,ARRAYSIZE(path));
    if(length && length<ARRAYSIZE(path)){directory=std::filesystem::path(path).parent_path();logPath=directory/"CGE.log";}
    wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH)){loadError=GetLastError();return;}
    auto runtime=std::wstring(system)+L"\\d3d11.dll";
    systemModule=LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!systemModule || systemModule==selfModule){loadError=GetLastError();return;}
    createDevice=reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(systemModule,"D3D11CreateDevice"));
    createChain=reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(GetProcAddress(systemModule,"D3D11CreateDeviceAndSwapChain"));
    Log("Community Graphics Engine (CGE) 0.1alpha | native SDR effects | PID=%lu",GetCurrentProcessId());
    Log("CGE configuration: %ls",(directory/"CGE.ini").c_str());
}
}
extern "C" HRESULT WINAPI D3D11CreateDevice(
    IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,
    D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    std::call_once(initialize,Initialize);if(!createDevice)return HRESULT_FROM_WIN32(loadError?loadError:ERROR_PROC_NOT_FOUND);
    return createDevice(adapter,driver,software,flags,levels,count,sdk,device,selected,context);
}
extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** chain,ID3D11Device** device,D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    std::call_once(initialize,Initialize);if(!createChain)return HRESULT_FROM_WIN32(loadError?loadError:ERROR_PROC_NOT_FOUND);
    const auto result=createChain(adapter,driver,software,flags,levels,count,sdk,desc,chain,device,selected,context);
    if(SUCCEEDED(result) && chain && *chain) {
        try {
            Microsoft::WRL::ComPtr<ID3D11Device> actual;
            if(device && *device)actual=*device;else (*chain)->GetDevice(IID_PPV_ARGS(&actual));
            if(actual && cge::graphics::Attach(*chain,actual.Get(),directory,[](const std::string& s){Log("%s",s.c_str());})) {
                auto* raw=*chain;*chain=cge::graphics::Wrap(raw);
                if(*chain==raw)cge::graphics::Detach(raw);
            }
        }catch(const std::exception& e){Log("CGE setup skipped: %s",e.what());}catch(...){Log("CGE setup skipped");}
    }return result;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH)selfModule=instance;
    if(reason==DLL_PROCESS_DETACH)cge::input::Shutdown();
    return TRUE;
}
