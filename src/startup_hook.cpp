#include "startup_hook.h"
#include "commonlib_bridge.h"
#include "address_library.h"
#include "prologue.h"
#include <vector>
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
const unsigned char* depthNativeRenderer{};
const void* worldRootSingleton{};
unsigned char* interfaceEntry{};
std::vector<unsigned char> interfaceOriginal,interfacePatch;
unsigned char* NearRelay(unsigned char* call);
void Interface(std::int64_t manager) {
    beforeInterface();
    commonlib::InvokeInterface(reinterpret_cast<std::uintptr_t>(drawInterface),manager);
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
AddressLibrary addresses;
std::filesystem::path addressPath;
std::uint32_t addressRuntime{};
bool MappedSpan(const void* entry,std::size_t bytes,bool executable) {
    auto cursor=reinterpret_cast<std::uintptr_t>(entry);
    if(!cursor || !bytes || cursor>UINTPTR_MAX-bytes)return false;
    const auto end=cursor+bytes;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
            !(region.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
            (executable && !(region.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))))return false;
        const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if(region.RegionSize>UINTPTR_MAX-base || base+region.RegionSize<=cursor)return false;
        cursor=base+region.RegionSize;
    }
    return true;
}
unsigned char* Resolve(HMODULE executable,const std::filesystem::path& library,
    runtime::Address key,std::size_t bytes,std::string& error) {
    const auto& layout=runtime::selected;
    if(addressPath!=library || addressRuntime!=layout.version) {
        addressPath.clear();addressRuntime=0;
        if(!addresses.Load(library,layout,error))return nullptr;
        addressPath=library;addressRuntime=layout.version;
    }
    const auto offset=addresses.Offset(layout.Id(key));
    if(!offset || !executable){error="Requested Address Library entry unavailable";return nullptr;}
    auto* base=reinterpret_cast<unsigned char*>(executable);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>1024*1024) {
        error="Invalid hook module";return nullptr;
    }
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        offset>=nt->OptionalHeader.SizeOfImage || bytes>nt->OptionalHeader.SizeOfImage-offset) {
        error="Requested address outside executable";return nullptr;
    }
    auto* result=base+offset;
    if(!MappedSpan(result,bytes,false)){error="Requested address is not readable";return nullptr;}
    return result;
}
bool ExecutableSpan(const void* entry,std::size_t bytes) {return MappedSpan(entry,bytes,true);}
std::uintptr_t cameraAccessor{},skyAccessor{};
const unsigned char* NativeSky() noexcept {
    if(!skyAccessor)return nullptr;
    __try {return static_cast<const unsigned char*>(commonlib::InvokeSky(skyAccessor));}
    __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
const unsigned char* NativeCamera() noexcept {
    if(!cameraAccessor)return nullptr;
    __try {return static_cast<const unsigned char*>(commonlib::InvokeCamera(cameraAccessor));}
    __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
bool ReadMemory(const void* source,void* target,std::size_t size) noexcept {
    SIZE_T bytes{};
    return source && ReadProcessMemory(GetCurrentProcess(),source,target,size,&bytes) && bytes==size;
}
struct WorldTaa {
    ULONGLONG tick{};
    unsigned char value{255};
};
thread_local WorldTaa worldTaa;
bool ReadTaa(unsigned char& value) noexcept {
    const unsigned char *manager{},*temporal{};
    return ReadMemory(imageSpaceSingleton,&manager,sizeof(manager)) && manager &&
        ReadMemory(manager+runtime::selected.temporal,&temporal,sizeof(temporal)) && temporal &&
        ReadMemory(temporal+runtime::selected.temporalEnabled,&value,sizeof(value)) && value<=1;
}
void CaptureWorldTaa() noexcept {
    unsigned char value{255};
    worldTaa=ReadTaa(value)?WorldTaa{GetTickCount64(),value}:WorldTaa{};
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
                                                                                      
    __try {
        return SUCCEEDED(static_cast<IUnknown*>(address)->QueryInterface(__uuidof(ID3D11ShaderResourceView),
            reinterpret_cast<void**>(output)));
    }__except(EXCEPTION_EXECUTE_HANDLER) {*output=nullptr;return false;}
}
}
bool InitializeMotion(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* renderer=Resolve(executable,library,runtime::Address::Renderer,runtime::selected.depthView+8,error);
    auto* ui=Resolve(executable,library,runtime::Address::UI,sizeof(void*),error);
    auto* manager=Resolve(executable,library,runtime::Address::ImageSpace,sizeof(void*),error);
    if(!renderer || !ui || !manager)return false;
    nativeRenderer=renderer;uiSingleton=ui;imageSpaceSingleton=manager;return true;
}
MotionInputs PollMotion() noexcept {
    MotionInputs result;
    const auto observed=worldTaa;worldTaa={};
    result.probeStatus="Renderer unavailable";
    if(!nativeRenderer)return result;
    const unsigned char* ui{};
    std::uint32_t pauses{};
    result.probeStatus="UI pause state unavailable";
    if(!ReadMemory(uiSingleton,&ui,sizeof(ui)) || !ui ||
        !ReadMemory(ui+runtime::selected.uiPauses,&pauses,sizeof(pauses)))return result;
    if(pauses){result.probeStatus="Game paused";result.state=MotionState::Paused;return result;}
    result.worldTaa=observed.value<=1 && GetTickCount64()-observed.tick<=100;
    result.probeStatus="TAA state unavailable";
    if(result.worldTaa)result.taa=observed.value;
    else if(!ReadTaa(result.taa))return result;
    if(result.taa!=1){result.probeStatus="Native TAA disabled";result.state=MotionState::TaaDisabled;return result;}
    void *velocity{},*depth{};
    result.probeStatus="Velocity target unavailable";
    if(!ReadMemory(nativeRenderer+runtime::selected.renderTargets+runtime::selected.velocityTarget*runtime::selected.targetStride+runtime::selected.targetView,&velocity,sizeof(velocity)) || !velocity)return result;
    result.probeStatus="Depth target unavailable";
    if(!ReadMemory(nativeRenderer+runtime::selected.depthView,&depth,sizeof(depth)) || !depth)return result;
    ID3D11ShaderResourceView *v{},*d{};
    result.probeStatus="Velocity GPU view unavailable";
    if(!AcquireNativeView(velocity,&v))return result;
    result.velocity.Attach(v);
    result.probeStatus="Depth GPU view unavailable";
    if(!AcquireNativeView(depth,&d)){result.velocity.Reset();return result;}
    result.depth.Attach(d);result.probeStatus="Native buffers acquired";result.state=MotionState::Ready;return result;
}
bool InitializeDepth(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* renderer=Resolve(executable,library,runtime::Address::Renderer,runtime::selected.depthView+8,error);
    auto* worldRoot=Resolve(executable,library,runtime::Address::WorldRoot,sizeof(void*),error);
    auto* getter=Resolve(executable,library,runtime::Address::CameraGetter,15,error);
    if(!renderer || !worldRoot || !getter)return false;
                                                                                     
    if(!ExecutableSpan(getter,15) || !NativeEntry({getter,15})) {
        error="World camera accessor is unsupported or already redirected; depth effects disabled";return false;
    }
    depthNativeRenderer=renderer;worldRootSingleton=worldRoot;
    cameraAccessor=reinterpret_cast<std::uintptr_t>(getter);return true;
}
DepthInputs PollDepth() noexcept {
    DepthInputs result;
    if(!depthNativeRenderer || !worldRootSingleton)return result;
    const unsigned char *root{},*camera{},*sameRoot{},*sameCamera{};
    float planes[6]{},verified[6]{};unsigned char orthographic{};
                                                                                  
    if(!ReadMemory(worldRootSingleton,&root,sizeof(root)) || !root ||
        !(camera=NativeCamera()) ||
        !ReadMemory(camera+runtime::selected.frustum,planes,sizeof(planes)) ||
        !ReadMemory(camera+runtime::selected.frustum+0x18,&orthographic,sizeof(orthographic)) || orthographic ||
        !ValidDepthProjection(planes[4],planes[5]) ||
        !ReadMemory(camera+runtime::selected.frustum,verified,sizeof(verified)) || memcmp(planes,verified,sizeof(planes))!=0 ||
        (sameCamera=NativeCamera())!=camera ||
        !ReadMemory(worldRootSingleton,&sameRoot,sizeof(sameRoot)) || sameRoot!=root) {
        result.state=DepthState::CameraUnavailable;return result;
    }
    std::array<float,16> projection{},sameProjection{};CameraVector position{},samePosition{};
    if(ReadMemory(camera+runtime::selected.cameraProjection,projection.data(),sizeof(projection)) &&
        ReadMemory(camera+runtime::selected.cameraPosition,position.data(),sizeof(position)) &&
        ReadMemory(camera+runtime::selected.cameraProjection,sameProjection.data(),sizeof(sameProjection)) &&
        ReadMemory(camera+runtime::selected.cameraPosition,samePosition.data(),sizeof(samePosition)) &&
        projection==sameProjection && position==samePosition && NativeCamera()==camera)
        result.camera=CameraFromProjection(projection,position);
    result.nearPlane=planes[4];result.farPlane=planes[5];
    result.left=planes[0];result.right=planes[1];result.top=planes[2];result.bottom=planes[3];
    void* raw{};
    if(!ReadMemory(depthNativeRenderer+runtime::selected.depthView,&raw,sizeof(raw)))return result;
    ID3D11ShaderResourceView* view{};
    if(!AcquireNativeView(raw,&view))return result;
    result.depth.Attach(view);result.state=DepthState::Ready;return result;
}
bool InitializeEnvironment(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    auto* address=Resolve(executable,library,runtime::Address::Player,sizeof(void*),error);
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
    auto* address=Resolve(executable,library,runtime::Address::Calendar,sizeof(void*),error);
    if(!address)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(address,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
        region.Protect&(PAGE_GUARD|PAGE_NOACCESS) || !(region.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))) {
        error="Calendar singleton is not in readable game data";return false;
    }
    calendarSingleton=address;return true;
}
bool InitializeWeather(HMODULE executable,const std::filesystem::path& library,std::string& error) {
    skyAccessor=0;
    auto* address=Resolve(executable,library,runtime::Address::SkyGetter,16,error);
    if(!address || !ExecutableSpan(address,16)) {
        if(address)error="Sky getter is not executable";
        return false;
    }
    skyAccessor=reinterpret_cast<std::uintptr_t>(address);return true;
}
WeatherObservation PollWeather() noexcept {
    const auto* sky=NativeSky();
    const unsigned char *current{},*last{},*sameCurrent{},*sameLast{};
    float progress{};unsigned char currentFlags{},lastFlags{},form{};
    if(!sky || !ReadMemory(sky+0x48,&current,sizeof(current)) || !current ||
        !ReadMemory(sky+0x50,&last,sizeof(last)) || !ReadMemory(sky+0x1b8,&progress,sizeof(progress)) ||
        !ReadMemory(current+0x1a,&form,1) || form!=0x36 || !ReadMemory(current+0x66f,&currentFlags,1))return {};
    if(last && progress<1 && (!ReadMemory(last+0x1a,&form,1) || form!=0x36 || !ReadMemory(last+0x66f,&lastFlags,1)))return {};
    if(!ReadMemory(sky+0x48,&sameCurrent,sizeof(sameCurrent)) || current!=sameCurrent ||
        !ReadMemory(sky+0x50,&sameLast,sizeof(sameLast)) || last!=sameLast || NativeSky()!=sky)return {};
    return ObserveWeather(currentFlags,lastFlags,progress,last!=nullptr);
}
float PollGameHour() noexcept {
    const unsigned char *calendar{},*global{},*sameCalendar{},*sameGlobal{};
    float hour{};
                                                                               
    if(!ReadMemory(calendarSingleton,&calendar,sizeof(calendar)) || !calendar ||
        !ReadMemory(calendar+0x20,&global,sizeof(global)) || !global ||
        !ReadMemory(global+0x34,&hour,sizeof(hour)) || !std::isfinite(hour) || hour<0 || hour>24 ||
        !ReadMemory(calendar+0x20,&sameGlobal,sizeof(sameGlobal)) || sameGlobal!=global ||
        !ReadMemory(calendarSingleton,&sameCalendar,sizeof(sameCalendar)) || sameCalendar!=calendar)return -1;
    return hour==24?0:hour;
}
Environment PollEnvironment() noexcept {
                                                                                 
                                                                                
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
    auto* entry=Resolve(executable,library,runtime::Address::Interface,32,error);
    if(!entry)return false;
    if(!ExecutableSpan(entry,32)){error="Interface entry is not executable";return false;}
    auto* relay=NearRelay(entry);
    if(!relay){error="Interface relay allocation failed";return false;}
    const auto length=RelocatePrologue({entry,32},{relay,26});
    if(!length) {
        VirtualFree(relay,0,MEM_RELEASE);
        error="Interface prologue cannot be safely relocated or is already hooked; entry bytes=";
        constexpr char hex[]="0123456789ABCDEF";
        for(unsigned n=0;n<8;++n){error+=' ';error+=hex[entry[n]>>4];error+=hex[entry[n]&15];}
        error+="; scene effects disabled";return false;
    }
    interfaceOriginal.assign(entry,entry+length);
    AbsoluteJump(relay+length,entry+length);
    AbsoluteJump(relay+40,reinterpret_cast<const void*>(Interface));
    const auto branch=reinterpret_cast<std::intptr_t>(relay+40)-reinterpret_cast<std::intptr_t>(entry+5);
    if(branch<INT32_MIN || branch>INT32_MAX){VirtualFree(relay,0,MEM_RELEASE);error="Interface relay outside branch range";return false;}
    DWORD old{};
    if(!VirtualProtect(relay,64,PAGE_EXECUTE_READ,&old)){
        VirtualFree(relay,0,MEM_RELEASE);error="Interface relay protection failed";return false;
    }
    FlushInstructionCache(GetCurrentProcess(),relay,64);
    interfacePatch.assign(length,0x90);interfacePatch[0]=0xe9;
    const auto displacement=static_cast<std::int32_t>(branch);memcpy(interfacePatch.data()+1,&displacement,4);
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
using ImageSpaceDraw=void(*)(void*,std::uintptr_t,std::uint32_t,std::uint32_t,void*);
ImageSpaceDraw imageSpaceDraw{};
void(*beforeTonemap)(ID3D11ShaderResourceView*) noexcept{};
const unsigned char* bloomRenderer{};
unsigned char* tonemapCall{};
struct CallPatch {
    unsigned char* call{};
    unsigned char* relay{};
    std::array<unsigned char,5> original{},patch{};
};
std::vector<CallPatch> hdrPatches,opaquePatches;
using WorldDraw=void(*)(bool);
using OpaqueDraw=void(*)(void*,std::uint32_t,std::uint32_t,std::uint32_t,int);
using DecalDraw=void(*)(void*,std::uint32_t);
WorldDraw drawWorld{};
OpaqueDraw drawOpaque{};
DecalDraw drawDecals{};
void(*beginOpaque)() noexcept{};
void(*endOpaque)() noexcept{};
thread_local bool inMainWorld=false;
thread_local bool capturedWorldTaa=false;
void World(bool value) {
    const bool parent=inMainWorld;const bool captured=capturedWorldTaa;
    if(!parent){capturedWorldTaa=false;worldTaa={};}
    inMainWorld=true;
    drawWorld(value);
    endOpaque();inMainWorld=parent;capturedWorldTaa=captured;
}
void Opaque(void* batch,std::uint32_t start,std::uint32_t end,std::uint32_t flags,int group) {
    if(inMainWorld){if(!capturedWorldTaa){CaptureWorldTaa();capturedWorldTaa=true;}beginOpaque();}
    drawOpaque(batch,start,end,flags,group);
}
void Decals(void* accumulator,std::uint32_t flags) {
    if(inMainWorld)endOpaque();
    drawDecals(accumulator,flags);
}
void Tonemap(void* manager,std::uintptr_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
                                                                        
    void* raw{};
    if(source<runtime::selected.targetCount && ReadMemory(bloomRenderer+runtime::selected.renderTargets+std::size_t(source)*runtime::selected.targetStride+runtime::selected.targetView,&raw,sizeof(raw))) {
        ID3D11ShaderResourceView* view{};
        if(AcquireNativeView(raw,&view)) {
            beforeTonemap(view);view->Release();
        }else beforeTonemap(nullptr);
    }else beforeTonemap(nullptr);
    commonlib::InvokeImageSpace(reinterpret_cast<std::uintptr_t>(imageSpaceDraw),manager,effect,source,target,parameters);
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
namespace {
CallPatch inputPatch;
using InputSend=void(*)(void*,const void*);
InputSend sendInput{};
void(*keyPressed)(unsigned) noexcept{};
const void* inputUiSingleton{};
void InputPress(unsigned key) noexcept {
    const unsigned char* ui{};std::uint32_t pauses{};
    if(keyPressed && ReadMemory(inputUiSingleton,&ui,sizeof(ui)) && ui &&
        ReadMemory(ui+runtime::selected.uiPauses,&pauses,sizeof(pauses)) && !pauses)keyPressed(key);
}
void Input(void* source,const void* events) {
    commonlib::ProcessKeyboard(events,InputPress);
    sendInput(source,events);
}
}
bool HookInput(HMODULE executable,const std::filesystem::path& library,void(*press)(unsigned) noexcept,std::string& error) {
    std::lock_guard lock(mutex);if(inputPatch.call)return true;
    if(!press){error="Missing keyboard callback";return false;}
    auto* poll=Resolve(executable,library,runtime::Address::InputDispatch,0x80,error);
    auto* ui=Resolve(executable,library,runtime::Address::UI,sizeof(void*),error);
    if(!poll || !ui)return false;
    auto* call=poll+0x7b;
    if(!ExecutableSpan(call,5) || call[0]!=0xe8){error="Native input dispatch signature differs or is already hooked";return false;}
    std::int32_t displacement{};memcpy(&displacement,call+1,4);auto* target=call+5+displacement;
    HMODULE owner{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(target),&owner) || owner!=executable || !ExecutableSpan(target,32) || !NativeEntry({target,32})) {
        error="Native input dispatch target is unsupported";return false;
    }
    CallPatch patch;patch.call=call;memcpy(patch.original.data(),call,5);patch.relay=NearRelay(call);
    if(!patch.relay){error="Input relay allocation failed";return false;}
    AbsoluteJump(patch.relay,reinterpret_cast<void*>(Input));
    const auto delta=reinterpret_cast<std::intptr_t>(patch.relay)-reinterpret_cast<std::intptr_t>(call+5);
    DWORD old{},ignored{};
    if(delta<INT32_MIN || delta>INT32_MAX || !VirtualProtect(patch.relay,64,PAGE_EXECUTE_READ,&old)) {
        VirtualFree(patch.relay,0,MEM_RELEASE);error="Input relay cannot be used";return false;
    }
    FlushInstructionCache(GetCurrentProcess(),patch.relay,64);
    patch.patch[0]=0xe8;displacement=static_cast<std::int32_t>(delta);memcpy(patch.patch.data()+1,&displacement,4);
    if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)) {
        VirtualFree(patch.relay,0,MEM_RELEASE);error="Input call cannot be patched";return false;
    }
    sendInput=reinterpret_cast<InputSend>(target);keyPressed=press;inputUiSingleton=ui;
    memcpy(call,patch.patch.data(),5);VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
    inputPatch=patch;return true;
}

