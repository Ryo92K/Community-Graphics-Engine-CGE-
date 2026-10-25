#include "skse_api.h"
#include "settings.h"
#include "presets.h"
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
__declspec(allocate(".cgeui")) const unsigned char interfaceFixture[]={
    0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x50,
    0x48,0x83,0xc4,0x50,0x41,0x5f,0x41,0x5d,0x41,0x5c,0x5f,0x5e,0xc3
};
void InterfaceBoundary(){reinterpret_cast<void(*)(std::int64_t)>(const_cast<unsigned char*>(interfaceFixture))(0);}
#pragma section(".cgehdr",read,execute)
__declspec(allocate(".cgehdr")) const unsigned char tonemapFixture[0x190]{};
__declspec(allocate(".cgehdr")) const unsigned char imageSpaceDrawFixture[64]{};
unsigned hdrDelegations{};
void* expectedHdrParameter=reinterpret_cast<void*>(0x1234);
void NativeTonemap(void* manager,std::uint32_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
    Check(manager==reinterpret_cast<void*>(0x5678) && effect==0x22 && source==1 && target==0x29 &&
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
    memcpy(main+0x170,arguments,sizeof(arguments));main[0x178]=0xe8;
    const auto offset=static_cast<std::int32_t>(draw-(main+0x17d));memcpy(main+0x179,&offset,4);
    const unsigned char leave[]={0x48,0x83,0xc4,0x30,0x5b,0xc3};memcpy(main+0x17d,leave,sizeof(leave));
    const unsigned char prefix[]={0x4c,0x8b,0xdc,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70,
        0x48,0x83,0xc4,0x70,0x41,0x5f,0x41,0x5e,0x5f,0x5e,0x5d};
    memcpy(draw,prefix,sizeof(prefix));
    draw[25]=0xff;draw[26]=0x25;memset(draw+27,0,4);
    auto* target=reinterpret_cast<void*>(NativeTonemap);memcpy(draw+31,&target,8);
    VirtualProtect(main,sizeof(tonemapFixture),oldMain,&ignored);
    VirtualProtect(draw,sizeof(imageSpaceDrawFixture),oldDraw,&ignored);
    FlushInstructionCache(GetCurrentProcess(),main,sizeof(tonemapFixture));
    FlushInstructionCache(GetCurrentProcess(),draw,sizeof(imageSpaceDrawFixture));
}
void TonemapBoundary() {
    using Draw=void(*)(void*,std::uint32_t,std::uint32_t,std::uint32_t,void*);
    reinterpret_cast<Draw>(const_cast<unsigned char*>(tonemapFixture))(reinterpret_cast<void*>(0x5678),0x22,1,0x29,expectedHdrParameter);
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
    const float planes[]={1,10000};memcpy(cameraFixture+0x160,planes,sizeof(planes));
    auto* code=const_cast<unsigned char*>(cameraGetterFixture);DWORD old{},ignored{};
    Check(VirtualProtect(code,15,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Camera fixture writable");
    const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&worldRootSingletonFixture)-
        reinterpret_cast<std::intptr_t>(code+7));memcpy(code+3,&displacement,4);
    VirtualProtect(code,15,old,&ignored);
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
void WriteLibrary(const fs::path& path) {
    std::vector<std::uint32_t> library(24+414661);
    library[0]=5;library[1]=1;library[2]=7;library[3]=104;
    memcpy(reinterpret_cast<char*>(library.data())+20,"SkyrimSE.exe",12);
    library[21]=8;library[23]=414661;
    library[24+82084]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(interfaceFixture)-
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    library[24+400447]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&calendarSingletonFixture)-
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    library[24+403521]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&playerSingletonFixture)-
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    const std::pair<unsigned,const void*> motionEntries[]={
        {36609,cameraGetterFixture},{403513,&worldRootSingletonFixture},{105674,tonemapFixture},{105676,imageSpaceDrawFixture},{411393,rendererFixture},{414660,&imageSpaceSingletonFixture},{400327,&uiSingletonFixture}};
    for(auto [id,address]:motionEntries)library[24+id]=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(address)-reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(library.data()),static_cast<std::streamsize>(library.size()*4));
    Check(bool(file),"Write Address Library fixture");
}

