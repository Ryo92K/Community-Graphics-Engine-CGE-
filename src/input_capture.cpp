#include "input_capture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <atomic>
#include <mutex>
#include <cstring>
namespace cge::input {
namespace {
using State=HRESULT(WINAPI*)(void*,DWORD,LPVOID);
using Events=HRESULT(WINAPI*)(void*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
struct Table {void** address{};State state{};Events events{};};
Table tables[8]{};
std::atomic<unsigned> count{};
std::atomic<Predicate> predicate{};
std::mutex installMutex;
Table* Find(void* device) {
    auto** address=*static_cast<void***>(device);
    for(unsigned i=0;i<count.load(std::memory_order_acquire);++i)if(tables[i].address==address)return tables+i;
    return nullptr;
}
bool Capturing(){auto fn=predicate.load();return fn && fn();}
HRESULT WINAPI ReadState(void* device,DWORD bytes,LPVOID data) {
    auto* table=Find(device);if(!table)return DIERR_GENERIC;
    const auto result=table->state(device,bytes,data);
    if(SUCCEEDED(result) && data && Capturing())memset(data,0,bytes);return result;
}
HRESULT WINAPI ReadEvents(void* device,DWORD bytes,LPDIDEVICEOBJECTDATA data,LPDWORD size,DWORD flags) {
    auto* table=Find(device);if(!table)return DIERR_GENERIC;
    if(!Capturing() || !size)return table->events(device,bytes,data,size,flags);
    DWORD flush=INFINITE;const auto result=table->events(device,bytes,nullptr,&flush,0);
    if(SUCCEEDED(result)){*size=0;return DI_OK;}return result;
}
void Replace(void** slot,void* value) {InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),value);}
}
void Configure(Predicate capture) noexcept {predicate=capture;}
bool HookDeviceTable(void** table) {
    std::lock_guard lock(installMutex);const auto used=count.load();
    for(unsigned i=0;i<used;++i)if(tables[i].address==table)return true;
    if(!table || used>=8 || !table[9] || !table[10])return false;
    DWORD old{};if(!VirtualProtect(table+9,2*sizeof(void*),PAGE_EXECUTE_READWRITE,&old))return false;
    tables[used]={table,reinterpret_cast<State>(table[9]),reinterpret_cast<Events>(table[10])};
    count.store(used+1,std::memory_order_release);
    Replace(table+9,reinterpret_cast<void*>(ReadState));Replace(table+10,reinterpret_cast<void*>(ReadEvents));
    DWORD ignored{};VirtualProtect(table+9,2*sizeof(void*),old,&ignored);return true;
}
bool Install(Predicate capture,std::string& status) {
    Configure(capture);static std::once_flag once;static bool installed=false;
    std::call_once(once,[]{
        wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH))return;
        auto path=std::wstring(system)+L"\\dinput8.dll";
        // Retain module and unacquired probes while their shared device tables are hooked.
        auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return;
        using Create=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
        auto create=reinterpret_cast<Create>(GetProcAddress(module,"DirectInput8Create"));if(!create)return;
        unsigned hooked=0;IDirectInput8A* ansi{};
        if(SUCCEEDED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,reinterpret_cast<void**>(&ansi),nullptr))) {
            for(auto guid:{&GUID_SysKeyboard,&GUID_SysMouse}) {
                IDirectInputDevice8A* device{};
                if(SUCCEEDED(ansi->CreateDevice(*guid,&device,nullptr)) && HookDeviceTable(*reinterpret_cast<void***>(device)))++hooked;
            }
        }
        IDirectInput8W* wide{};
        if(SUCCEEDED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&wide),nullptr))) {
            for(auto guid:{&GUID_SysKeyboard,&GUID_SysMouse}) {
                IDirectInputDevice8W* device{};
                if(SUCCEEDED(wide->CreateDevice(*guid,&device,nullptr)) && HookDeviceTable(*reinterpret_cast<void***>(device)))++hooked;
            }
        }installed=hooked==4;
    });
    status=installed?"CGE input capture ready: native DirectInput keyboard/mouse.":
        "CGE DirectInput capture incomplete; window-message capture remains active.";return installed;
}
void Shutdown() noexcept {
    predicate=nullptr;
    for(unsigned i=0;i<count.load(std::memory_order_acquire);++i) {
        auto& table=tables[i];DWORD old{};
        if(!VirtualProtect(table.address+9,2*sizeof(void*),PAGE_EXECUTE_READWRITE,&old))continue;
        if(table.address[9]==reinterpret_cast<void*>(ReadState))Replace(table.address+9,reinterpret_cast<void*>(table.state));
        if(table.address[10]==reinterpret_cast<void*>(ReadEvents))Replace(table.address+10,reinterpret_cast<void*>(table.events));
        DWORD ignored{};VirtualProtect(table.address+9,2*sizeof(void*),old,&ignored);
    }
}
}