bool HookTonemap(HMODULE executable,const std::filesystem::path& library,
    void(*before)(ID3D11ShaderResourceView*) noexcept,std::string& error) {
    std::lock_guard lock(mutex);
    if(tonemapCall)return true;
    if(!before){error="Missing HDR callback";return false;}
    const auto& layout=runtime::selected;
    const unsigned span=layout.tonemapCalls[layout.callCount-1]+5;
    auto* main=Resolve(executable,library,runtime::Address::Tonemap,span,error);
    auto* renderer=Resolve(executable,library,runtime::Address::Renderer,layout.depthView+8,error);
    if(!main || !renderer || !ExecutableSpan(main,span))return false;
    std::vector<CallPatch> pending;
    unsigned char* draw{};
    for(unsigned i=0;i<layout.callCount;++i) {
        auto* call=main+layout.tonemapCalls[i];
        constexpr unsigned char arguments[]={0xba,0x22,0,0,0,0x48,0x8b,0xcb};
        if(call[0]!=0xe8 || (!layout.Id(runtime::Address::Draw) && memcmp(call-8,arguments,sizeof(arguments)))) {
            error="HDR call signature differs or is already hooked; HDR effects disabled";return false;
        }
        std::int32_t displacement{};memcpy(&displacement,call+1,4);
        auto* target=call+5+displacement;
        HMODULE owner{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(target),&owner)) {
            error="Unexpected HDR call target: target module unavailable; HDR effects disabled";return false;
        }
        if(owner!=executable) {
            char path[32768]{};const auto length=GetModuleFileNameA(owner,path,32768);
            path[32767]=0;const char* name=length?strrchr(path,'\\'):nullptr;
            error="Unexpected HDR call target: outside executable, module="+std::string(name?name+1:length?path:"unknown")+"; HDR effects disabled";return false;
        }
        if(!ExecutableSpan(target,32)) {
            error="Unexpected HDR call target: unreadable or non-executable entry; HDR effects disabled";return false;
        }
        if(!NativeEntry({target,32})) {
            error="Unexpected HDR call target: unsupported entry bytes";
            constexpr char hex[]="0123456789ABCDEF";
            for(unsigned n=0;n<8;++n){error+=' ';error+=hex[target[n]>>4];error+=hex[target[n]&15];}
            error+="; HDR effects disabled";return false;
        }
        if(draw && draw!=target) {
            error="Unexpected HDR call target: tone-map calls use different native targets; HDR effects disabled";return false;
        }
        draw=target;
        if(layout.Id(runtime::Address::Draw)) {
            auto* expected=Resolve(executable,library,runtime::Address::Draw,32,error);
            if(!expected || draw!=expected){error="HDR target does not match Address Library";return false;}
        }
        CallPatch item;item.call=call;memcpy(item.original.data(),call,5);pending.push_back(item);
    }
    auto discard=[&] {
        for(auto& item:pending) {
            if(item.call && item.patch[0] && !memcmp(item.call,item.patch.data(),5)) {
                DWORD old{},ignored{};
                if(VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&old)) {
                    memcpy(item.call,item.original.data(),5);VirtualProtect(item.call,5,old,&ignored);
                    FlushInstructionCache(GetCurrentProcess(),item.call,5);
                }
            }
            if(item.relay)VirtualFree(item.relay,0,MEM_RELEASE);
        }
    };
    for(auto& item:pending) {
        item.relay=NearRelay(item.call);
        if(!item.relay){discard();error="HDR relay allocation failed";return false;}
        AbsoluteJump(item.relay,reinterpret_cast<const void*>(Tonemap));
        DWORD old{};
        if(!VirtualProtect(item.relay,64,PAGE_EXECUTE_READ,&old)){discard();error="HDR relay protection failed";return false;}
        FlushInstructionCache(GetCurrentProcess(),item.relay,64);
        const auto delta=reinterpret_cast<std::intptr_t>(item.relay)-reinterpret_cast<std::intptr_t>(item.call+5);
        if(delta<INT32_MIN || delta>INT32_MAX){discard();error="HDR relay outside branch range";return false;}
        item.patch[0]=0xe8;const auto displacement=static_cast<std::int32_t>(delta);memcpy(item.patch.data()+1,&displacement,4);
    }
    imageSpaceDraw=reinterpret_cast<ImageSpaceDraw>(draw);beforeTonemap=before;bloomRenderer=renderer;
    for(auto& item:pending) {
        DWORD protection{},ignored{};
        if(!VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&protection)){discard();error="HDR call cannot be patched";return false;}
        memcpy(item.call,item.patch.data(),5);VirtualProtect(item.call,5,protection,&ignored);
        FlushInstructionCache(GetCurrentProcess(),item.call,5);
    }
    hdrPatches=std::move(pending);tonemapCall=hdrPatches.front().call;return true;
}

