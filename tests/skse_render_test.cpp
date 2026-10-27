#include "skse_api.h"
#include "settings.h"
#include "presets.h"
#include "weather_profiles.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
template<class T>T Proc(HMODULE m,const char* name){auto p=reinterpret_cast<T>(GetProcAddress(m,name));Check(p!=nullptr,name);return p;}
void Write(const fs::path& p,const std::string& text){std::ofstream f(p);Check(bool(f<<text),"Write fixture");}
#pragma section(".cgeui",read,execute)
__declspec(allocate(".cgeui")) const unsigned char interfaceFixture[64]={
    0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x50,
    0x48,0x83,0xc4,0x50,0x41,0x5f,0x41,0x5d,0x41,0x5c,0x5f,0x5e,0xc3
};
bool interfaceRip{},interfaceLea{},interfaceRip32{},interfaceStore32{},interfaceEndbr{};
std::uint64_t interfaceDelegations{};
bool shortPrologue{},ripEntry{},cameraVariant{},hdrReordered{},hdrWide{},hdrMismatch{};
std::uintptr_t expectedHdrEffect{0x22};
cge::runtime::Layout fixtureLayout=*cge::runtime::Select(cge::skse::Runtime);
void PrepareInterfaceFixture() {
    if(interfaceRip || interfaceLea || interfaceRip32 || interfaceStore32 || interfaceEndbr) {
        auto* code=const_cast<unsigned char*>(interfaceFixture);DWORD old{},ignored{};
        Check(VirtualProtect(code,64,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Relative interface writable");
        const unsigned char bytes[]={0x48,0x8b,0x05,0,0,0,0,0x48,0x83,0xc0,0x01,0x48,0x89,0x05,0,0,0,0,0xc3};
        const unsigned start=interfaceEndbr?4u:0u;
        if(interfaceEndbr){const unsigned char marker[]={0xf3,0x0f,0x1e,0xfa};memcpy(code,marker,sizeof(marker));}
        code+=start;
        memcpy(code,bytes,sizeof(bytes));
        const auto offset=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+7));
        memcpy(code+3,&offset,4);
        const auto store=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+18));
        memcpy(code+14,&store,4);
        if(interfaceLea){code[1]=0x8d;const unsigned char tail[]={0x48,0xff,0x00,0xc3};memcpy(code+7,tail,sizeof(tail));}
        if(interfaceRip32) {
            const unsigned char narrow[]={0x8b,0x05,0,0,0,0,0x83,0xc0,0x01,0x89,0x05,0,0,0,0,0xc3};memcpy(code,narrow,sizeof(narrow));
            const auto load=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+6));
            const auto store32=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+15));
            memcpy(code+2,&load,4);memcpy(code+11,&store32,4);
        }
        if(interfaceStore32) {
            const unsigned char narrow[]={0x89,0x0d,0,0,0,0,0x48,0xff,0x05,0,0,0,0,0xc3};memcpy(code,narrow,sizeof(narrow));
            const auto store32=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+6));
            const auto increment=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&interfaceDelegations)-reinterpret_cast<std::intptr_t>(code+13));
            memcpy(code+2,&store32,4);memcpy(code+9,&increment,4);interfaceDelegations=23;
        }
        code-=start;
        VirtualProtect(code,64,old,&ignored);FlushInstructionCache(GetCurrentProcess(),code,64);return;
    }
    if(shortPrologue) {
        auto* code=const_cast<unsigned char*>(interfaceFixture);DWORD old{},ignored{};
        const unsigned char bytes[]={0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0,0,0,0,0x48,0x83,0xc4,0x20,0x5b,0xc3};
        Check(VirtualProtect(code,64,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Short prologue writable");
        memcpy(code,bytes,sizeof(bytes));VirtualProtect(code,64,old,&ignored);FlushInstructionCache(GetCurrentProcess(),code,64);return;
    }
    if(fixtureLayout.family!=cge::runtime::Family::SE)return;
    const unsigned char bytes[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x83,0xec,0x20,
        0x48,0x83,0xc4,0x20,0x5f,0x48,0x8b,0x5c,0x24,0x08,0x48,0x8b,0x74,0x24,0x10,0xc3};
    auto* code=const_cast<unsigned char*>(interfaceFixture);DWORD old{},ignored{};
    Check(VirtualProtect(code,sizeof(interfaceFixture),PAGE_EXECUTE_READWRITE,&old)!=FALSE,"SE prologue writable");
    memcpy(code,bytes,sizeof(bytes));VirtualProtect(code,sizeof(interfaceFixture),old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),code,sizeof(interfaceFixture));
}
bool motionUiTaa{};
unsigned char ExchangeInterfaceTaa(unsigned char value);
void InterfaceBoundary(){
    const auto previous=motionUiTaa?ExchangeInterfaceTaa(0):0;
    reinterpret_cast<void(*)(std::int64_t)>(const_cast<unsigned char*>(interfaceFixture))(0);
    if(motionUiTaa)ExchangeInterfaceTaa(previous);
}
#pragma section(".cgeinput",read,execute)
__declspec(allocate(".cgeinput")) const unsigned char inputPollFixture[0x100]{};
__declspec(allocate(".cgeinput")) const unsigned char inputTargetFixture[64]{};
bool inputConflict{};
unsigned inputDelegations{};
const void* expectedEvents{};
struct ButtonFixture {
    void* vtable{};
    std::uint32_t device{},type{};
    ButtonFixture* next{};
    const void* userEvent{};
    std::uint32_t key{},padding{};
    float value{},held{};
};
static_assert(sizeof(ButtonFixture)==0x30);
void NativeInput(void* source,const void* events) {
    Check(source==reinterpret_cast<void*>(0x1234) && events==expectedEvents,"Keyboard observer preserves dispatcher and event list");++inputDelegations;
}
void PrepareInputFixture() {
    auto* code=const_cast<unsigned char*>(inputPollFixture);auto* target=const_cast<unsigned char*>(inputTargetFixture);
    DWORD old{},oldTarget{},ignored{};
    Check(VirtualProtect(code,0x100,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Input fixture writable");
    Check(VirtualProtect(target,64,PAGE_EXECUTE_READWRITE,&oldTarget)!=FALSE,"Input target writable");
    memset(code,0x90,0x100);
    const unsigned char enter[]={0x53,0x48,0x83,0xec,0x20};memcpy(code,enter,sizeof(enter));
    code[0x7b]=inputConflict?0xe9:0xe8;
    const auto offset=static_cast<std::int32_t>(target-(code+0x80));memcpy(code+0x7c,&offset,4);
    const unsigned char leave[]={0x48,0x83,0xc4,0x20,0x5b,0xc3};memcpy(code+0x80,leave,sizeof(leave));
    const unsigned char prefix[]={0x48,0x83,0xec,0x28,0x48,0x83,0xc4,0x28,0xff,0x25,0,0,0,0};
    memcpy(target,prefix,sizeof(prefix));auto* native=reinterpret_cast<void*>(NativeInput);memcpy(target+14,&native,sizeof(native));
    VirtualProtect(code,0x100,old,&ignored);VirtualProtect(target,64,oldTarget,&ignored);
    FlushInstructionCache(GetCurrentProcess(),code,0x100);FlushInstructionCache(GetCurrentProcess(),target,64);
}
void InputBoundary(ButtonFixture* event) {
    expectedEvents=&event;
    reinterpret_cast<void(*)(void*,const void*)>(const_cast<unsigned char*>(inputPollFixture))(reinterpret_cast<void*>(0x1234),&event);
}
#pragma section(".cgeao",read,execute)
__declspec(allocate(".cgeao")) const unsigned char worldFrameFixture[0x890]{};
__declspec(allocate(".cgeao")) const unsigned char worldOpaqueFixture[0x340]{};
__declspec(allocate(".cgeao")) const unsigned char worldTargetFixture[64]{};
__declspec(allocate(".cgeao")) const unsigned char opaqueTargetFixture[64]{};
__declspec(allocate(".cgeao")) const unsigned char decalTargetFixture[64]{};
__declspec(allocate(".cgeao")) const unsigned char decalEntryFixture[64]{};
unsigned worldDelegations{},opaqueDelegations{},decalDelegations{};
void NativeOpaque(void* batch,std::uint32_t start,std::uint32_t end,std::uint32_t flags,int group) {
    Check(batch==reinterpret_cast<void*>(0x1234) && start==3 && end==7 && flags==11 && group==17,"Opaque delegate preserves all five arguments");
    ++opaqueDelegations;
}
void NativeDecals(void* accumulator,std::uint32_t flags) {
    Check(accumulator==reinterpret_cast<void*>(0x5678) && flags==23,"Decal delegate preserves both arguments");++decalDelegations;
}
void NativeWorld(bool value) {
    Check(value,"World delegate preserves native boolean");++worldDelegations;
    using Opaque=void(*)(void*,std::uint32_t,std::uint32_t,std::uint32_t,int);
    reinterpret_cast<Opaque>(const_cast<unsigned char*>(worldOpaqueFixture))(reinterpret_cast<void*>(0x1234),3,7,11,17);
    using Decals=void(*)(void*,std::uint32_t);
    reinterpret_cast<Decals>(const_cast<unsigned char*>(decalEntryFixture))(reinterpret_cast<void*>(0x5678),23);
}
void PrepareOpaqueFixture() {
    auto write=[&](const unsigned char* buffer,std::size_t size,auto fill) {
        auto* memory=const_cast<unsigned char*>(buffer);DWORD old{},ignored{};
        Check(VirtualProtect(memory,size,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Opaque fixture writable");
        memset(memory,0x90,size);fill(memory);VirtualProtect(memory,size,old,&ignored);FlushInstructionCache(GetCurrentProcess(),memory,size);
    };
    auto tail=[&](const unsigned char* target,void* function) {
        write(target,64,[&](unsigned char* memory){
            const unsigned char prefix[]={0x48,0x83,0xec,0x28,0x48,0x83,0xc4,0x28};memcpy(memory,prefix,sizeof(prefix));
            if(ripEntry) {
                const unsigned char load[]={0x48,0x8b,0x05,0,0,0,0,0x90};memcpy(memory,load,sizeof(load));
                const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&worldDelegations)-reinterpret_cast<std::intptr_t>(memory+7));
                memcpy(memory+3,&displacement,4);
            }
            memory[8]=0xff;memory[9]=0x25;memset(memory+10,0,4);memcpy(memory+14,&function,8);
        });
    };
    tail(worldTargetFixture,reinterpret_cast<void*>(NativeWorld));tail(opaqueTargetFixture,reinterpret_cast<void*>(NativeOpaque));
    tail(decalTargetFixture,reinterpret_cast<void*>(NativeDecals));
    auto call=[&](unsigned char* at,const unsigned char* target) {
        at[0]=0xe8;const auto delta=static_cast<std::int32_t>(target-at-5);memcpy(at+1,&delta,4);
    };
    const bool se=fixtureLayout.family==cge::runtime::Family::SE;
    const unsigned frame=se?0x831:fixtureLayout.version>=cge::runtime::Pack(7,99)?0x85e:0x841;
    const unsigned start=se?0x8e:0x84,end=se?0x319:0x308;
    const unsigned char enter[]={0x48,0x83,0xec,0x28},leave[]={0x48,0x83,0xc4,0x28,0xc3};
    write(worldFrameFixture,sizeof(worldFrameFixture),[&](unsigned char* memory) {
        memcpy(memory,enter,sizeof(enter));call(memory+frame,worldTargetFixture);memcpy(memory+frame+5,leave,sizeof(leave));
    });
    write(worldOpaqueFixture,sizeof(worldOpaqueFixture),[&](unsigned char* memory) {
        const unsigned char prefix[]={0x48,0x83,0xec,0x38,0x48,0x8b,0x44,0x24,0x60,0x48,0x89,0x44,0x24,0x20};
        memcpy(memory,prefix,sizeof(prefix));call(memory+start,opaqueTargetFixture);
        const unsigned char finish[]={0x48,0x83,0xc4,0x38,0xc3};memcpy(memory+start+5,finish,sizeof(finish));
        call(memory+end,decalTargetFixture);memcpy(memory+end+5,leave,sizeof(leave));
    });
    write(decalEntryFixture,sizeof(decalEntryFixture),[&](unsigned char* memory) {
        memcpy(memory,enter,sizeof(enter));memory[4]=0xff;memory[5]=0x25;memset(memory+6,0,4);
        const auto* target=worldOpaqueFixture+end;memcpy(memory+10,&target,8);
    });
}
void WorldBoundary() {
    reinterpret_cast<void(*)(bool)>(const_cast<unsigned char*>(worldFrameFixture))(true);
}
#pragma section(".cgehdr",read,execute)
__declspec(allocate(".cgehdr")) const unsigned char tonemapFixture[0x260]{};
__declspec(allocate(".cgehdr")) const unsigned char imageSpaceDrawFixture[64]{};
__declspec(allocate(".cgehdr")) const unsigned char secondTonemapFixture[64]{};
unsigned hdrDelegations{};
void* expectedHdrParameter=reinterpret_cast<void*>(0x1234);
void NativeTonemap(void* manager,std::uintptr_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
    Check(manager==reinterpret_cast<void*>(0x5678) && effect==expectedHdrEffect && source==1 && target==0x29 &&
        parameters==expectedHdrParameter,"HDR delegate preserves all five native arguments");
    ++hdrDelegations;
}
void PrepareTonemapFixture() {
    auto* main=const_cast<unsigned char*>(tonemapFixture);
    auto* draw=const_cast<unsigned char*>(imageSpaceDrawFixture);
    DWORD oldMain{},oldDraw{},ignored{};
    Check(VirtualProtect(main,sizeof(tonemapFixture),PAGE_EXECUTE_READWRITE,&oldMain)!=FALSE,"HDR fixture writable");
    Check(VirtualProtect(draw,sizeof(imageSpaceDrawFixture),PAGE_EXECUTE_READWRITE,&oldDraw)!=FALSE,"Draw fixture writable");
    memset(main,0x90,sizeof(tonemapFixture));
    const unsigned char enter[]={0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0xd9,
        0x48,0x8b,0x44,0x24,0x60,0x48,0x89,0x44,0x24,0x20};
    memcpy(main,enter,sizeof(enter));
    const unsigned char arguments[]={0xba,0x22,0,0,0,0x48,0x8b,0xcb};
    for(unsigned i=0;i<fixtureLayout.callCount;++i) {
        const auto at=fixtureLayout.tonemapCalls[i];
        memcpy(main+at-8,arguments,sizeof(arguments));main[at]=0xe8;
        if(hdrReordered) {
            const unsigned char reordered[]={0x48,0x8b,0xcb,0xba,0x22,0,0,0};memcpy(main+at-8,reordered,sizeof(reordered));
        }
        if(hdrWide)memset(main+at-8,0x90,5);
        const auto offset=static_cast<std::int32_t>(draw-(main+at+5));memcpy(main+at+1,&offset,4);
        const unsigned char leave[]={0x48,0x83,0xc4,0x30,0x5b,0xc3};memcpy(main+at+5,leave,sizeof(leave));
    }
    const unsigned char prefix[]={0x4c,0x8b,0xdc,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70,
        0x48,0x83,0xc4,0x70,0x41,0x5f,0x41,0x5e,0x5f,0x5e,0x5d};
    memcpy(draw,prefix,sizeof(prefix));
    if(shortPrologue) {
        const unsigned char shortCode[]={0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x05,0,0,0,0,0x48,0x83,0xc4,0x30,0x5b};
        memcpy(draw,shortCode,sizeof(shortCode));
        for(unsigned i=17;i<25;++i)draw[i]=0x90;
    }
    if(ripEntry) {
        memset(draw,0x90,25);const unsigned char load[]={0x48,0x8b,0x05,0,0,0,0};memcpy(draw,load,sizeof(load));
        const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&hdrDelegations)-reinterpret_cast<std::intptr_t>(draw+7));
        memcpy(draw+3,&displacement,4);
    }
    draw[25]=0xff;draw[26]=0x25;memset(draw+27,0,4);
    auto* target=reinterpret_cast<void*>(NativeTonemap);memcpy(draw+31,&target,8);
    if(fixtureLayout.callCount==2) {
        auto* second=const_cast<unsigned char*>(secondTonemapFixture);DWORD oldSecond{};
        Check(VirtualProtect(second,64,PAGE_EXECUTE_READWRITE,&oldSecond)!=FALSE,"SE alternate HDR entry writable");
        memcpy(second,enter,sizeof(enter));const auto at=sizeof(enter);
        second[at]=0xff;second[at+1]=0x25;memset(second+at+2,0,4);
        auto* target=main+fixtureLayout.tonemapCalls[1]-8;memcpy(second+at+6,&target,8);
        VirtualProtect(second,64,oldSecond,&ignored);FlushInstructionCache(GetCurrentProcess(),second,64);
    }
    VirtualProtect(main,sizeof(tonemapFixture),oldMain,&ignored);
    VirtualProtect(draw,sizeof(imageSpaceDrawFixture),oldDraw,&ignored);
    FlushInstructionCache(GetCurrentProcess(),main,sizeof(tonemapFixture));
    FlushInstructionCache(GetCurrentProcess(),draw,sizeof(imageSpaceDrawFixture));
}
void TonemapBoundary() {
    using Draw=void(*)(void*,std::uintptr_t,std::uint32_t,std::uint32_t,void*);
    static bool alternate=false;alternate=!alternate;
    auto* code=fixtureLayout.callCount==2 && alternate?secondTonemapFixture:tonemapFixture;
    reinterpret_cast<Draw>(const_cast<unsigned char*>(code))(reinterpret_cast<void*>(0x5678),expectedHdrEffect,1,0x29,expectedHdrParameter);
}
alignas(8) unsigned char playerFixture[0x80]{};
alignas(8) unsigned char cellFixture[0x50]{};
void* playerSingletonFixture=playerFixture;
alignas(8) unsigned char calendarFixture[0x40]{};
alignas(8) unsigned char gameHourFixture[0x38]{};
void* calendarSingletonFixture=calendarFixture;
alignas(8) unsigned char rendererFixture[0x2200]{};
alignas(8) unsigned char uiFixture[0x168]{};
alignas(8) unsigned char imageSpaceFixture[0x1f8]{};
alignas(8) unsigned char temporalFixture[0x20]{};
unsigned char ExchangeInterfaceTaa(unsigned char value){const auto old=temporalFixture[0x18];temporalFixture[0x18]=value;return old;}
void* uiSingletonFixture=uiFixture;
void* imageSpaceSingletonFixture=imageSpaceFixture;

