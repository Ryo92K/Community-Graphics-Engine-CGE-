#include "startup_hook.h"
#include <cstring>
#include <mutex>
#include <fstream>
#include <array>
#include <cstdint>
namespace cge::startup {
namespace {
void* volatile* installedSlot{};
void* previousValue{};
void* replacementValue{};
std::mutex mutex;
using InterfaceDraw=void(*)(std::int64_t);
InterfaceDraw drawInterface{};
void(*beforeInterface)() noexcept{};
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
bool HookInterface(HMODULE executable,const std::filesystem::path& library,
    void(*before)() noexcept,std::string& error) {
    std::lock_guard lock(mutex);
    if(interfaceEntry)return true;
    // Address Library V5: 96-byte header followed by one uint32 RVA per ID.
    // Only the verified 1.7.104 ABI/prologue is accepted by this build.
    std::ifstream input(library,std::ios::binary|std::ios::ate);
    if(!input){error="Address Library V5 missing; install the 1.7.104 library through the mod manager";return false;}
    const auto length=input.tellg();input.seekg(0);
    std::array<std::uint32_t,24> header{};
    if(!input.read(reinterpret_cast<char*>(header.data()),sizeof(header)) ||
        header[0]!=5 || header[1]!=1 || header[2]!=7 || header[3]!=104 || header[4]!=0 ||
        memcmp(reinterpret_cast<const char*>(header.data())+20,"SkyrimSE.exe",12) ||
        header[21]!=8 || header[23]<=82084 || length!=std::streamoff(96ull+4ull*header[23])) {
        error="Invalid or incompatible Address Library V5 header";return false;
    }
    std::uint32_t offset{};input.seekg(96ull+4ull*82084);
    if(!input.read(reinterpret_cast<char*>(&offset),sizeof(offset)) || !offset || !executable || !before) {
        error="DrawInterfaceStart address unavailable";return false;
    }
    auto* base=reinterpret_cast<unsigned char*>(executable);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>1024*1024) {
        error="Invalid interface hook module";return false;
    }
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        offset>=nt->OptionalHeader.SizeOfImage || sizeof(interfacePrologue)>nt->OptionalHeader.SizeOfImage-offset) {
        error="Interface address outside executable";return false;
    }
    auto* entry=base+offset;
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
void Shutdown() noexcept {
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