void(*listener)(cge::skse::Message*){};
unsigned handleCalls{};
bool Listen(std::uint32_t handle,const char* sender,void(*callback)(cge::skse::Message*)) {
    Check(handle==7 && !strcmp(sender,"SKSE"),"SKSE message subscription");listener=callback;return true;
}
cge::skse::Messaging messaging{2,Listen,nullptr,nullptr};
void* Query(std::uint32_t id){return id==5?&messaging:nullptr;}
std::uint32_t Handle(){++handleCalls;return 7;}
int wmain(int argc,wchar_t** argv) {
    Check(argc==5,"DLL, fixture, mode and test framework");
    const std::wstring mode=argv[3];
    const bool missing=mode==L"--missing-framework",old=mode==L"--old-framework",
        incomplete=mode==L"--incomplete-framework",sceneMissing=mode==L"--missing-library",
        invalidLibrary=mode==L"--invalid-library",sceneConflict=mode==L"--hook-conflict",
        hdrConflict=mode==L"--hdr-hook-conflict";
    const bool frameworkReady=!missing && !old && !incomplete;
    const bool sceneReady=!sceneMissing && !invalidLibrary && !sceneConflict;
    const auto root=fs::absolute(argv[2])/L"SKSE"/L"Plugins"/L"CGE-\u03b3";
    fs::create_directories(root);fs::copy_file(argv[1],root/"CGE.dll",fs::copy_options::overwrite_existing);
    auto config=root/"CGE.ini";
    Write(config,"[Color]\nExposureStops=-1\n[Input]\nToggleKey=119\n");
    PrepareTonemapFixture();PrepareCameraFixture();const std::uint32_t formId=0x14;memcpy(playerFixture+0x14,&formId,4);
    SetCell(false);SetGameHour(12);
    const void* temporal=temporalFixture;memcpy(imageSpaceFixture+0x1f0,&temporal,sizeof(temporal));temporalFixture[0x18]=1;
    const auto library=root/"versionlib-1-7-104-0.bin";WriteLibrary(library);
    if(sceneMissing)fs::remove(library);
    if(invalidLibrary)Write(library,"invalid");
    auto corrupt=[](const unsigned char* address){
        DWORD oldProtection{},ignored{};auto* p=const_cast<unsigned char*>(address);
        Check(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&oldProtection)!=FALSE,"Fixture writable");
        *p=0x90;VirtualProtect(p,1,oldProtection,&ignored);
    };
    if(sceneConflict)corrupt(interfaceFixture);
    if(hdrConflict)corrupt(tonemapFixture+0x178);
    auto module=LoadLibraryExW((root/"CGE.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"Load production CGE.dll");
    auto load=Proc<bool(*)(const cge::skse::Interface*)>(module,"SKSEPlugin_Load");
    cge::skse::Interface api{0x02030010,cge::skse::Runtime,0,0,Query,Handle};
    auto wrong=api;wrong.runtimeVersion=0x01061170;
    Check(!load(&wrong) && !load(nullptr),"Unsupported runtime refused");
    Check(load(&api) && load(&api) && listener && handleCalls==1,"Creation hook and one SKSE listener");
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
    if(pageCount)Check(pageCount()==(frameworkReady?9u:0u),"Validate API before registering exactly nine pages");
    const char* shared="Community Graphics Engine/Effects/Color";
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
    memcpy(rendererFixture+0xbc0,&v,sizeof(v));memcpy(rendererFixture+0x20a0,&z,sizeof(z));
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags|=D3D11_BIND_RENDER_TARGET;
    device->CreateTexture2D(&d,nullptr,&hdr);device->CreateShaderResourceView(hdr.Get(),nullptr,&hdrView);
    auto* h=hdrView.Get();memcpy(rendererFixture+0xaa0,&h,sizeof(h));
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    device->CreateTexture2D(&d,nullptr,&hdrReadback);
    std::vector<float> hdrPixels(64*64*4,0.05f);
    for(unsigned y=30;y<34;++y)for(unsigned x=30;x<34;++x)for(unsigned c=0;c<3;++c)hdrPixels[(y*64+x)*4+c]=8;
    bool nativeResourcesValid=true,clarityPattern=false;
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
        auto* rt=output.Get();context->OMSetRenderTargets(1,&rt,nullptr);
        D3D11_VIEWPORT sentinel{3,4,19,20,0,1};context->RSSetViewports(1,&sentinel);
        if(nativeResourcesValid) {
            context->UpdateSubresource(hdr.Get(),0,nullptr,hdrPixels.data(),64*16,0);
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
        Check(abs(int(bytes[32*map.RowPitch+32*4])-red)<=2,"Effects still precede native HUD");
        Check(bytes[0]==17 && bytes[1]==231 && bytes[2]==93 && bytes[3]==77,"HUD remains untouched");
        context->Unmap(readback.Get(),0);
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
    if(frameworkReady) {
        edit(shared,3,"Exposure",0);present();pixel(204);
        edit(bloom,2,"Threshold",0.5f);
        edit(bloom,1,"Bloom enabled",1);present();
        Check(hdrConflict || !sceneReady || halo()>0.05f,"Framework controls operate production HDR bloom");
        edit(bloom,2,"Threshold",99);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).bloom.threshold==1,"Framework bloom slider clamps to 0..1");
        edit(bloom,3,"Threshold",0.5f);
        edit(bloom,3,"Intensity",0.75f);edit(shared,5,"Save",0);
        Check(cge::Settings::Load(config).bloom.intensity==0.75f,"Framework numeric input persists");
        edit(bloom,1,"Bloom enabled",0);present();Check(halo()==0.05f,"Framework bloom switch bypasses HDR");
        const char* general="Community Graphics Engine/General";
        const char* profiles="Community Graphics Engine/Profiles";
        edit(profiles,1,"Automatic profiles",1);
        edit(shared,4,"##Profile",1);edit(shared,3,"Exposure",0.5f);
        edit(bloom,4,"##Profile",3);edit(bloom,3,"Intensity",0.9f);
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
            void* invalid=reinterpret_cast<void*>(1);memcpy(rendererFixture+0x20a0,&invalid,sizeof(invalid));
            present();Check(halo()==0.05f,"Invalid native depth pointer bypasses fog");
            auto* valid=depthView.Get();memcpy(rendererFixture+0x20a0,&valid,sizeof(valid));
            float invalidNear=0;memcpy(cameraFixture+0x160,&invalidNear,4);present();
            Check(halo()==0.05f,"Invalid native frustum bypasses fog");
            const float nearPlane=1;memcpy(cameraFixture+0x160,&nearPlane,4);
            cameraFixture[0x168]=1;present();Check(halo()==0.05f,"Orthographic camera bypasses perspective fog");cameraFixture[0x168]=0;
            playerSingletonFixture=nullptr;present();Check(halo()==0.05f,"Missing player cell bypasses fog");
            playerSingletonFixture=playerFixture;present();Check(halo()>0.2f,"Fog recovers after valid native inputs return");
        }
        temporalFixture[0x18]=1;
        edit(profiles,1,"Automatic profiles",1);
        edit(fog,4,"##Profile",3);edit(fog,3,"Density",0.8f);edit(fog,3,"Start Distance (units)",500);
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
        context->UpdateSubresource(depth.Get(),0,nullptr,farDepth.data(),64*4,0);
        temporalFixture[0x18]=0;present();
        if(sceneReady && !hdrConflict) {
            Check(hdrPixel(32)<7,"Native depth of field blurs the HDR background without fog, TAA or motion blur");
            edit(dof,3,"Focus Distance (units)",50000);present();Check(hdrPixel(32)==8,"Native manual focus can restore sharp HDR highlights");
            edit(dof,3,"Focus Distance (units)",500);
            edit(general,1,"Effects enabled",0);present();Check(hdrPixel(32)==8,"Master checkbox bypasses depth of field");
            edit(general,1,"Effects enabled",1);
            void* invalid=reinterpret_cast<void*>(1);memcpy(rendererFixture+0x20a0,&invalid,sizeof(invalid));
            present();Check(hdrPixel(32)==8,"Missing native depth safely bypasses depth of field");
            auto* valid=depthView.Get();memcpy(rendererFixture+0x20a0,&valid,sizeof(valid));
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
        const char* motion="Community Graphics Engine/Effects/Motion Blur";
        edit(motion,1,"Motion blur enabled",1);edit(motion,4,"Quality",2);edit(motion,5,"Save",0);
        Check(cge::Settings::Load(config).motion.samples==16,"Motion quality and enable persist");
        if(sceneReady) {
            present();present();present();
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
        edit(general,5,"Save",0);
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
        present();pixel(sceneReady?208:204);
        edit(presetPage,5,"Undo Preset",0);edit(presetPage,5,"Save",0);
        Check(same(cge::Settings::Load(config),previous),"Undo restores all settings preceding the preset");
        const auto choices=cge::presets::List();
        Check(choices.size()==5,"Five built-in presets");
        for(std::size_t i=0;i<choices.size();++i) {
            edit(presetPage,5,choices[i].name,0);edit(presetPage,5,"Save",0);
            cge::Settings expected;Check(cge::presets::Apply(i,expected),"Valid preset index");
            Check(same(cge::Settings::Load(config),expected),"Every preset button applies and saves all profile/effect settings");
        }
        auto invalidPreset=previous;
        Check(!cge::presets::Apply(choices.size(),invalidPreset) && same(previous,invalidPreset),"Invalid preset index leaves settings unchanged");
        edit(general,5,"Reset All",0);edit(general,5,"Save",0);
        Check(cge::Settings::Load(config).Neutral() && !cge::Settings::Load(config).bloom.enabled,"Reset All saves neutral effects");
    }
    context->ClearState();output.Reset();buffer.Reset();readback.Reset();
    Check(SUCCEEDED(chain->ResizeBuffers(1,80,48,DXGI_FORMAT_UNKNOWN,0)),"No retained game backbuffer during resize");
    chain.Reset();context.Reset();device.Reset();
    Check(GetWindowLongPtrW(window,GWLP_WNDPROC)==originalWindowProc,"Window input handling remains unchanged");
    DestroyWindow(window);
    std::ifstream log(root/"CGE.log");std::string logged((std::istreambuf_iterator<char>(log)),{});
    if(missing)Check(logged.find("install SKSE Menu Framework")!=std::string::npos,"Missing framework diagnosis");
    if(old)Check(logged.find("3 API required")!=std::string::npos,"Old framework diagnosis");
    if(incomplete)Check(logged.find("exports are incomplete")!=std::string::npos,"Incomplete API diagnosis");
    if(!sceneReady)Check(logged.find("scene hook unavailable")!=std::string::npos,"Scene hook failure diagnosis");
    if(hdrConflict)Check(logged.find("HDR hook unavailable")!=std::string::npos,"HDR hook conflict diagnosis");
    std::puts("PASS: production SKSE hooks, late SMF registration, pages, controls, persistence, HUD and input ownership.");
    return 0;
}