#pragma section(".cgecamera",read,execute)
__declspec(allocate(".cgecamera")) const unsigned char cameraGetterFixture[15]={
    0x48,0x8b,0x05,0,0,0,0,0x48,0x8b,0x80,0x28,0x01,0,0,0xc3};
alignas(8) unsigned char worldRootFixture[0x130]{};
alignas(8) unsigned char cameraFixture[0x188]{};
void* worldRootSingletonFixture=worldRootFixture;
void PrepareCameraFixture() {
    const void* camera=cameraFixture;memcpy(worldRootFixture+0x128,&camera,sizeof(camera));
    const float planes[]={-1,1,0.75f,-0.75f,1,10000};memcpy(cameraFixture+0x150,planes,sizeof(planes));
    const float projection[]={1,0,0,0,0,1.3333333f,0,0,0,0,1.0001f,-1.0001f,0,0,1,0};
    memcpy(cameraFixture+0x110,projection,sizeof(projection));
    auto* code=const_cast<unsigned char*>(cameraGetterFixture);DWORD old{},ignored{};
    Check(VirtualProtect(code,15,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Camera fixture writable");
    const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&worldRootSingletonFixture)-
        reinterpret_cast<std::intptr_t>(code+7));memcpy(code+3,&displacement,4);
    if(cameraVariant || fixtureLayout.family==cge::runtime::Family::SE) {code[2]=0x0d;code[9]=0x81;}
    VirtualProtect(code,15,old,&ignored);FlushInstructionCache(GetCurrentProcess(),code,15);
}
void SetGameHour(float hour) {
    memcpy(gameHourFixture+0x34,&hour,sizeof(hour));
    const void* global=gameHourFixture;memcpy(calendarFixture+0x20,&global,sizeof(global));
}
void SetCell(bool interior) {
    const std::uint16_t flags=interior?1:0;
    memcpy(cellFixture+0x40,&flags,sizeof(flags));
    const void* cell=cellFixture;memcpy(playerFixture+0x60,&cell,sizeof(cell));
}
unsigned char skyFixture[0x2c8]{},weatherFixture[0x680]{};
__declspec(noinline) void* SkyGetterFixture(){return skyFixture;}
void SetWeather(unsigned flags,float progress=1) {
    weatherFixture[0x1a]=0x36;weatherFixture[0x66f]=static_cast<unsigned char>(flags);
    const auto* weather=weatherFixture;memcpy(skyFixture+0x48,&weather,sizeof(weather));
    memcpy(skyFixture+0x1b8,&progress,sizeof(progress));
}
void WriteLibrary(const fs::path& path) {
    SetWeather(1);
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::pair<cge::runtime::Address,const void*> values[]={
        {cge::runtime::Address::SkyGetter,reinterpret_cast<const void*>(SkyGetterFixture)},
        {cge::runtime::Address::InputDispatch,inputPollFixture},
        {cge::runtime::Address::WorldOpaque,worldOpaqueFixture},{cge::runtime::Address::WorldFrame,worldFrameFixture},
        {cge::runtime::Address::Interface,interfaceFixture},{cge::runtime::Address::Calendar,&calendarSingletonFixture},
        {cge::runtime::Address::Player,&playerSingletonFixture},{cge::runtime::Address::CameraGetter,cameraGetterFixture},
        {cge::runtime::Address::WorldRoot,&worldRootSingletonFixture},{cge::runtime::Address::Tonemap,tonemapFixture},
        {cge::runtime::Address::Renderer,rendererFixture},{cge::runtime::Address::ImageSpace,&imageSpaceSingletonFixture},
        {cge::runtime::Address::UI,&uiSingletonFixture}};
    std::vector<std::pair<std::uint32_t,std::uint32_t>> entries;
    for(auto [key,address]:values)entries.emplace_back(fixtureLayout.Id(key),static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address)-base));
    if(fixtureLayout.Id(cge::runtime::Address::Draw))entries.emplace_back(fixtureLayout.Id(cge::runtime::Address::Draw),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(imageSpaceDrawFixture)-base));
    std::ofstream file(path,std::ios::binary);
    if(fixtureLayout.version>=cge::runtime::Pack(7,99)) {
        std::vector<std::uint32_t> library(24+414661);
        library[0]=5;library[1]=1;library[2]=(fixtureLayout.version>>16)&255;library[3]=(fixtureLayout.version>>4)&4095;
        memcpy(reinterpret_cast<char*>(library.data())+20,"SkyrimSE.exe",12);library[21]=8;library[23]=414661;
        for(auto [id,offset]:entries)library[24+id]=offset;
        file.write(reinterpret_cast<const char*>(library.data()),static_cast<std::streamsize>(library.size()*4));
    }else {
        const std::uint32_t header[]={fixtureLayout.family==cge::runtime::Family::SE?1u:2u,1,
            (fixtureLayout.version>>16)&255,(fixtureLayout.version>>4)&4095,0,12};
        file.write(reinterpret_cast<const char*>(header),sizeof(header));file.write("SkyrimSE.exe",12);
        const std::uint32_t tail[]={8,static_cast<std::uint32_t>(entries.size())};
        file.write(reinterpret_cast<const char*>(tail),sizeof(tail));
        for(auto [id,offset]:entries) {
            file.put(0);
            const std::uint64_t pair[]={id,offset};file.write(reinterpret_cast<const char*>(pair),sizeof(pair));
        }
    }
    Check(bool(file),"Write Address Library fixture");
}