bool HookOpaque(HMODULE executable,const std::filesystem::path& library,
    void(*begin)() noexcept,void(*end)() noexcept,std::string& error) {
    std::lock_guard lock(mutex);
    if(!opaquePatches.empty())return true;
    if(!begin || !end){error="Missing opaque callbacks";return false;}
    const auto& layout=runtime::selected;
    const unsigned worldOffset=layout.worldCall;
    auto* frame=Resolve(executable,library,runtime::Address::WorldFrame,worldOffset+5,error);
    auto* opaque=Resolve(executable,library,runtime::Address::WorldOpaque,layout.decalCall+5,error);
    if(!frame || !opaque)return false;
    std::array<unsigned char*,3> calls={frame+worldOffset,opaque+layout.opaqueCall,opaque+layout.decalCall};
    std::array<void*,3> callbacks={reinterpret_cast<void*>(World),reinterpret_cast<void*>(Opaque),reinterpret_cast<void*>(Decals)};
    std::array<void*,3> targets{};std::vector<CallPatch> pending;
    auto discard=[&] {
        for(auto& item:pending) {
            DWORD old{},ignored{};
            if(item.call && item.patch[0] && !memcmp(item.call,item.patch.data(),5) &&
                VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&old)) {
                memcpy(item.call,item.original.data(),5);VirtualProtect(item.call,5,old,&ignored);
                FlushInstructionCache(GetCurrentProcess(),item.call,5);
            }
            if(item.relay)VirtualFree(item.relay,0,MEM_RELEASE);
        }
    };
    for(unsigned i=0;i<3;++i) {
        auto* call=calls[i];
        if(!ExecutableSpan(call,5) || call[0]!=0xe8) {discard();error="Opaque call signature differs or is already hooked";return false;}
        std::int32_t displacement{};memcpy(&displacement,call+1,4);auto* target=call+5+displacement;
        HMODULE owner{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(target),&owner) || owner!=executable || !ExecutableSpan(target,32) || !NativeEntry({target,32})) {
            discard();error="Opaque call target is unsupported";return false;
        }
        targets[i]=target;CallPatch item;item.call=call;memcpy(item.original.data(),call,5);
        item.relay=NearRelay(call);
        if(!item.relay){discard();error="Opaque relay allocation failed";return false;}
        pending.push_back(item);auto& patch=pending.back();AbsoluteJump(patch.relay,callbacks[i]);
        DWORD old{};
        if(!VirtualProtect(patch.relay,64,PAGE_EXECUTE_READ,&old)){discard();error="Opaque relay protection failed";return false;}
        FlushInstructionCache(GetCurrentProcess(),patch.relay,64);
        const auto delta=reinterpret_cast<std::intptr_t>(patch.relay)-reinterpret_cast<std::intptr_t>(call+5);
        if(delta<INT32_MIN || delta>INT32_MAX){discard();error="Opaque relay outside branch range";return false;}
        patch.patch[0]=0xe8;const auto relative=static_cast<std::int32_t>(delta);memcpy(patch.patch.data()+1,&relative,4);
    }
    drawWorld=reinterpret_cast<WorldDraw>(targets[0]);drawOpaque=reinterpret_cast<OpaqueDraw>(targets[1]);
    drawDecals=reinterpret_cast<DecalDraw>(targets[2]);beginOpaque=begin;endOpaque=end;
    for(auto& item:pending) {
        DWORD protection{},ignored{};
        if(!VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&protection)){discard();error="Opaque call cannot be patched";return false;}
        memcpy(item.call,item.patch.data(),5);VirtualProtect(item.call,5,protection,&ignored);
        FlushInstructionCache(GetCurrentProcess(),item.call,5);
    }
    opaquePatches=std::move(pending);return true;
}

