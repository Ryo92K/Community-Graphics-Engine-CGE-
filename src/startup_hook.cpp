#include "startup_hook.h"
#include <cstring>
#include <mutex>
#include <fstream>
#include <array>
#include <cstdint>
#include <cmath>
namespace cge::startup {
namespace {
void* volatile* installedSlot{};
void* previousValue{};
void* replacementValue{};
std::mutex mutex;
using InterfaceDraw=void(*)(std::int64_t);
InterfaceDraw drawInterface{};
void(*beforeInterface)() noexcept{};
const void* playerSingleton{};
const void* calendarSingleton{};
const unsigned char* nativeRenderer{};
const void *uiSingleton{},*imageSpaceSingleton{};
unsigned char* interfaceEntry{};
constexpr std::array<unsigned char,15> interfacePrologue={
    0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x50
};
std::array<unsigned char,15> interfacePatch{};
void Interface(std::int64_t manager) {
    beforeInterface();
    drawInterface(manager);
}
void AbsoluteJump(unsigned char* at,const void* target) {
    at[0]=0xff;at[1]=0x25;
    memset(at+2,0,4);memcpy(at+6,&target,sizeof(target));
}
}
bool HookCreation(HMODULE executable,PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN replacement,
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN& original,std::string& error) {
    std::lock_guard lock(mutex);
    if(installedSlot){error="CGE creation hook already installed";return false;}
    if(!executable || !replacement){error="Missing executable or creation function";return false;}
    auto* base=reinterpret_cast<unsigned char*>(executable);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>1024*1024){error="Invalid executable header";return false;}
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC){error="Executable is not PE64";return false;}
    const auto size=nt->OptionalHeader.SizeOfImage;
    auto valid=[&](size_t offset,size_t count){return offset<size && count<=size-offset;};
    const auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!imports.VirtualAddress || !valid(imports.VirtualAddress,imports.Size)){error="Executable imports unavailable";return false;}
    for(size_t n=0;n+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=imports.Size;n+=sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        auto* import=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+imports.VirtualAddress+n);
        if(!import->Name)break;
        if(!valid(import->Name,1) || !memchr(base+import->Name,0,size-import->Name))continue;
        if(_stricmp(reinterpret_cast<const char*>(base+import->Name),"d3d11.dll"))continue;
        if(!import->OriginalFirstThunk || !import->FirstThunk)continue;
        for(size_t index=0;;++index) {
            const size_t nameOffset=import->OriginalFirstThunk+index*sizeof(IMAGE_THUNK_DATA64);
            const size_t slotOffset=import->FirstThunk+index*sizeof(IMAGE_THUNK_DATA64);
            if(!valid(nameOffset,sizeof(IMAGE_THUNK_DATA64)) || !valid(slotOffset,sizeof(IMAGE_THUNK_DATA64)))break;
            const auto name=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+nameOffset)->u1.AddressOfData;
            if(!name)break;
            if(IMAGE_SNAP_BY_ORDINAL64(name) || !valid(name,3))continue;
            const auto textOffset=static_cast<size_t>(name)+2;
            if(!memchr(base+textOffset,0,size-textOffset))continue;
            if(strcmp(reinterpret_cast<const char*>(base+textOffset),"D3D11CreateDeviceAndSwapChain"))continue;
            auto* slot=reinterpret_cast<void* volatile*>(base+slotOffset);
            auto* prior=*slot;
            if(!prior || prior==reinterpret_cast<void*>(replacement)){error="Creation slot is invalid or already redirected";return false;}
            DWORD protection{};
            if(!VirtualProtect(const_cast<void**>(slot),sizeof(void*),PAGE_READWRITE,&protection)){error="Creation import cannot be updated";return false;}
            // Publish the delegate before replacing the pointer, preserving earlier hooks.
            original=reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(prior);
            InterlockedExchangePointer(slot,reinterpret_cast<void*>(replacement));
            DWORD ignored{};VirtualProtect(const_cast<void**>(slot),sizeof(void*),protection,&ignored);
            installedSlot=slot;previousValue=prior;replacementValue=reinterpret_cast<void*>(replacement);
            return true;
        }
    }
    error="D3D11CreateDeviceAndSwapChain import not found; CGE did not modify the executable";
    return false;
}
namespace {
unsigned char* ResolveV5(HMODULE executable,const std::filesystem::path& library,
    std::uint32_t id,std::size_t bytes,std::string& error) {
    // Address Library V5: 96-byte header followed by one uint32 RVA per ID.
    // Only the verified 1.7.104 ABI/prologue is accepted by this build.
    std::ifstream input(library,std::ios::binary|std::ios::ate);
    if(!input){error="Address Library V5 missing; install the 1.7.104 library through the mod manager";return nullptr;}
    const auto length=input.tellg();input.seekg(0);
    std::array<std::uint32_t,24> header{};
    if(!input.read(reinterpret_cast<char*>(header.data()),sizeof(header)) ||
        header[0]!=5 || header[1]!=1 || header[2]!=7 || header[3]!=104 || header[4]!=0 ||
        memcmp(reinterpret_cast<const char*>(header.data())+20,"SkyrimSE.exe",12) ||
        header[21]!=8 || header[23]<=id || length!=std::streamoff(96ull+4ull*header[23])) {
        error="Invalid or incompatible Address Library V5 header";return nullptr;
    }
    std::uint32_t offset{};input.seekg(96ull+4ull*id);
    if(!input.read(reinterpret_cast<char*>(&offset),sizeof(offset)) || !offset || !executable) {
        error="Requested Address Library entry unavailable";return nullptr;
    }
    auto* base=reinterpret_cast<unsigned char*>(executable);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>1024*1024) {
        error="Invalid interface hook module";return nullptr;
    }
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        offset>=nt->OptionalHeader.SizeOfImage || bytes>nt->OptionalHeader.SizeOfImage-offset) {
        error="Requested address outside executable";return nullptr;
    }
    return base+offset;
}
bool ReadMemory(const void* source,void* target,std::size_t size) noexcept {
    SIZE_T bytes{};
    return source && ReadProcessMemory(GetCurrentProcess(),source,target,size,&bytes) && bytes==size;
}
}
namespace {
bool AcquireNativeView(void* address,ID3D11ShaderResourceView** output) noexcept {
    *output=nullptr;
    void *table{},*query{};
    if(!ReadMemory(address,&table,sizeof(table)) || !ReadMemory(table,&query,sizeof(query)))return false;
    HMODULE owner{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(query),&owner) || owner!=GetModuleHandleW(L"d3d11.dll"))return false;
    // Only a native D3D11 COM object may be queried; game objects are rejected above.
    __try {
        return SUCCEEDED(static_cast<IUnknown*>(address)->QueryInterface(__uuidof(ID3D11ShaderResourceView),
            reinterpret_cast<void**>(output)));
    }__except(EXCEPTION_EXECUTE_HANDLER) {*output=nullptr;return false;}
}
}
bool InitializeMotion(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* renderer=ResolveV5(executable,library,411393,0x20a8,error);
    auto* ui=ResolveV5(executable,library,400327,sizeof(void*),error);
    auto* manager=ResolveV5(executable,library,414660,sizeof(void*),error);
    if(!renderer || !ui || !manager)return false;
    nativeRenderer=renderer;uiSingleton=ui;imageSpaceSingleton=manager;return true;
}
MotionInputs PollMotion() noexcept {
    MotionInputs result;
    if(!nativeRenderer)return result;
    const unsigned char *ui{},*manager{},*temporal{};
    std::uint32_t pauses{};unsigned char taa{};
    if(!ReadMemory(uiSingleton,&ui,sizeof(ui)) || !ui ||
        !ReadMemory(ui+0x160,&pauses,sizeof(pauses)))return result;
    if(pauses){result.state=MotionState::Paused;return result;}
    if(!ReadMemory(imageSpaceSingleton,&manager,sizeof(manager)) || !manager ||
        !ReadMemory(manager+0x1f0,&temporal,sizeof(temporal)) || !temporal ||
        !ReadMemory(temporal+0x18,&taa,sizeof(taa)))return result;
    if(taa!=1){result.state=MotionState::TaaDisabled;return result;}
    void *velocity{},*depth{};
    // Runtime 1.7.104: target 7 SRV at A58 + 7*30 + 18, main depth SRV at 2018 + 88.
    if(!ReadMemory(nativeRenderer+0xbc0,&velocity,sizeof(velocity)) ||
        !ReadMemory(nativeRenderer+0x20a0,&depth,sizeof(depth)))return result;
    ID3D11ShaderResourceView *v{},*d{};
    if(!AcquireNativeView(velocity,&v))return result;
    result.velocity.Attach(v);
    if(!AcquireNativeView(depth,&d)){result.velocity.Reset();return result;}
    result.depth.Attach(d);result.state=MotionState::Ready;return result;
}
bool InitializeEnvironment(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* address=ResolveV5(executable,library,403521,sizeof(void*),error);
    if(!address)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(address,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
        region.Protect&(PAGE_GUARD|PAGE_NOACCESS) ||
        !(region.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))) {
        error="Player singleton is not in readable game data";return false;
    }
    playerSingleton=address;return true;
}
bool InitializeClock(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* address=ResolveV5(executable,library,400447,sizeof(void*),error);
    if(!address)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(address,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
        region.Protect&(PAGE_GUARD|PAGE_NOACCESS) || !(region.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))) {
        error="Calendar singleton is not in readable game data";return false;
    }
    calendarSingleton=address;return true;
}
float PollGameHour() noexcept {
    const unsigned char *calendar{},*global{},*sameCalendar{},*sameGlobal{};
    float hour{};
    // Calendar::gameHour at 0x20; TESGlobal::value at 0x34 on runtime 1.7.104.
    if(!ReadMemory(calendarSingleton,&calendar,sizeof(calendar)) || !calendar ||
        !ReadMemory(calendar+0x20,&global,sizeof(global)) || !global ||
        !ReadMemory(global+0x34,&hour,sizeof(hour)) || !std::isfinite(hour) || hour<0 || hour>24 ||
        !ReadMemory(calendar+0x20,&sameGlobal,sizeof(sameGlobal)) || sameGlobal!=global ||
        !ReadMemory(calendarSingleton,&sameCalendar,sizeof(sameCalendar)) || sameCalendar!=calendar)return -1;
    return hour==24?0:hour;
}
Environment PollEnvironment() noexcept {
    // Runtime 1.7.104: reference parentCell at 0x60, cell interior flag at 0x40.
    // Copy bounded fields instead of dereferencing objects during cell changes.
    const unsigned char *player{},*cell{},*again{},*samePlayer{};
    std::uint32_t formId{};std::uint16_t flags{};
    if(!ReadMemory(playerSingleton,&player,sizeof(player)) || !player ||
        !ReadMemory(player+0x14,&formId,sizeof(formId)) || formId!=0x14 ||
        !ReadMemory(player+0x60,&cell,sizeof(cell)) || !cell ||
        !ReadMemory(cell+0x40,&flags,sizeof(flags)) ||
        !ReadMemory(player+0x60,&again,sizeof(again)) || again!=cell ||
        !ReadMemory(playerSingleton,&samePlayer,sizeof(samePlayer)) || samePlayer!=player)return Environment::Unknown;
    return (flags&1)?Environment::Interior:Environment::Exterior;
}
bool HookInterface(HMODULE executable,const std::filesystem::path& library,
    void(*before)() noexcept,std::string& error) {
    std::lock_guard lock(mutex);
    if(interfaceEntry)return true;
    if(!before){error="Missing interface callback";return false;}
    auto* entry=ResolveV5(executable,library,82084,interfacePrologue.size(),error);
    if(!entry)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(entry,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
        reinterpret_cast<std::uintptr_t>(entry)+interfacePrologue.size()>
            reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize ||
        !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
        memcmp(entry,interfacePrologue.data(),interfacePrologue.size())) {
        error="Interface prologue differs or another plugin already hooks it; scene effects disabled";return false;
    }
    // The verified prefix consists only of whole, non-relative instructions.
    // The relay reproduces it without changing registers or the stack layout.
    auto* relay=static_cast<unsigned char*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!relay){error="Interface relay allocation failed";return false;}
    memcpy(relay,entry,interfacePrologue.size());
    AbsoluteJump(relay+interfacePrologue.size(),entry+interfacePrologue.size());
    DWORD old{};
    if(!VirtualProtect(relay,64,PAGE_EXECUTE_READ,&old)){
        VirtualFree(relay,0,MEM_RELEASE);error="Interface relay protection failed";return false;
    }
    FlushInstructionCache(GetCurrentProcess(),relay,64);
    interfacePatch.fill(0x90);AbsoluteJump(interfacePatch.data(),reinterpret_cast<const void*>(Interface));
    DWORD protection{};
    if(!VirtualProtect(entry,interfacePatch.size(),PAGE_EXECUTE_READWRITE,&protection)){
        VirtualFree(relay,0,MEM_RELEASE);error="Interface entry cannot be patched";return false;
    }
    drawInterface=reinterpret_cast<InterfaceDraw>(relay);beforeInterface=before;
    memcpy(entry,interfacePatch.data(),interfacePatch.size());
    DWORD ignored{};VirtualProtect(entry,interfacePatch.size(),protection,&ignored);
    FlushInstructionCache(GetCurrentProcess(),entry,interfacePatch.size());
    interfaceEntry=entry;
    return true;
}
namespace {
using ImageSpaceDraw=void(*)(void*,std::uint32_t,std::uint32_t,std::uint32_t,void*);
ImageSpaceDraw imageSpaceDraw{};
void(*beforeTonemap)(ID3D11ShaderResourceView*) noexcept{};
const unsigned char* bloomRenderer{};
unsigned char* tonemapCall{};
std::array<unsigned char,5> tonemapOriginal{},tonemapPatch{};
void Tonemap(void* manager,std::uint32_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
    // This delegate replaces only the main cinematic tone-mapping call.
    void* raw{};
    if(source<114 && ReadMemory(bloomRenderer+0xa58+std::size_t(source)*0x30+0x18,&raw,sizeof(raw))) {
        ID3D11ShaderResourceView* view{};
        if(AcquireNativeView(raw,&view)) {
            beforeTonemap(view);view->Release();
        }else beforeTonemap(nullptr);
    }else beforeTonemap(nullptr);
    imageSpaceDraw(manager,effect,source,target,parameters);
}
unsigned char* NearRelay(unsigned char* call) {
    SYSTEM_INFO info{};GetSystemInfo(&info);
    const auto step=std::uintptr_t(info.dwAllocationGranularity);
    const auto center=reinterpret_cast<std::uintptr_t>(call)&~(step-1);
    for(std::uintptr_t distance=step;distance<0x7fff0000;distance+=step) {
        for(int side:{-1,1}) {
            const auto candidate=side<0?(center>distance?center-distance:0):center+distance;
            if(candidate<step)continue;
            MEMORY_BASIC_INFORMATION region{};
            if(!VirtualQuery(reinterpret_cast<void*>(candidate),&region,sizeof(region)) || region.State!=MEM_FREE)continue;
            if(auto* relay=VirtualAlloc(reinterpret_cast<void*>(candidate),64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))
                return static_cast<unsigned char*>(relay);
        }
    }
    return nullptr;
}
}
bool HookTonemap(HMODULE executable,const std::filesystem::path& library,
    void(*before)(ID3D11ShaderResourceView*) noexcept,std::string& error) {
    std::lock_guard lock(mutex);
    if(tonemapCall)return true;
    if(!before){error="Missing HDR callback";return false;}
    auto* main=ResolveV5(executable,library,105674,0x17d,error);
    auto* draw=ResolveV5(executable,library,105676,14,error);
    auto* renderer=ResolveV5(executable,library,411393,0x2018,error);
    if(!main || !draw || !renderer)return false;
    constexpr unsigned char prologue[]={0x4c,0x8b,0xdc,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70};
    constexpr unsigned char arguments[]={0xba,0x22,0,0,0,0x48,0x8b,0xcb};
    auto* call=main+0x178;
    std::int32_t displacement{};
    if(call[0]!=0xe8 || memcmp(main+0x170,arguments,sizeof(arguments)) || memcmp(draw,prologue,sizeof(prologue))) {
        error="HDR call signature differs or is already hooked; bloom disabled";return false;
    }
    memcpy(&displacement,call+1,4);
    if(call+5+displacement!=draw){error="Unexpected HDR call target; bloom disabled";return false;}
    auto* relay=NearRelay(call);
    if(!relay){error="HDR relay allocation failed";return false;}
    AbsoluteJump(relay,reinterpret_cast<const void*>(Tonemap));
    DWORD prior{};
    if(!VirtualProtect(relay,64,PAGE_EXECUTE_READ,&prior)) {
        VirtualFree(relay,0,MEM_RELEASE);error="HDR relay protection failed";return false;
    }
    FlushInstructionCache(GetCurrentProcess(),relay,64);
    const auto relative=reinterpret_cast<std::intptr_t>(relay)-reinterpret_cast<std::intptr_t>(call+5);
    if(relative<INT32_MIN || relative>INT32_MAX) {
        VirtualFree(relay,0,MEM_RELEASE);error="HDR relay outside branch range";return false;
    }
    tonemapPatch[0]=0xe8;displacement=static_cast<std::int32_t>(relative);
    memcpy(tonemapPatch.data()+1,&displacement,4);
    DWORD protection{};
    if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&protection)) {
        VirtualFree(relay,0,MEM_RELEASE);error="HDR call cannot be patched";return false;
    }
    memcpy(tonemapOriginal.data(),call,5);
    imageSpaceDraw=reinterpret_cast<ImageSpaceDraw>(draw);beforeTonemap=before;bloomRenderer=renderer;
    memcpy(call,tonemapPatch.data(),5);
    DWORD ignored{};VirtualProtect(call,5,protection,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
    tonemapCall=call;return true;
}
void Shutdown() noexcept {
    if(tonemapCall && !memcmp(tonemapCall,tonemapPatch.data(),5)) {
        DWORD protection{};
        if(VirtualProtect(tonemapCall,5,PAGE_EXECUTE_READWRITE,&protection)) {
            memcpy(tonemapCall,tonemapOriginal.data(),5);
            DWORD ignored{};VirtualProtect(tonemapCall,5,protection,&ignored);
            FlushInstructionCache(GetCurrentProcess(),tonemapCall,5);
        }
    }
    playerSingleton=nullptr;calendarSingleton=nullptr;nativeRenderer=nullptr;uiSingleton=imageSpaceSingleton=nullptr;
    if(interfaceEntry && !memcmp(interfaceEntry,interfacePatch.data(),interfacePatch.size())) {
        DWORD protection{};
        if(VirtualProtect(interfaceEntry,interfacePatch.size(),PAGE_EXECUTE_READWRITE,&protection)) {
            memcpy(interfaceEntry,interfacePrologue.data(),interfacePrologue.size());
            DWORD ignored{};VirtualProtect(interfaceEntry,interfacePatch.size(),protection,&ignored);
            FlushInstructionCache(GetCurrentProcess(),interfaceEntry,interfacePatch.size());
        }
    }
    if(!installedSlot)return;
    DWORD protection{};
    if(VirtualProtect(const_cast<void**>(installedSlot),sizeof(void*),PAGE_READWRITE,&protection)) {
        InterlockedCompareExchangePointer(installedSlot,previousValue,replacementValue);
        DWORD ignored{};VirtualProtect(const_cast<void**>(installedSlot),sizeof(void*),protection,&ignored);
    }
}
}