void(*listener)(cge::skse::Message*){};
unsigned handleCalls{};
bool Listen(std::uint32_t handle,const char* sender,void(*callback)(cge::skse::Message*)) {
    Check(handle==7 && !strcmp(sender,"SKSE"),"SKSE message subscription");listener=callback;return true;
}
cge::skse::Messaging messaging{2,Listen,nullptr,nullptr};
bool legacyApi{};
struct LegacyMessaging {std::uint32_t version;bool(*registerListener)(std::uint32_t,const char*,void(*)(cge::skse::Message*));};
LegacyMessaging legacyMessaging{1,Listen};
void* Query(std::uint32_t id){return id==5?(legacyApi?static_cast<void*>(&legacyMessaging):static_cast<void*>(&messaging)):nullptr;}
std::uint32_t Handle(){++handleCalls;return 7;}
int wmain(int argc,wchar_t** argv) {
    Check(argc==5,"DLL, fixture, mode and test framework");
    const std::wstring mode=argv[3];
    inputConflict=mode==L"--input-hook-conflict";
    const bool gogRuntime=mode.starts_with(L"--runtime-gog-");
    if(gogRuntime) {
        unsigned minor{},revision{};
        Check(swscanf_s(mode.c_str(),L"--runtime-gog-%u-%u",&minor,&revision)==2,"Parse GOG runtime case");
        const auto selected=cge::runtime::Select(cge::runtime::Pack(minor,revision)|1u);
        Check(selected.has_value(),"Supported marked GOG runtime");fixtureLayout=*selected;
    }else if(mode.starts_with(L"--runtime-")) {
        unsigned minor{},revision{};
        Check(swscanf_s(mode.c_str(),L"--runtime-%u-%u",&minor,&revision)==2,"Parse runtime case");
        const auto selected=cge::runtime::Select(cge::runtime::Pack(minor,revision));
        Check(selected.has_value(),"Supported fixture runtime");fixtureLayout=*selected;
    }
    interfaceRip=mode==L"--interface-rip";interfaceLea=mode==L"--interface-lea";
    interfaceRip32=mode==L"--interface-rip32";interfaceStore32=mode==L"--interface-store32";interfaceEndbr=mode==L"--interface-endbr";
    if(interfaceRip || interfaceLea || interfaceRip32 || interfaceStore32 || interfaceEndbr)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(6,1170));
    motionUiTaa=mode==L"--motion-ui-taa";
    if(motionUiTaa)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(6,1170));
    hdrReordered=mode==L"--hdr-reordered";hdrWide=mode==L"--hdr-wide-argument";hdrMismatch=mode==L"--hdr-native-mismatch";
    if(hdrWide)expectedHdrEffect=0x1234567800000022ull;
    if(hdrReordered || hdrWide || hdrMismatch)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(6,1170));
    ripEntry=mode==L"--rip-entry";cameraVariant=mode==L"--camera-variant";
    if(ripEntry || cameraVariant)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(6,1170));
    shortPrologue=mode==L"--short-prologue";
    if(shortPrologue)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(6,1170));
    legacyApi=mode==L"--legacy-api";
    if(legacyApi)fixtureLayout=*cge::runtime::Select(cge::runtime::Pack(5,3));
    const bool missing=mode==L"--missing-framework",old=mode==L"--old-framework",
        incomplete=mode==L"--incomplete-framework",sceneMissing=mode==L"--missing-library",
        invalidLibrary=mode==L"--invalid-library",sceneConflict=mode==L"--hook-conflict",
        hdrConflict=mode==L"--hdr-hook-conflict" || hdrMismatch,opaqueConflict=mode==L"--opaque-hook-conflict";
    const bool frameworkReady=!missing && !old && !incomplete;
    const bool sceneReady=!sceneMissing && !invalidLibrary && !sceneConflict;
    const auto root=fs::absolute(argv[2])/L"SKSE"/L"Plugins"/L"CGE-\u03b3";
    fs::create_directories(root);fs::copy_file(argv[1],root/"CGE.dll",fs::copy_options::overwrite_existing);
    fs::remove(root/"CGE.log");fs::remove(root/"CGE-loader.log");
    auto config=root/"CGE.ini";
    fs::create_directories(root/"CGE"/"LUTs");
    fs::copy_file(fs::path(argv[1]).parent_path()/"Warm.cube",root/"CGE"/"LUTs"/"Warm.cube",fs::copy_options::overwrite_existing);
    {std::ofstream f(root/"CGE"/"LUTs"/"Test.cube");f<<"LUT_3D_SIZE 2\n0 0 0\n0.5 0 0\n0 1 0\n0.5 1 0\n0 0 1\n0.5 0 1\n0 1 1\n0.5 1 1\n";}
    Write(config,"[Color]\nExposureStops=-1\n[Input]\nToggleKey=119\n");
    PrepareInputFixture();PrepareInterfaceFixture();PrepareTonemapFixture();PrepareOpaqueFixture();PrepareCameraFixture();const std::uint32_t formId=0x14;memcpy(playerFixture+0x14,&formId,4);
    SetCell(false);SetGameHour(12);
    const void* temporal=temporalFixture;memcpy(imageSpaceFixture+0x1f0,&temporal,sizeof(temporal));temporalFixture[0x18]=1;
    const auto library=root/fixtureLayout.LibraryName();WriteLibrary(library);
    if(sceneMissing)fs::remove(library);
    if(invalidLibrary)Write(library,"invalid");
    auto corrupt=[](const unsigned char* address){
        DWORD oldProtection{},ignored{};auto* p=const_cast<unsigned char*>(address);
        Check(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&oldProtection)!=FALSE,"Fixture writable");
        *p=0x90;VirtualProtect(p,1,oldProtection,&ignored);
    };
    if(sceneConflict)corrupt(interfaceFixture);
    if(hdrConflict && !hdrMismatch)corrupt(tonemapFixture+fixtureLayout.tonemapCalls[0]);
    if(hdrMismatch) {
        auto* call=const_cast<unsigned char*>(tonemapFixture)+fixtureLayout.tonemapCalls[0];DWORD oldProtection{},ignored{};
        Check(VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&oldProtection)!=FALSE,"Native HDR mismatch fixture writable");
        const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(opaqueTargetFixture)-reinterpret_cast<std::intptr_t>(call+5));
        memcpy(call+1,&displacement,4);VirtualProtect(call,5,oldProtection,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
    }
    if(opaqueConflict)corrupt(worldOpaqueFixture+(fixtureLayout.family==cge::runtime::Family::SE?0x8e:0x84));
    const auto frameOffset=fixtureLayout.family==cge::runtime::Family::SE?0x831:fixtureLayout.version>=cge::runtime::Pack(7,99)?0x85e:0x841;
    unsigned char savedWorldCall[5];memcpy(savedWorldCall,worldFrameFixture+frameOffset,5);
    auto module=LoadLibraryExW((root/"CGE.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"Load production CGE.dll");
    Check(fs::exists(root/"CGE-loader.log"),"Bootstrap log exists before SKSE Load is called");
    auto load=Proc<bool(*)(const cge::skse::Interface*)>(module,"SKSEPlugin_Load");
    cge::skse::Interface api{0x02030010,fixtureLayout.version,0,0,Query,Handle};
    if(gogRuntime)api.skseVersion=((fixtureLayout.version>>4)&4095)==659?0x02020030:0x02020060;
    if(shortPrologue)api.skseVersion=0x02020080;
    if(motionUiTaa || interfaceRip || interfaceLea || interfaceRip32 || interfaceStore32 || interfaceEndbr)api.skseVersion=0x02020060;
    const cge::skse::Interface* apiView=&api;
    if(legacyApi) {
        api.skseVersion=0x02000020;
        SYSTEM_INFO system{};GetSystemInfo(&system);
        auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,system.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        Check(memory!=nullptr,"Legacy ABI allocation");DWORD oldProtection{};
        Check(VirtualProtect(memory+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&oldProtection)!=FALSE,"Legacy ABI guard page");
        constexpr auto size=offsetof(cge::skse::Interface,getReleaseIndex);
        auto* prefix=memory+system.dwPageSize-size;memcpy(prefix,&api,size);
        apiView=reinterpret_cast<const cge::skse::Interface*>(prefix);
    }
    auto wrong=api;wrong.runtimeVersion=cge::runtime::Pack(7,105);
    Check(!load(&wrong) && !load(nullptr),"Unsupported runtime refused");
    auto query=Proc<bool(*)(const cge::skse::Interface*,cge::skse::Info*)>(module,"SKSEPlugin_Query");
    cge::skse::Info info{};
    Check(query(apiView,&info) && info.version==cge::skse::Version && !query(&wrong,&info),"Legacy SKSE Query uses selected runtime");
    const auto* metadata=Proc<const cge::skse::Metadata*>(module,"SKSEPlugin_Version");
    Check(metadata->versionIndependenceEx==2 && !metadata->minimumSkse,"V5 support with no fixed SKSE package version");
    if(fixtureLayout.family==cge::runtime::Family::AE) {
        bool listed=false;for(auto value:metadata->compatibleVersions)if(value==fixtureLayout.version)listed=true;
        Check(listed,"AE runtime is declared in SKSE loader metadata");
    }
    Check(load(apiView) && load(apiView) && listener && handleCalls==1,"Creation hook and one SKSE listener");
    HMODULE framework{};
    using Render=bool(*)(const char*);
    Render render{};
    void(*action)(int,const char*,float){};
    void(*blocking)(bool){};
    unsigned(*pageCount)(){};
    if(!missing) {
        fs::copy_file(argv[4],root/"SKSEMenuFramework.dll",fs::copy_options::overwrite_existing);
        framework=LoadLibraryExW((root/"SKSEMenuFramework.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(framework!=nullptr,"Load test framework after CGE");
        pageCount=Proc<unsigned(*)()>(framework,"TestPageCount");
        action=Proc<void(*)(int,const char*,float)>(framework,"TestAction");
        render=Proc<Render>(framework,"TestRender");
        blocking=Proc<void(*)(bool)>(framework,"TestSetBlocking");
        if(old)Proc<void(*)(float)>(framework,"TestSetVersion")(2.0f);
        Check(pageCount()==0,"No premature registration during plugin load");
    }
    cge::skse::Message message{"SKSE",0,0,nullptr};listener(&message);
    if(pageCount)Check(pageCount()==0,"Wait for PostPostLoad");
    message.type=1;listener(&message);listener(&message);
    if(pageCount)Check(pageCount()==(frameworkReady?22u:0u),"Validate API before registering exactly twenty-two pages");
    if(frameworkReady) {
        const auto index=Proc<int(*)(const char*)>(framework,"TestPageIndex");
        Check(index("Community Graphics Engine/Color/Lift Gamma Gain")==index("Community Graphics Engine/Color/Color")+1,"RGB grading follows Color");
        Check(index("Community Graphics Engine/Effects/Lens Dirt")==index("Community Graphics Engine/Effects/Bloom")+1,"Lens Dirt follows Bloom");
        Check(index("Community Graphics Engine/Effects/Sharpness")+1==index("Community Graphics Engine/Effects/Clarity") &&
            index("Community Graphics Engine/Effects/Film Grain")==index("Community Graphics Engine/Effects/Clarity")+1,"Detail pages follow the requested order");
        for(const char* path:{"Community Graphics Engine/Color/3D LUT","Community Graphics Engine/Color/Vibrance","Community Graphics Engine/Color/Split Toning","Community Graphics Engine/Color/Vignette","Community Graphics Engine/Color/Debanding"})Check(index(path)>=0,"Color effects grouped together");
    }
    const char* shared="Community Graphics Engine/Color/Color";
    const char* bloom="Community Graphics Engine/Effects/Bloom";
    if(frameworkReady)Check(render(shared),"Page callback tolerates missing graphics device");
    HWND window=CreateWindowW(L"STATIC",L"CGE SMF test",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"Hidden window");
    const auto originalWindowProc=GetWindowLongPtrW(window,GWLP_WNDPROC);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_SEQUENTIAL;
    ComPtr<IDXGISwapChain> chain;ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,&chain,&device,nullptr,&context)),"WARP device through production IAT hook");
    Check(GetWindowLongPtrW(window,GWLP_WNDPROC)==originalWindowProc,"CGE never replaces the game's window procedure");
    ComPtr<ID3D11Texture2D> buffer,readback;ComPtr<ID3D11RenderTargetView> output;
    auto buffers=[&] {
        Check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))),"Backbuffer");
        device->CreateRenderTargetView(buffer.Get(),nullptr,&output);
        D3D11_TEXTURE2D_DESC d{};buffer->GetDesc(&d);d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"LDR staging");
    };buffers();
    ComPtr<ID3D11Texture2D> hdr,hdrReadback,velocity,depth;
    ComPtr<ID3D11ShaderResourceView> hdrView,velocityView,depthView;
    D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=64;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32G32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&d,nullptr,&velocity);device->CreateShaderResourceView(velocity.Get(),nullptr,&velocityView);
    std::vector<float> data(64*64*2,0.01f);context->UpdateSubresource(velocity.Get(),0,nullptr,data.data(),64*8,0);
    d.Format=DXGI_FORMAT_R32_FLOAT;device->CreateTexture2D(&d,nullptr,&depth);
    device->CreateShaderResourceView(depth.Get(),nullptr,&depthView);
    data.assign(64*64,0.5f);context->UpdateSubresource(depth.Get(),0,nullptr,data.data(),64*4,0);
    auto* v=velocityView.Get();auto* z=depthView.Get();
    memcpy(rendererFixture+0xbc0,&v,sizeof(v));memcpy(rendererFixture+fixtureLayout.depthView,&z,sizeof(z));
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags|=D3D11_BIND_RENDER_TARGET;
    device->CreateTexture2D(&d,nullptr,&hdr);device->CreateShaderResourceView(hdr.Get(),nullptr,&hdrView);
    auto* h=hdrView.Get();memcpy(rendererFixture+0xaa0,&h,sizeof(h));
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    device->CreateTexture2D(&d,nullptr,&hdrReadback);
    std::vector<float> hdrPixels(64*64*4,0.05f);
    for(unsigned y=30;y<34;++y)for(unsigned x=30;x<34;++x)for(unsigned c=0;c<3;++c)hdrPixels[(y*64+x)*4+c]=8;
    bool nativeResourcesValid=true,clarityPattern=false,aberrationPattern=false,bandingPattern=false;
    auto present=[&] {
        const float color[]={0.8f,0.4f,0.2f,0.3f};context->ClearRenderTargetView(output.Get(),color);
        if(clarityPattern) {
            std::vector<unsigned char> pixels(64*64*4);
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
                const auto at=(y*64+x)*4;
                pixels[at]=pixels[at+1]=pixels[at+2]=x>=31 && x<=32?145:120;pixels[at+3]=77;
            }
            context->UpdateSubresource(buffer.Get(),0,nullptr,pixels.data(),64*4,0);
        }
        if(aberrationPattern || bandingPattern) {
            std::vector<unsigned char> pixels(64*64*4);
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
                const auto at=(y*64+x)*4;const auto v=static_cast<unsigned char>(bandingPattern?40+(x/8)*8:(x==6 || x==57?240:30));
                pixels[at]=pixels[at+1]=pixels[at+2]=v;pixels[at+3]=77;
            }
            context->UpdateSubresource(buffer.Get(),0,nullptr,pixels.data(),64*4,0);
        }
        auto* rt=output.Get();context->OMSetRenderTargets(1,&rt,nullptr);
        D3D11_VIEWPORT sentinel{3,4,19,20,0,1};context->RSSetViewports(1,&sentinel);
        if(nativeResourcesValid) {
            context->UpdateSubresource(hdr.Get(),0,nullptr,hdrPixels.data(),64*16,0);
            if(sceneReady && !opaqueConflict)WorldBoundary();
            if(!hdrConflict){TonemapBoundary();TonemapBoundary();}
        }
        if(sceneReady){InterfaceBoundary();InterfaceBoundary();}
        const unsigned char hud[]={17,231,93,77};D3D11_BOX box{0,0,0,1,1,1};
        context->UpdateSubresource(buffer.Get(),0,&box,hud,4,0);
        Check(SUCCEEDED(chain->Present(0,0)),"Present without CGE overlay");
        UINT count=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&count,&viewport);
        Check(count==1 && viewport.TopLeftX==3 && viewport.Width==19,"Restore native viewport");
        ComPtr<ID3D11RenderTargetView> actual;context->OMGetRenderTargets(1,&actual,nullptr);
        Check(actual.Get()==output.Get(),"Restore native output binding");
    };
    auto pixel=[&](int red) {
        context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"LDR readback");
        const auto* bytes=static_cast<unsigned char*>(map.pData);
        if(abs(int(bytes[32*map.RowPitch+32*4])-red)>2)std::printf("Pixel: got %u, expected %d\n",unsigned(bytes[32*map.RowPitch+32*4]),red);
        Check(abs(int(bytes[32*map.RowPitch+32*4])-red)<=2,"Effects still precede native HUD");
        Check(bytes[0]==17 && bytes[1]==231 && bytes[2]==93 && bytes[3]==77,"HUD remains untouched");
        context->Unmap(readback.Get(),0);
    };
    auto image=[&]() {
        context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"Native image readback");
        std::vector<unsigned char> result(64*64*4);for(unsigned y=0;y<64;++y)memcpy(result.data()+y*64*4,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,64*4);
        context->Unmap(readback.Get(),0);Check(result[0]==17 && result[1]==231 && result[2]==93,"Native effect test preserves HUD");return result;
    };
    auto hdrPixel=[&](unsigned x) {
        context->CopyResource(hdrReadback.Get(),hdr.Get());D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(hdrReadback.Get(),0,D3D11_MAP_READ,0,&map)),"HDR readback");
        const auto* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(map.pData)+32*map.RowPitch);
        const float value=row[x*4];context->Unmap(hdrReadback.Get(),0);return value;
    };
    auto halo=[&]{return hdrPixel(37);};
    auto edit=[&](const char* page,int type,const char* name,float number) {
        action(type,name,number);Check(render(page),"Registered page callback");
    };
    present();pixel(sceneReady?102:204);
    if(sceneReady && !inputConflict) {
        ButtonFixture key;key.key=66;key.value=1;
        const auto count=inputDelegations;
        InputBoundary(&key);present();pixel(204);
        Check(!cge::Settings::Load(config).enabled,"F8 disables and saves the master switch");
        InputBoundary(&key);Check(!cge::Settings::Load(config).enabled,"Repeated key-down does not toggle twice");
        key.held=1;InputBoundary(&key);Check(!cge::Settings::Load(config).enabled,"Held key does not toggle");
        key.value=0;InputBoundary(&key);key.value=1;key.held=0;InputBoundary(&key);present();pixel(102);
        Check(cge::Settings::Load(config).enabled && cge::Settings::Load(config).exposure==-1,"Second press restores effects without changing controls");
        key.value=0;key.held=1;InputBoundary(&key);
        key.device=1;key.value=1;key.held=0;InputBoundary(&key);
        Check(cge::Settings::Load(config).enabled,"Mouse buttons do not activate keyboard hotkey");
        key.device=0;key.type=2;InputBoundary(&key);Check(cge::Settings::Load(config).enabled,"Text events do not activate keyboard hotkey");
        key.type=0;key.key=65;InputBoundary(&key);Check(cge::Settings::Load(config).enabled,"Other keys do not toggle effects");
        key.value=0;key.held=1;InputBoundary(&key);key.key=66;key.value=1;key.held=0;
        std::uint32_t paused=1;memcpy(uiFixture+0x160,&paused,4);InputBoundary(&key);
        Check(cge::Settings::Load(config).enabled,"Paused game suppresses hotkey");
        paused=0;memcpy(uiFixture+0x160,&paused,4);key.value=0;key.held=1;InputBoundary(&key);
        if(frameworkReady) {
            blocking(true);key.value=1;key.held=0;InputBoundary(&key);
            Check(cge::Settings::Load(config).enabled,"Framework numeric input suppresses hotkey");
            blocking(false);key.value=0;key.held=1;InputBoundary(&key);
            const char* page="Community Graphics Engine/General";
            edit(page,4,"##ToggleKey",8);edit(page,5,"Save",0);
            Check(cge::Settings::Load(config).toggleScanCode==67,"Menu saves remapped F9 hotkey");
            key.value=1;key.held=0;InputBoundary(&key);Check(cge::Settings::Load(config).enabled,"Old hotkey stops after remapping");
            key.value=0;key.held=1;InputBoundary(&key);key.key=67;key.value=1;key.held=0;InputBoundary(&key);
            Check(!cge::Settings::Load(config).enabled,"Remapped F9 toggles effects");
            key.value=0;key.held=1;InputBoundary(&key);key.value=1;key.held=0;InputBoundary(&key);
            key.value=0;key.held=1;InputBoundary(&key);
            edit(page,4,"##ToggleKey",0);edit(page,5,"Save",0);key.value=1;key.held=0;InputBoundary(&key);
            Check(cge::Settings::Load(config).enabled && cge::Settings::Load(config).toggleScanCode==0,"Disabled hotkey does not toggle");
            key.value=0;key.held=1;InputBoundary(&key);edit(page,4,"##ToggleKey",7);edit(page,5,"Save",0);
        }
        InputBoundary(nullptr);Check(inputDelegations>count,"Every observed event and empty list reaches native dispatch");
    }
    if(frameworkReady) {
        edit(shared,3,"Exposure",0);present();pixel(204);
        edit(bloom,1,"Bloom enabled",1);edit(bloom,2,"Threshold",0.5f);present();
        Check(hdrConflict || !sceneReady || halo()>0.05f,"Framework controls operate production HDR bloom");
        edit(bloom,2,"Threshold",99);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).bloom.threshold==1,"Framework bloom slider clamps to 0..1");
        edit(bloom,3,"Threshold",0.5f);
        edit(bloom,3,"Intensity",0.75f);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).bloom.intensity==0.75f,"Framework numeric input persists");
        const char* lensDirtPage="Community Graphics Engine/Effects/Lens Dirt";
        present();std::vector<float> cleanHalo(64);for(unsigned x=0;x<64;++x)cleanHalo[x]=hdrPixel(x);
        edit(lensDirtPage,1,"Lens Dirt enabled",1);edit(lensDirtPage,3,"Strength",1);edit(lensDirtPage,3,"Light Threshold",0.01f);present();pixel(204);
        float dirtDelta=0;for(unsigned x=0;x<64;++x)dirtDelta=std::max(dirtDelta,hdrPixel(x)-cleanHalo[x]);
        Check(hdrConflict || !sceneReady || dirtDelta>0.000001f,"Framework Lens Dirt affects native HDR bloom before HUD");
        edit(lensDirtPage,1,"Lens Dirt enabled",0);present();
        if(sceneReady && !hdrConflict)for(unsigned x=0;x<64;++x)Check(std::abs(hdrPixel(x)-cleanHalo[x])<0.00001f,"Dirt toggle restores original bloom");
        edit(bloom,1,"Bloom enabled",0);present();Check(halo()==0.05f,"Framework bloom switch bypasses HDR");
        const char* ssao="Community Graphics Engine/Effects/SSAO";
        edit(ssao,1,"SSAO enabled",1);edit(ssao,3,"Strength",1);edit(ssao,3,"Radius (units)",80);
        edit(ssao,4,"##Quality",2);edit(ssao,5,"Save",0);
        Check(cge::Settings::Load(config).ssao.quality==2 && cge::Settings::Load(config).ssao.radius==80,"SSAO quality and controls persist");
        if(sceneReady && !hdrConflict) {
            auto encodeDepth=[](float z){return 10000*(z-1)/(z*9999);};
            std::vector<float> aoDepth(64*64,encodeDepth(300));
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<32;++x)aoDepth[y*64+x]=encodeDepth(275);
            context->UpdateSubresource(depth.Get(),0,nullptr,aoDepth.data(),64*4,0);
            temporalFixture[0x18]=0;present();pixel(204);
            if(!opaqueConflict) {
                auto temporalMessages=[&] {
                    std::ifstream file(root/"CGE.log");std::string contents((std::istreambuf_iterator<char>(file)),{});
                    std::size_t at=0;unsigned count=0;while((at=contents.find("CGE SSAO temporal:",at))!=std::string::npos){++count;++at;}return count;
                };
                const auto messages=temporalMessages();for(unsigned frame=0;frame<16;++frame)present();
                Check(temporalMessages()==messages,"Stable SSAO does not write a diagnostic on every frame");
            }
            Check(hdrPixel(35)==0.05f && hdrPixel(15)==0.05f,"HDR SSAO multiplication is removed; occlusion belongs to opaque material lighting");
            const float invalidSide=0;memcpy(cameraFixture+0x150,&invalidSide,4);memcpy(cameraFixture+0x154,&invalidSide,4);
            present();Check(hdrPixel(35)==0.05f,"Invalid native frustum bypasses SSAO");
            const float sides[]={-1,1};memcpy(cameraFixture+0x150,sides,sizeof(sides));
            present();Check(hdrPixel(35)==0.05f,"SSAO never falls back to post-lighting dimming");
            data.assign(64*64,0.5f);context->UpdateSubresource(depth.Get(),0,nullptr,data.data(),64*4,0);
            temporalFixture[0x18]=1;
            for(unsigned type:{2u,3u,7u}) {
                cge::skse::Message resetMessage{"SKSE",type,0,nullptr};listener(&resetMessage);present();
            }
        }
        edit(shared,5,"Reset Color",0);edit(ssao,5,"Save",0);
        Check(cge::Settings::Load(config).ssao.enabled,"Reset Color preserves SSAO");
        edit(ssao,5,"Reset SSAO",0);edit(ssao,5,"Save",0);Check(!cge::Settings::Load(config).ssao.enabled,"SSAO reset disables only this effect");
        const char* general="Community Graphics Engine/General";
        auto hudFrame=Proc<unsigned(*)()>(framework,"TestHudFrame");
        auto hudText=Proc<const char*(*)()>(framework,"TestHudText");
        Check(hudFrame()==0,"FPS HUD disabled by default");
        edit(general,1,"Show FPS",1);edit(general,5,"Save",0);
        Check(cge::Settings::Load(config).showFps,"General FPS toggle persists");
        present();Sleep(510);present();
        Check(hudFrame()==2 && std::string(hudText()).starts_with("FPS: "),"Framework HUD draws FPS with shadow while menu is closed");
        Check(Proc<float(*)()>(framework,"TestHudX")()==12 && Proc<float(*)()>(framework,"TestHudY")()==12,"FPS counter is anchored top left");
        edit(general,1,"Effects enabled",0);Check(hudFrame()==2,"FPS HUD remains independent of graphics effects");
        edit(general,1,"Effects enabled",1);edit(general,1,"Show FPS",0);edit(general,5,"Save",0);
        Check(hudFrame()==0 && !cge::Settings::Load(config).showFps,"General switch hides and persists FPS HUD");
        const char* profiles="Community Graphics Engine/Profiles";
        edit(profiles,1,"Automatic profiles",1);
        edit(shared,4,"##Profile",1);edit(shared,3,"Exposure",0.5f);
        edit(bloom,4,"##Profile",3);edit(bloom,1,"Bloom enabled",1);edit(bloom,3,"Intensity",0.9f);
        edit(profiles,3,"Dawn (hour)",7);edit(profiles,3,"Dusk (hour)",21);edit(general,5,"Save",0);
        auto saved=cge::Settings::Load(config);
        Check(saved.interior.exposure==0.5f && saved.exteriorNight.bloom.intensity==0.9f &&
            saved.dayHour==7 && saved.nightHour==21,"Framework persists independent profiles and timing");
        edit(profiles,1,"Automatic profiles",0);
        edit(shared,4,"##Profile",1);edit(shared,3,"Exposure",2);
        edit(general,5,"Save",0);
        Check(cge::Settings::Load(config).interior.exposure==0.5f,"Inactive automatic profile controls are disabled");
        edit(shared,4,"##Profile",0);
        edit(shared,3,"Exposure",std::numeric_limits<float>::quiet_NaN());edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).exposure==0,"Non-finite framework input is rejected");
        const char* fog="Community Graphics Engine/Effects/Distance Fog";
        edit(fog,4,"##Profile",0);
        edit(fog,1,"Fog enabled",1);edit(fog,3,"Density",99);
        edit(fog,3,"Start Distance (units)",99999);edit(fog,5,"Save",0);
        auto savedFog=cge::Settings::Load(config);
        Check(savedFog.fog.enabled && savedFog.fog.density==1 && savedFog.fog.startDistance==50000,"Fog page bounds and save");
        edit(fog,3,"Start Distance (units)",0);
        std::vector<float> farDepth(64*64,0.9999f);
        context->UpdateSubresource(depth.Get(),0,nullptr,farDepth.data(),64*4,0);
        temporalFixture[0x18]=0;present();
        if(sceneReady && !hdrConflict) {
            Check(halo()>0.2f,"Fog uses native depth while TAA and motion blur are disabled");
            edit(general,1,"Effects enabled",0);present();Check(halo()==0.05f,"Master checkbox bypasses fog");
            edit(general,1,"Effects enabled",1);
            void* invalid=reinterpret_cast<void*>(1);memcpy(rendererFixture+fixtureLayout.depthView,&invalid,sizeof(invalid));
            present();Check(halo()==0.05f,"Invalid native depth pointer bypasses fog");
            auto* valid=depthView.Get();memcpy(rendererFixture+fixtureLayout.depthView,&valid,sizeof(valid));
            float invalidNear=0;memcpy(cameraFixture+0x160,&invalidNear,4);present();
            Check(halo()==0.05f,"Invalid native frustum bypasses fog");
            const float nearPlane=1;memcpy(cameraFixture+0x160,&nearPlane,4);
            cameraFixture[0x168]=1;present();Check(halo()==0.05f,"Orthographic camera bypasses perspective fog");cameraFixture[0x168]=0;
            playerSingletonFixture=nullptr;present();Check(halo()==0.05f,"Missing player cell bypasses fog");
            playerSingletonFixture=playerFixture;present();Check(halo()>0.2f,"Fog recovers after valid native inputs return");
        }
        temporalFixture[0x18]=1;
        edit(profiles,1,"Automatic profiles",1);
        edit(fog,4,"##Profile",3);edit(fog,1,"Fog enabled",1);edit(fog,3,"Density",0.8f);edit(fog,3,"Start Distance (units)",500);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).exteriorNight.fog.density==0.8f,"Reset Color preserves the selected profile's fog");
        edit(fog,5,"Reset Fog",0);edit(fog,5,"Save",0);
        Check(!cge::Settings::Load(config).exteriorNight.fog.enabled &&
            cge::Settings::Load(config).fog.enabled,"Fog reset affects only the selected profile");
        edit(profiles,1,"Automatic profiles",0);
        edit(fog,4,"##Profile",0);edit(fog,1,"Fog enabled",0);edit(fog,5,"Save",0);
        context->UpdateSubresource(depth.Get(),0,nullptr,data.data(),64*4,0);
        const char* dof="Community Graphics Engine/Effects/Depth of Field";
        edit(dof,4,"##Profile",0);edit(dof,1,"Depth of field enabled",1);
        edit(dof,3,"Focus Distance (units)",-1);edit(dof,3,"Blur Strength",99);edit(dof,5,"Save",0);
        auto savedDof=cge::Settings::Load(config);
        Check(savedDof.dof.enabled && savedDof.dof.focusDistance==1 && savedDof.dof.strength==1,"Depth-of-field page bounds and save");
        edit(dof,3,"Focus Distance (units)",500);edit(dof,3,"Blur Strength",0.75f);edit(dof,5,"Save",0);
        savedDof=cge::Settings::Load(config);
        Check(savedDof.dof.focusDistance==500 && savedDof.dof.strength==0.75f,"Manual focus and strength persist");
        edit(dof,1,"Depth of field (all profiles)",0);edit(dof,5,"Save",0);
        Check(!cge::Settings::Load(config).dofAllowed && cge::Settings::Load(config).dof.enabled,"Global DOF off preserves per-profile settings");
        context->UpdateSubresource(depth.Get(),0,nullptr,farDepth.data(),64*4,0);present();
        if(sceneReady && !hdrConflict)Check(hdrPixel(32)==8,"Global DOF off immediately restores sharp scene");
        edit(dof,1,"Depth of field (all profiles)",1);
        context->UpdateSubresource(depth.Get(),0,nullptr,farDepth.data(),64*4,0);
        temporalFixture[0x18]=0;present();
        if(sceneReady && !hdrConflict) {
            Check(hdrPixel(32)<7,"Native depth of field blurs the HDR background without fog, TAA or motion blur");
            edit(dof,3,"Focus Distance (units)",50000);present();Check(hdrPixel(32)==8,"Native manual focus can restore sharp HDR highlights");
            edit(dof,3,"Focus Distance (units)",500);
            edit(general,1,"Effects enabled",0);present();Check(hdrPixel(32)==8,"Master checkbox bypasses depth of field");
            edit(general,1,"Effects enabled",1);
            void* invalid=reinterpret_cast<void*>(1);memcpy(rendererFixture+fixtureLayout.depthView,&invalid,sizeof(invalid));
            present();Check(hdrPixel(32)==8,"Missing native depth safely bypasses depth of field");
            auto* valid=depthView.Get();memcpy(rendererFixture+fixtureLayout.depthView,&valid,sizeof(valid));
            float invalidNear=0;memcpy(cameraFixture+0x160,&invalidNear,4);present();
            Check(hdrPixel(32)==8,"Invalid camera safely bypasses depth of field");
            const float nearPlane=1;memcpy(cameraFixture+0x160,&nearPlane,4);
            present();Check(hdrPixel(32)<7,"Depth of field recovers when native inputs return");
        }
        temporalFixture[0x18]=1;
        edit(profiles,1,"Automatic profiles",1);
        edit(dof,4,"##Profile",3);edit(dof,1,"Depth of field enabled",1);
        edit(dof,3,"Focus Distance (units)",700);edit(dof,3,"Blur Strength",0.9f);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).exteriorNight.dof.focusDistance==700 &&
            cge::Settings::Load(config).exteriorNight.dof.strength==0.9f,"Reset Color preserves depth of field");
        edit(dof,5,"Reset Depth of Field",0);edit(dof,5,"Save",0);
        Check(!cge::Settings::Load(config).exteriorNight.dof.enabled &&
            cge::Settings::Load(config).dof.enabled,"Depth-of-field reset affects only the selected profile");
        edit(profiles,1,"Automatic profiles",0);
        edit(dof,4,"##Profile",0);edit(dof,1,"Depth of field enabled",0);edit(dof,5,"Save",0);
        present();if(sceneReady && !hdrConflict)Check(hdrPixel(32)==8,"Depth-of-field toggle restores sharp native scene");
        context->UpdateSubresource(depth.Get(),0,nullptr,data.data(),64*4,0);
        const char* clarity="Community Graphics Engine/Effects/Clarity";
        edit(clarity,4,"##Profile",0);edit(clarity,1,"Clarity enabled",1);
        edit(clarity,3,"Strength",99);edit(clarity,5,"Save",0);
        Check(cge::Settings::Load(config).clarity.enabled && cge::Settings::Load(config).clarity.strength==1,"Clarity numeric input bounds and save");
        clarityPattern=true;present();
        context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE clarityMap{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&clarityMap)),"Native clarity readback");
        const auto* clarityBytes=static_cast<const unsigned char*>(clarityMap.pData);
        Check(sceneReady?clarityBytes[32*clarityMap.RowPitch+32*4]>145:clarityBytes[32*clarityMap.RowPitch+32*4]==145,
            "Clarity runs only at the verified scene boundary");
        Check(clarityBytes[0]==17 && clarityBytes[1]==231 && clarityBytes[2]==93 && clarityBytes[3]==77,"Clarity leaves the native HUD untouched");
        context->Unmap(readback.Get(),0);clarityPattern=false;
        edit(clarity,3,"Strength",0.65f);edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).clarity.strength==0.65f && cge::Settings::Load(config).clarity.enabled,"Reset Color preserves clarity");
        edit(profiles,1,"Automatic profiles",1);edit(clarity,4,"##Profile",3);
        edit(clarity,1,"Clarity enabled",1);edit(clarity,3,"Strength",0.8f);edit(clarity,5,"Save",0);
        Check(cge::Settings::Load(config).exteriorNight.clarity.strength==0.8f,"Clarity saves selected profile");
        edit(clarity,5,"Reset Clarity",0);edit(clarity,5,"Save",0);
        Check(!cge::Settings::Load(config).exteriorNight.clarity.enabled &&
            cge::Settings::Load(config).clarity.strength==0.65f,"Clarity reset affects only the selected profile");
        edit(profiles,1,"Automatic profiles",0);edit(clarity,4,"##Profile",0);
        edit(clarity,5,"Reset Clarity",0);edit(clarity,5,"Save",0);
        const char* vibrance="Community Graphics Engine/Color/Vibrance";
        edit(vibrance,4,"##Profile",0);edit(vibrance,1,"Vibrance enabled",1);
        edit(vibrance,3,"Strength",99);edit(vibrance,5,"Save",0);
        auto savedVibrance=cge::Settings::Load(config);
        Check(savedVibrance.vibrance.enabled && savedVibrance.vibrance.strength==1,"Vibrance numeric input clamps and persists");
        present();pixel(sceneReady?209:204);
        edit(vibrance,1,"Vibrance enabled",0);present();pixel(204);
        edit(vibrance,1,"Vibrance enabled",1);
        edit(vibrance,3,"Strength",0.65f);edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        savedVibrance=cge::Settings::Load(config);
        Check(savedVibrance.vibrance.enabled && savedVibrance.vibrance.strength==0.65f,"Reset Color preserves Vibrance");
        edit(profiles,1,"Automatic profiles",1);
        edit(vibrance,4,"##Profile",3);edit(vibrance,1,"Vibrance enabled",1);
        edit(vibrance,3,"Strength",0.8f);edit(vibrance,5,"Save",0);
        Check(cge::Settings::Load(config).exteriorNight.vibrance.strength==0.8f,"Vibrance saves selected profile");
        edit(clarity,1,"Clarity enabled",1);edit(clarity,3,"Strength",0.45f);
        edit(vibrance,5,"Reset Vibrance",0);edit(vibrance,5,"Save",0);
        savedVibrance=cge::Settings::Load(config);
        Check(!savedVibrance.exteriorNight.vibrance.enabled && savedVibrance.vibrance.strength==0.65f &&
            savedVibrance.exteriorNight.clarity.enabled && savedVibrance.exteriorNight.clarity.strength==0.45f,"Vibrance reset preserves other profiles and effects");
        edit(clarity,5,"Reset Clarity",0);
        edit(profiles,1,"Automatic profiles",0);edit(vibrance,4,"##Profile",0);
        edit(vibrance,5,"Reset Vibrance",0);edit(vibrance,5,"Save",0);
        const char* motion="Community Graphics Engine/Effects/Motion Blur";
        edit(motion,1,"Motion blur enabled",1);edit(motion,4,"Quality",2);edit(motion,5,"Save",0);
        Check(cge::Settings::Load(config).motion.samples==16,"Motion quality and enable persist");
        if(sceneReady) {
            present();present();present();
            if(motionUiTaa) {
                std::ifstream file(root/"CGE.log");std::string contents((std::istreambuf_iterator<char>(file)),{});
                Check(contents.find("CGE motion probe: Native buffers acquired, TAA=1, source=world")!=std::string::npos,"World TAA survives temporary interface disable on 1.6.1170");
                Check(contents.find("CGE motion blur: Ready")!=std::string::npos,"Motion blur runs after world TAA observation");
                clarityPattern=true;present();
                context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE moving{};
                Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&moving)),"Moving native scene readback");
                const auto blurred=static_cast<const unsigned char*>(moving.pData)[32*moving.RowPitch+31*4];
                context->Unmap(readback.Get(),0);
                Check(blurred<145,"Motion blur changes native scene pixels despite temporary interface TAA disable");
                temporalFixture[0x18]=0;present();
                context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE sharp{};
                Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&sharp)),"Disabled TAA scene readback");
                Check(static_cast<const unsigned char*>(sharp.pData)[32*sharp.RowPitch+31*4]==145,"Disabled world TAA preserves sharp scene pixels");
                context->Unmap(readback.Get(),0);clarityPattern=false;
                std::ifstream disabled(root/"CGE.log");std::string disabledLog((std::istreambuf_iterator<char>(disabled)),{});
                Check(disabledLog.find("CGE motion probe: Native TAA disabled, TAA=0, source=world")!=std::string::npos,"Genuinely disabled world TAA still blocks motion blur");
                temporalFixture[0x18]=1;present();present();present();
            }
            blocking(true);present();
            std::ifstream log(root/"CGE.log");std::string logged((std::istreambuf_iterator<char>(log)),{});
            Check(logged.find("CGE motion blur: Paused")!=std::string::npos,"Framework input capture pauses motion");
            blocking(false);
        }
        edit(motion,1,"Motion blur enabled",0);
        edit(shared,3,"Exposure",-1);edit(shared,5,"Save",0);
        edit(shared,3,"Exposure",0);present();pixel(204);
                                                                      
        context->ClearState();output.Reset();buffer.Reset();readback.Reset();chain.Reset();
        memset(rendererFixture,0,sizeof(rendererFixture));
        Check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            &desc,&chain,&device,nullptr,&context)),"Reattach renderer");
        nativeResourcesValid=false;buffers();present();pixel(sceneReady?102:204);
        const char* presetPage="Community Graphics Engine/Presets";
        edit(vibrance,4,"##Profile",0);edit(vibrance,1,"Vibrance enabled",1);edit(vibrance,3,"Strength",0.7f);
        edit(general,5,"Save",0);
        edit(motion,1,"Motion blur enabled",1);edit(motion,3,"Intensity",1.7f);edit(motion,5,"Save",0);
        const auto previous=cge::Settings::Load(config);
        auto same=[](const cge::Settings& a,const cge::Settings& b) {
            return static_cast<const cge::ColorSettings&>(a)==static_cast<const cge::ColorSettings&>(b) &&
                a.interior==b.interior && a.exterior==b.exterior && a.exteriorNight==b.exteriorNight &&
                a.enabled==b.enabled && a.separateProfiles==b.separateProfiles &&
                a.profilesConfigured==b.profilesConfigured && a.motion==b.motion &&
                a.transitionSeconds==b.transitionSeconds && a.dayHour==b.dayHour && a.nightHour==b.nightHour;
        };
        edit(presetPage,5,"Natural",0);
        Check(same(cge::Settings::Load(config),previous),"Applying a preset does not silently overwrite the saved INI");
        present();pixel(sceneReady?216:204);
        edit(shared,3,"Contrast",1.31f);edit(shared,5,"Save",0);
        const auto editingActive=cge::Settings::Load(config);
        Check(!sceneReady || (editingActive.exterior.contrast==1.31f && editingActive.contrast==1.08f),"Preset editing follows the active day profile when scene state is available");
        edit(presetPage,5,"Undo Preset",0);edit(presetPage,5,"Save",0);
        Check(same(cge::Settings::Load(config),previous),"Undo restores all settings preceding the preset");
        const auto choices=cge::presets::List();
        Check(choices.size()==5,"Five built-in presets");
        for(std::size_t i=0;i<choices.size();++i) {
            edit(presetPage,5,choices[i].name,0);edit(presetPage,5,"Save",0);
            cge::Settings expected;expected.motion=previous.motion;Check(cge::presets::Apply(i,expected),"Valid preset index");
            Check(same(cge::Settings::Load(config),expected),"Preset buttons preserve motion settings while replacing visual profiles");
        }
        auto invalidPreset=previous;
        Check(!cge::presets::Apply(choices.size(),invalidPreset) && same(previous,invalidPreset),"Invalid preset index leaves settings unchanged");
        edit(general,5,"Reset All",0);edit(general,5,"Save",0);
        Check(cge::Settings::Load(config).Neutral() && !cge::Settings::Load(config).bloom.enabled,"Reset All saves neutral effects");
        const auto controlDisabled=Proc<int(*)(int,const char*)>(framework,"TestControlDisabled");
        struct DisabledCase {
            const char *page,*toggle,*control,*reset;
            float(*value)(const cge::Settings&);
        };
        const char* vignette="Community Graphics Engine/Color/Vignette";
        const char* grain="Community Graphics Engine/Effects/Film Grain";
        const char* split="Community Graphics Engine/Color/Split Toning";
        const char* aberration="Community Graphics Engine/Effects/Chromatic Aberration";
        const char* debanding="Community Graphics Engine/Color/Debanding";
        const char* sharpness="Community Graphics Engine/Effects/Sharpness";
        const char* distortion="Community Graphics Engine/Effects/Lens Distortion";
        const char* dirt="Community Graphics Engine/Effects/Lens Dirt";
        const char* lut="Community Graphics Engine/Color/3D LUT";
        const char* lgg="Community Graphics Engine/Color/Lift Gamma Gain";
        edit(sharpness,1,"Sharpness enabled",0);
        const DisabledCase cases[]={
            {ssao,"SSAO enabled","Strength","Reset SSAO",[](const cge::Settings& s){return s.ssao.strength;}},
            {sharpness,"Sharpness enabled","Sharpness","Reset Sharpness",[](const cge::Settings& s){return s.sharpen;}},
            {distortion,"Lens Distortion enabled","Amount","Reset Lens Distortion",[](const cge::Settings& s){return s.lensDistortion.amount;}},
            {dirt,"Lens Dirt enabled","Strength","Reset Lens Dirt",[](const cge::Settings& s){return s.lensDirt.strength;}},
            {lut,"3D LUT enabled","Strength","Reset 3D LUT",[](const cge::Settings& s){return s.lut.strength;}},
            {bloom,"Bloom enabled","Intensity","Reset Bloom",[](const cge::Settings& s){return s.bloom.intensity;}},
            {fog,"Fog enabled","Density","Reset Fog",[](const cge::Settings& s){return s.fog.density;}},
            {dof,"Depth of field enabled","Blur Strength","Reset Depth of Field",[](const cge::Settings& s){return s.dof.strength;}},
            {clarity,"Clarity enabled","Strength","Reset Clarity",[](const cge::Settings& s){return s.clarity.strength;}},
            {vibrance,"Vibrance enabled","Strength","Reset Vibrance",[](const cge::Settings& s){return s.vibrance.strength;}},
            {lgg,"Lift Gamma Gain enabled","Strength","Reset Lift Gamma Gain",[](const cge::Settings& s){return s.liftGammaGain.strength;}},
            {debanding,"Debanding enabled","Strength","Reset Debanding",[](const cge::Settings& s){return s.debanding.strength;}},
            {vignette,"Vignette enabled","Strength","Reset Vignette",[](const cge::Settings& s){return s.vignette.strength;}},
            {grain,"Film Grain enabled","Strength","Reset Film Grain",[](const cge::Settings& s){return s.filmGrain.strength;}},
            {split,"Split Toning enabled","Strength","Reset Split Toning",[](const cge::Settings& s){return s.splitToning.strength;}},
            {aberration,"Chromatic Aberration enabled","Strength","Reset Chromatic Aberration",[](const cge::Settings& s){return s.chromaticAberration.strength;}},
            {motion,"Motion blur enabled","Intensity","",[](const cge::Settings& s){return s.motion.strength;}}
        };
        for(const auto& item:cases) {
            Check(render(item.page),"Render inactive effect");
            Check(controlDisabled(1,item.toggle)==0 && controlDisabled(2,item.control)==1 &&
                controlDisabled(3,item.control)==1 && controlDisabled(5,"Save")==0,"Inactive effect grays controls while toggle and Save stay accessible");
            edit(item.page,1,item.toggle,1);
            Check(controlDisabled(2,item.control)==0 && controlDisabled(3,item.control)==0,"Enabling effect unlocks controls immediately");
            edit(item.page,3,item.control,0.6f);edit(item.page,5,"Save",0);
            Check(item.value(cge::Settings::Load(config))==0.6f,"Enabled field accepts input");
            edit(general,1,"Effects enabled",0);Check(render(item.page),"Render with master disabled");
            Check(controlDisabled(2,item.control)==1 && controlDisabled(3,item.control)==1,"Master switch grays effect settings");
            edit(item.page,3,item.control,0.9f);edit(item.page,5,"Save",0);
            Check(item.value(cge::Settings::Load(config))==0.6f,"Master-disabled controls retain values");
            edit(general,1,"Effects enabled",1);edit(item.page,1,item.toggle,0);
            edit(item.page,2,item.control,0.8f);edit(item.page,3,item.control,0.9f);edit(item.page,5,"Reset",0);
            if(item.reset[0])edit(item.page,5,item.reset,0);
            edit(item.page,5,"Save",0);
            Check(item.value(cge::Settings::Load(config))==0.6f,"Inactive sliders, numeric fields and resets cannot edit settings");
            edit(item.page,1,item.toggle,1);
            Check(controlDisabled(2,item.control)==0,"Reactivation restores editable slider");
            edit(item.page,3,item.control,0.4f);edit(item.page,5,"Save",0);
            Check(item.value(cge::Settings::Load(config))==0.4f,"Reactivated control accepts edits");
            edit(item.page,1,item.toggle,0);
        }
        aberrationPattern=true;present();const auto noAberration=image();
        edit(aberration,1,"Chromatic Aberration enabled",1);edit(aberration,3,"Strength",1);edit(aberration,3,"Center Radius",0.1f);present();const auto withAberration=image();
        Check(sceneReady?withAberration!=noAberration:withAberration==noAberration,"Native aberration visibly shifts edge channels only through the verified hook");
        edit(aberration,5,"Reset Chromatic Aberration",0);aberrationPattern=false;
        bandingPattern=true;present();const auto noDebanding=image();
        edit(debanding,1,"Debanding enabled",1);edit(debanding,3,"Strength",1);edit(debanding,3,"Radius (px)",4);edit(debanding,3,"Threshold",0.05f);present();const auto withDebanding=image();
        Check(sceneReady?withDebanding!=noDebanding:withDebanding==noDebanding,"Native debanding smooths eligible gradients through the verified hook");
        edit(debanding,5,"Reset Debanding",0);bandingPattern=false;
        edit(sharpness,1,"Sharpness enabled",1);edit(sharpness,3,"Sharpness",0.5f);edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).sharpen==0.5f && cge::Settings::Load(config).sharpnessEnabled,"Reset Color preserves the separate sharpness effect");
        edit(sharpness,5,"Reset Sharpness",0);edit(sharpness,1,"Sharpness enabled",0);
        edit(debanding,1,"Debanding enabled",1);edit(debanding,3,"Threshold",0.05f);edit(debanding,5,"Save",0);
        Check(cge::Settings::Load(config).debanding.threshold==0.05f,"Debanding threshold can be edited and saved");
        edit(debanding,5,"Reset Debanding",0);
        edit(distortion,1,"Lens Distortion enabled",1);edit(distortion,3,"Amount",-0.75f);present();pixel(204);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).lensDistortion.enabled && cge::Settings::Load(config).lensDistortion.amount==-0.75f,"Reset Color retains signed distortion settings");
        edit(distortion,3,"Amount",99);edit(distortion,5,"Save",0);
        Check(cge::Settings::Load(config).lensDistortion.amount==1,"Distortion numeric input clamps positive values");
        edit(distortion,3,"Amount",-99);edit(distortion,5,"Save",0);
        Check(cge::Settings::Load(config).lensDistortion.amount==-1,"Distortion numeric input clamps negative values");
        edit(distortion,5,"Reset Lens Distortion",0);edit(distortion,5,"Save",0);
        Check(!cge::Settings::Load(config).lensDistortion.enabled && cge::Settings::Load(config).lensDistortion.amount==0.2f,"Distortion reset is independent");
        edit(dirt,1,"Lens Dirt enabled",1);edit(dirt,3,"Strength",0.75f);edit(dirt,3,"Light Threshold",0.6f);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).lensDirt.enabled && cge::Settings::Load(config).lensDirt.strength==0.75f && cge::Settings::Load(config).lensDirt.threshold==0.6f,"Reset Color retains Lens Dirt");
        edit(dirt,5,"Reset Lens Dirt",0);edit(dirt,5,"Save",0);
        Check(!cge::Settings::Load(config).lensDirt.enabled && cge::Settings::Load(config).lensDirt.strength==0.35f,"Lens Dirt reset is independent");
        edit(lut,1,"3D LUT enabled",1);edit(lut,3,"Strength",1);
        edit(lut,4,"##LUT File",1);edit(lut,5,"Save",0);
        Check(cge::Settings::Load(config).lut.file=="Test.cube","Select external LUT from framework");
        present();pixel(sceneReady?102:204);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).lut.file=="Test.cube" && cge::Settings::Load(config).lut.enabled,"Reset Color preserves LUT");
        edit(lut,5,"Reload LUTs",0);present();pixel(sceneReady?102:204);
        edit(lut,5,"Reset 3D LUT",0);edit(lut,5,"Save",0);
        Check(cge::Settings::Load(config).lut.file.empty() && !cge::Settings::Load(config).lut.enabled,"Independent LUT reset");
        edit(lgg,1,"Lift Gamma Gain enabled",1);edit(lgg,3,"Strength",1);edit(lgg,3,"Gain Red",0.5f);
        present();pixel(sceneReady?102:204);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).liftGammaGain.enabled && cge::Settings::Load(config).liftGammaGain.gainRed==0.5f,
            "Reset Color preserves separate RGB grading");
        edit(lgg,3,"Lift Blue",99);edit(lgg,3,"Gamma Green",-1);edit(lgg,3,"Gain Blue",99);edit(lgg,5,"Save",0);
        const auto graded=cge::Settings::Load(config).liftGammaGain;
        Check(graded.liftBlue==0.25f && graded.gammaGreen==0.5f && graded.gainBlue==2,"RGB grading numeric fields clamp correctly");
        edit(lgg,5,"Reset Lift Gamma Gain",0);edit(lgg,5,"Save",0);
        Check(!cge::Settings::Load(config).liftGammaGain.enabled &&
            cge::Settings::Load(config).liftGammaGain.Neutral(),"RGB grading reset restores neutral values");
        edit(vignette,1,"Vignette enabled",1);edit(vignette,3,"Strength",1);present();pixel(204);
        context->CopyResource(readback.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE edgeMap{};
        Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&edgeMap)),"Native vignette edge readback");
        const auto edgeRed=static_cast<const unsigned char*>(edgeMap.pData)[63*edgeMap.RowPitch+63*4];
        Check(sceneReady?edgeRed<20:edgeRed==204,"Vignette reaches native scene edges only with the verified hook");
        context->Unmap(readback.Get(),0);edit(vignette,1,"Vignette enabled",0);
        edit(split,1,"Split Toning enabled",1);edit(split,3,"Shadows Red",0.75f);
        edit(shared,5,"Reset Color",0);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).splitToning.enabled && cge::Settings::Load(config).splitToning.shadowRed==0.75f,
            "Color reset preserves new finishing settings");
        edit(split,5,"Reset Split Toning",0);edit(split,5,"Save",0);
        Check(render(motion) && controlDisabled(4,"Quality")==1 && controlDisabled(5,"Natural")==1 &&
            controlDisabled(5,"Strong")==1,"Motion quality and shortcuts disabled with effect");
        edit(motion,4,"Quality",0);edit(motion,5,"Strong",0);edit(motion,5,"Save",0);
        Check(!cge::Settings::Load(config).motion.enabled && cge::Settings::Load(config).motion.samples==12,"Inactive motion shortcuts cannot activate or change the effect");
        edit(dof,1,"Depth of field enabled",1);edit(dof,4,"Focus Mode",1);
        Check(controlDisabled(3,"Focus Distance (units)")==1 && controlDisabled(3,"Focus Speed")==0 &&
            controlDisabled(3,"Focus Area")==0,"Automatic focus enables its fields and grays manual focus");
        edit(dof,3,"Focus Speed",9);edit(dof,3,"Focus Area",0.08f);
        edit(dof,3,"Focus Distance (units)",99);edit(dof,5,"Save",0);
        const auto automatic=cge::Settings::Load(config).dof;
        Check(automatic.autofocus && automatic.focusSpeed==9 && automatic.focusRadius==0.08f &&
            automatic.focusDistance==2000,"Automatic focus controls persist without modifying manual fallback");
        edit(dof,4,"Focus Mode",0);
        Check(controlDisabled(3,"Focus Distance (units)")==0 && controlDisabled(3,"Focus Speed")==1,"Manual mode restores manual control");
        edit(dof,3,"Focus Speed",2);edit(dof,5,"Save",0);
        Check(cge::Settings::Load(config).dof.focusSpeed==9,"Inactive auto control retains its value");
        edit(dof,5,"Reset Depth of Field",0);edit(dof,5,"Save",0);
        edit(profiles,3,"Dawn (hour)",7);edit(profiles,5,"Save",0);
        Check(cge::Settings::Load(config).dayHour==6 && controlDisabled(2,"Dawn (hour)")==1,"Automatic profile controls disabled when profiles are off");
        edit(profiles,1,"Automatic profiles",1);
        edit(clarity,4,"##Profile",3);edit(clarity,1,"Clarity enabled",1);edit(clarity,3,"Strength",0.7f);
        edit(clarity,5,"Save",0);edit(profiles,1,"Automatic profiles",0);
        Check(render(clarity) && controlDisabled(1,"Clarity enabled")==1 && controlDisabled(3,"Strength")==1,
            "Inactive profile keeps nested enabled-effect controls disabled");
        edit(clarity,3,"Strength",0.1f);edit(clarity,1,"Clarity enabled",0);edit(clarity,5,"Reset Clarity",0);edit(clarity,5,"Save",0);
        Check(cge::Settings::Load(config).exteriorNight.clarity.enabled &&
            cge::Settings::Load(config).exteriorNight.clarity.strength==0.7f,"Nested disabled scopes preserve inactive profile settings");
        edit(profiles,1,"Automatic profiles",1);Check(render(clarity) && controlDisabled(3,"Strength")==0,"Active profile restores nested controls");
        edit(general,1,"Effects enabled",0);edit(shared,4,"##Profile",0);
        Check(controlDisabled(3,"Exposure")==1,"Master switch grays color controls");
        edit(shared,3,"Exposure",1);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).exposure==0,"Disabled color controls preserve values");
        edit(general,1,"Effects enabled",1);edit(general,5,"Reset All",0);edit(general,5,"Save",0);
        present();
        Check(render("Community Graphics Engine/Effects/Motion Blur") &&
            Proc<bool(*)(const char*)>(framework,"TestHasText")("Requires native TAA enabled."),"Motion blur page explains native TAA requirement");
        const char* weatherPage="Community Graphics Engine/Weather Profiles";
        auto disabledControl=Proc<int(*)(int,const char*)>(framework,"TestControlDisabled");
        edit(weatherPage,3,"Exposure Offset",2);
        Check(disabledControl(3,"Exposure Offset")==1,"Weather controls disabled until weather profiles are enabled");
        edit(weatherPage,1,"Weather profiles enabled",1);
        edit(weatherPage,3,"Weather Transition (sec)",0);
        edit(weatherPage,3,"Exposure Offset",1);
        edit(weatherPage,5,"Save",0);
        auto weatherSaved=cge::Settings::Load(config);
        Check(weatherSaved.weather.enabled && weatherSaved.weather.profiles[0].exposure==1,"Framework saves weather adjustments");
        SetCell(false);present();
        if(sceneReady) {
        Check(render(weatherPage) && Proc<bool(*)(const char*)>(framework,"TestHasText")("Clear"),"Native Sky getter reports clear weather through Address Library");
        if(sceneReady)pixel(255);
        SetWeather(4);Sleep(110);present();
        Check(render(weatherPage) && Proc<bool(*)(const char*)>(framework,"TestHasText")("Rain"),"Native weather flags drive the active weather category");
        SetWeather(1);SetCell(true);present();
        Check(render(weatherPage) && Proc<bool(*)(const char*)>(framework,"TestHasText")("Inactive indoors."),"Interior ignores exterior weather");
        }else Check(render(weatherPage) && Proc<bool(*)(const char*)>(framework,"TestHasText")("Weather data unavailable."),"Missing native weather safely retains base settings");
        SetWeather(1);SetCell(false);present();
        edit(weatherPage,1,"Weather profiles enabled",0);
        edit(general,5,"Reset All",0);edit(general,5,"Save",0);

    }
    context->ClearState();output.Reset();buffer.Reset();readback.Reset();
    Check(SUCCEEDED(chain->ResizeBuffers(1,80,48,DXGI_FORMAT_UNKNOWN,0)),"No retained game backbuffer during resize");
    chain.Reset();context.Reset();device.Reset();
    Check(GetWindowLongPtrW(window,GWLP_WNDPROC)==originalWindowProc,"Window input handling remains unchanged");
    DestroyWindow(window);
    std::ifstream log(root/"CGE.log");std::string logged((std::istreambuf_iterator<char>(log)),{});
    Check(logged.find("CommonLibSSE-NG bridge ready")!=std::string::npos,"Official CommonLib interface bridge used by the production plugin");
    Check(logged.find("CGE startup stage: scene hook")!=std::string::npos && logged.find("CGE startup stage: HDR hook")!=std::string::npos,"Startup stages identify the last completed native initialization");
    if(!sceneMissing && !invalidLibrary && !opaqueConflict) {
        Check(logged.find("CGE opaque hooks ready")!=std::string::npos,"Native opaque pass hooks installed for selected runtime");
        if(sceneReady)Check(worldDelegations>0 && worldDelegations==opaqueDelegations && opaqueDelegations==decalDelegations,"World, opaque and decal delegates stay balanced");
    }
    if(frameworkReady && sceneReady && !hdrConflict && !opaqueConflict)
        Check(logged.find("CGE SSAO temporal: camera reprojection available")!=std::string::npos,
            "Native camera activates temporal AO with native TAA disabled");
    if(opaqueConflict) {
        Check(logged.find("CGE native SSAO unavailable")!=std::string::npos,"Opaque conflict diagnosed without disabling other effects");
        Check(!memcmp(savedWorldCall,worldFrameFixture+frameOffset,5),"Opaque hook failure leaves no partial world hook");
    }
    if(missing)Check(logged.find("install SKSE Menu Framework")!=std::string::npos,"Missing framework diagnosis");
    if(old)Check(logged.find("3 API required")!=std::string::npos,"Old framework diagnosis");
    if(incomplete)Check(logged.find("exports are incomplete")!=std::string::npos,"Incomplete API diagnosis");
    if(interfaceStore32)Check(interfaceDelegations==1,"Relocated 32-bit store preserves the native manager argument and original data target");
    if(interfaceRip || interfaceLea || interfaceRip32 || interfaceStore32 || interfaceEndbr)Check(interfaceDelegations>0,"Relocated interface accesses the original native RIP-relative data");
    if(!sceneReady)Check(logged.find("scene hook unavailable")!=std::string::npos,"Scene hook failure diagnosis");
    if(gogRuntime)Check(logged.find("CGE executable variant: GOG")!=std::string::npos &&
        fixtureLayout.LibraryName().ends_with("-0.bin"),"GOG marker is retained for SKSE and normalized only in the library filename");
    if(inputConflict)Check(logged.find("CGE native keyboard toggle unavailable")!=std::string::npos && *reinterpret_cast<const volatile unsigned char*>(inputPollFixture+0x7b)==0xe9,"Input hook conflict is diagnosed and left untouched");
    if(hdrConflict)Check(logged.find("HDR hook unavailable")!=std::string::npos,"HDR hook conflict diagnosis");
    std::puts("PASS: production SKSE hooks, late SMF registration, pages, controls, persistence, HUD and input ownership.");
    return 0;
}