void Shutdown() noexcept {
    if(inputPatch.call && !memcmp(inputPatch.call,inputPatch.patch.data(),5)) {
        DWORD old{},ignored{};
        if(VirtualProtect(inputPatch.call,5,PAGE_EXECUTE_READWRITE,&old)) {
            memcpy(inputPatch.call,inputPatch.original.data(),5);VirtualProtect(inputPatch.call,5,old,&ignored);
            FlushInstructionCache(GetCurrentProcess(),inputPatch.call,5);
        }
    }
    for(const auto& item:opaquePatches) {
        DWORD old{},ignored{};
        if(item.call && !memcmp(item.call,item.patch.data(),5) && VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&old)) {
            memcpy(item.call,item.original.data(),5);VirtualProtect(item.call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),item.call,5);
        }
    }
    for(const auto& item:hdrPatches) {
        if(item.call && !memcmp(item.call,item.patch.data(),5)) {
            DWORD protection{},ignored{};
            if(VirtualProtect(item.call,5,PAGE_EXECUTE_READWRITE,&protection)) {
                memcpy(item.call,item.original.data(),5);VirtualProtect(item.call,5,protection,&ignored);
                FlushInstructionCache(GetCurrentProcess(),item.call,5);
            }
        }
    }
    worldTaa={};
    depthNativeRenderer=nullptr;worldRootSingleton=nullptr;cameraAccessor=0;
    playerSingleton=nullptr;calendarSingleton=nullptr;skyAccessor=0;nativeRenderer=nullptr;uiSingleton=imageSpaceSingleton=nullptr;
    if(interfaceEntry && !memcmp(interfaceEntry,interfacePatch.data(),interfacePatch.size())) {
        DWORD protection{};
        if(VirtualProtect(interfaceEntry,interfacePatch.size(),PAGE_EXECUTE_READWRITE,&protection)) {
            memcpy(interfaceEntry,interfaceOriginal.data(),interfaceOriginal.size());
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
