#include "skse_api.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
template<class T>T Proc(HMODULE m,const char* name){auto fn=reinterpret_cast<T>(GetProcAddress(m,name));Check(fn!=nullptr,name);return fn;}
void Write(const fs::path& p,const std::string& text){std::ofstream f(p);Check(bool(f<<text),"Write fixture");}
HWND physicalForeground{};
SHORT physicalKeys[256]{};
POINT physicalPointer{};
BOOL WINAPI PhysicalCursor(LPPOINT point){*point=physicalPointer;return TRUE;}
BOOL WINAPI PhysicalToClient(HWND,LPPOINT){return TRUE;}
BOOL WINAPI PhysicalClient(HWND,LPRECT rect){*rect={0,0,1280,1440};return TRUE;}
SHORT WINAPI PhysicalKey(int key){return key>=0 && key<256?physicalKeys[key]:0;}
HWND WINAPI PhysicalForeground(){return physicalForeground;}
BOOL WINAPI NoClipRead(LPRECT){return FALSE;}
BOOL WINAPI NoClipWrite(const RECT*){return TRUE;}
BOOL WINAPI VisibleCursor(PCURSORINFO cursor){cursor->flags=CURSOR_SHOWING;return TRUE;}
HCURSOR WINAPI NoCursorChange(HCURSOR){return nullptr;}
// Only the isolated test DLL's imports are overridden. No desktop keys or
// cursor/clip mutations are sent to Windows.
struct ImportOverride {
    ULONG_PTR* slot{};ULONG_PTR previous{};
    template<class T>ImportOverride(HMODULE module,const char* symbol,T replacement) {
        auto* base=reinterpret_cast<unsigned char*>(module);
        auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        for(;imports->Name && !slot;++imports) {
            if(!imports->OriginalFirstThunk)continue;
            auto* names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+imports->OriginalFirstThunk);
            auto* entries=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+imports->FirstThunk);
            for(;names->u1.AddressOfData;++names,++entries) {
                if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
                auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
                if(!strcmp(reinterpret_cast<const char*>(name->Name),symbol)){slot=&entries->u1.Function;break;}
            }
        }
        Check(slot!=nullptr,"Physical-input test import exists");
        previous=*slot;Store(reinterpret_cast<ULONG_PTR>(replacement));
    }
    void Store(ULONG_PTR value) {
        DWORD prior{},ignored{};
        Check(VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&prior)!=FALSE,"Test import writable");
        *slot=value;
        Check(VirtualProtect(slot,sizeof(*slot),prior,&ignored)!=FALSE,"Test import protection restored");
    }
    ~ImportOverride(){Store(previous);}
};

unsigned forwarded{};
LRESULT CALLBACK Window(HWND window,UINT type,WPARAM w,LPARAM l) {
    if(type==WM_KEYDOWN && w=='W')++forwarded;
    return IsWindowUnicode(window)?DefWindowProcW(window,type,w,l):DefWindowProcA(window,type,w,l);
}

// An executable fixture with the verified Skyrim prefix and a balanced return.
// It exercises the production trampoline without executing game code.
#pragma section(".cgeui",read,execute)
__declspec(allocate(".cgeui")) const unsigned char interfaceFixture[]={
    0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x83,0xec,0x50,
    0x48,0x83,0xc4,0x50,0x41,0x5f,0x41,0x5d,0x41,0x5c,0x5f,0x5e,0xc3
};
void InterfaceBoundary(){reinterpret_cast<void(*)(std::int64_t)>(const_cast<unsigned char*>(interfaceFixture))(0);}
alignas(8) unsigned char playerFixture[0x80]{};
alignas(8) unsigned char cellFixture[0x50]{};
void* playerSingletonFixture=playerFixture;
void SetCell(bool interior) {
    const std::uint16_t flags=interior?1:0;
    memcpy(cellFixture+0x40,&flags,sizeof(flags));
    const void* cell=cellFixture;memcpy(playerFixture+0x60,&cell,sizeof(cell));
}
void WriteLibrary(const fs::path& path) {
    std::vector<std::uint32_t> library(24+403522);
    library[0]=5;library[1]=1;library[2]=7;library[3]=104;
    memcpy(reinterpret_cast<char*>(library.data())+20,"SkyrimSE.exe",12);
    library[21]=8;library[23]=403522;
    library[24+82084]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(interfaceFixture)-
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    library[24+403521]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&playerSingletonFixture)-
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(library.data()),static_cast<std::streamsize>(library.size()*4));
    Check(bool(file),"Write Address Library fixture");
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==4,"DLL, fixture and window mode");
    const std::wstring mode=argv[3];
    bool ansi=mode==L"--ansi";
    const bool rejectScene=mode==L"--missing-library" || mode==L"--invalid-library" || mode==L"--hook-conflict";
    fs::path root=fs::absolute(argv[2])/"SKSE"/"Plugins";fs::create_directories(root);
    fs::copy_file(argv[1],root/"CGE.dll",fs::copy_options::overwrite_existing);
    const std::uint32_t formId=0x14;memcpy(playerFixture+0x14,&formId,sizeof(formId));SetCell(false);
    const auto library=root/"versionlib-1-7-104-0.bin";
    WriteLibrary(library);
    if(mode==L"--missing-library")fs::remove(library);
    if(mode==L"--invalid-library")Write(library,"invalid");
    if(mode==L"--hook-conflict") {
        DWORD old{},ignored{};
        Check(VirtualProtect(const_cast<unsigned char*>(interfaceFixture),sizeof(interfaceFixture),PAGE_EXECUTE_READWRITE,&old)!=FALSE,"Fixture writable");
        *const_cast<unsigned char*>(interfaceFixture)=0x90;
        VirtualProtect(const_cast<unsigned char*>(interfaceFixture),sizeof(interfaceFixture),old,&ignored);
    }
    auto config=root/"CGE.ini";
    auto module=LoadLibraryExW((root/"CGE.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"Load CGE");
    for(const auto* name:{"ENBGetVersion","ENBGetSDKVersion","ENBGetParameter","DirtyHack","TwInit"})
        Check(!GetProcAddress(module,name),"No legacy renderer exports");
    Check(!GetModuleHandleW(L"KiENBExtender.dll") && !GetModuleHandleW(L"ENBHelperSE.dll"),"No external helpers");

    Check(!GetProcAddress(module,"D3D11CreateDeviceAndSwapChain"),"SKSE plugin does not export proxy entry points");
    auto query=Proc<bool(*)(const cge::skse::Interface*,cge::skse::Info*)>(module,"SKSEPlugin_Query");
    auto load=Proc<bool(*)(const cge::skse::Interface*)>(module,"SKSEPlugin_Load");
    auto* metadata=Proc<const cge::skse::Metadata*>(module,"SKSEPlugin_Version");
    Check(metadata->dataVersion==1 && metadata->compatibleVersions[0]==cge::skse::Runtime &&
        metadata->compatibleVersions[1]==0 && metadata->versionIndependence==0 && metadata->versionIndependenceEx==0,"Exact declared runtime, no false universal compatibility");
    cge::skse::Interface api{0x02030010,cge::skse::Runtime,0,0};
    cge::skse::Info info{};
    Check(!load(nullptr),"Reject null loader interface");
    auto wrong=api;wrong.runtimeVersion=0x01061170;
    Check(!query(&wrong,&info) && !load(&wrong),"Reject unsupported runtime");
    wrong=api;wrong.isEditor=1;Check(!load(&wrong),"Reject editor");
    Check(query(&api,&info) && info.infoVersion==1 && info.name,"Valid query metadata");
    Check(load(&api),"SKSE installs creation hook in the executable");
    Check(load(&api),"Repeat Load is idempotent");
    auto create=&D3D11CreateDeviceAndSwapChain;

    auto instance=GetModuleHandleW(nullptr);
    HWND window{};
    if(ansi) {
        WNDCLASSA wc{};wc.lpfnWndProc=Window;wc.hInstance=instance;wc.lpszClassName="CGEInputA";RegisterClassA(&wc);
        window=CreateWindowA(wc.lpszClassName,"CGE",WS_OVERLAPPEDWINDOW,0,0,640,720,nullptr,nullptr,instance,nullptr);
    }else {
        WNDCLASSW wc{};wc.lpfnWndProc=Window;wc.hInstance=instance;wc.lpszClassName=L"CGEInputW";RegisterClassW(&wc);
        window=CreateWindowW(wc.lpszClassName,L"CGE",WS_OVERLAPPEDWINDOW,0,0,640,720,nullptr,nullptr,instance,nullptr);
    }
    Check(window!=nullptr,"Create hidden window");
    ImportOverride focus(module,"GetForegroundWindow",PhysicalForeground);
    ImportOverride keyboard(module,"GetAsyncKeyState",PhysicalKey);
    ImportOverride clipRead(module,"GetClipCursor",NoClipRead);
    ImportOverride clipWrite(module,"ClipCursor",NoClipWrite);
    ImportOverride cursorRead(module,"GetCursorInfo",VisibleCursor);
    ImportOverride cursorWrite(module,"SetCursor",NoCursorChange);
    ImportOverride position(module,"GetCursorPos",PhysicalCursor);
    ImportOverride coordinates(module,"ScreenToClient",PhysicalToClient);
    ImportOverride clientArea(module,"GetClientRect",PhysicalClient);
    auto key=[&](WPARAM value){SendMessageW(window,WM_KEYDOWN,value,0);SendMessageW(window,WM_KEYUP,value,0);};
    for(unsigned cycle=0;cycle<(rejectScene?1u:10u);++cycle) {
        if(cycle==0)Write(config,"[CGE]\nEnabled=true\n[Color]\nExposureStops=-1\nContrast=1\nSaturation=1\n[Detail]\nSharpen=0\n");
        if(cycle==2)Write(config,"[CGE]\nEnabled=false\n[Color]\nExposureStops=2\n");
        if(cycle==3)Write(config,"[CGE]\nEnabled=true\n[Color]\nExposureStops=0\nSaturation=0\n");
        if(cycle==4)Write(config,"[CGE]\nEnabled=true\n[Color]\nContrast=0\n");
        if(cycle==5)Write(config,"[CGE]\nEnabled=true\n[Detail]\nSharpen=1\n");
        if(cycle==6)Write(config,"[Color]\nGamma=2\n");
        if(cycle==7)Write(config,"[Color]\nTemperature=100\n");
        if(cycle==8)Write(config,"[Color]\nTemperature=-100\n");
        if(cycle==9)Write(config,"[Color]\nExposureStops=0\n[Profiles]\nSeparate=true\nTransitionSeconds=0\n[Exterior]\nExposureStops=-1\n[Interior]\nExposureStops=1\n");
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=640;desc.BufferDesc.Height=720;
        desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;
        desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_SEQUENTIAL;
        ComPtr<IDXGISwapChain> chain;ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        Check(SUCCEEDED(create(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,nullptr,&context)),"Create WARP chain");
        Check((IsWindowUnicode(window)!=FALSE)==!ansi,"Preserve window encoding");
        ComPtr<ID3D11Texture2D> buffer;ComPtr<ID3D11RenderTargetView> target;
        auto buffers=[&] {Check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))),"Backbuffer");Check(SUCCEEDED(device->CreateRenderTargetView(buffer.Get(),nullptr,&target)),"Output view");};
        buffers();
        bool drawScene=!rejectScene;
        auto present=[&] {
            const float color[]={0.8f,0.4f,0.2f,0.3f};context->ClearRenderTargetView(target.Get(),color);
            if(cycle==5) {
                D3D11_TEXTURE2D_DESC td{};buffer->GetDesc(&td);
                std::vector<unsigned char> image(static_cast<size_t>(td.Width)*td.Height*4,102);
                for(size_t pixel=0;pixel<image.size()/4;++pixel)image[pixel*4+3]=77;
                auto offset=(static_cast<size_t>(td.Height/2)*td.Width+td.Width/2)*4;
                image[offset]=204;image[offset+1]=102;image[offset+2]=51;
                context->UpdateSubresource(buffer.Get(),0,nullptr,image.data(),td.Width*4,0);
            }
            auto* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
            D3D11_VIEWPORT sentinel={3,4,19,20,0,1};context->RSSetViewports(1,&sentinel);
            if(drawScene){InterfaceBoundary();InterfaceBoundary();}
            const unsigned char hud[]={17,231,93,77};
            D3D11_BOX hudRegion{0,0,0,1,1,1};
            context->UpdateSubresource(buffer.Get(),0,&hudRegion,hud,4,0);
            Check(SUCCEEDED(chain->Present(0,0)),"Present CGE");
            UINT n=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&n,&restored);
            Check(n==1 && restored.TopLeftX==3 && restored.Width==19,"Restore game viewport");
            ComPtr<ID3D11RenderTargetView> actual;context->OMGetRenderTargets(1,&actual,nullptr);
            Check(actual.Get()==target.Get(),"Restore game output binding");
        };
        auto pixel=[&](int r,int g,int b) {
            D3D11_TEXTURE2D_DESC td{};buffer->GetDesc(&td);td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&staging)),"Readback");
            context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
            Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Map pixels");
            auto* c=static_cast<unsigned char*>(mapped.pData)+mapped.RowPitch*(td.Height/2)+4*(td.Width/2);
            if(abs(int(c[0])-r)>2 || abs(int(c[1])-g)>2 || abs(int(c[2])-b)>2 || abs(int(c[3])-77)>2) {
                std::printf("Pixel=%u,%u,%u,%u expected=%d,%d,%d,77\n",c[0],c[1],c[2],c[3],r,g,b);Check(false,"CGE native pass pixels and alpha");
            }
            auto* hud=static_cast<unsigned char*>(mapped.pData);
            Check(hud[0]==17 && hud[1]==231 && hud[2]==93 && hud[3]==77,"HUD drawn after scene effects stays unchanged");
            context->Unmap(staging.Get(),0);
        };

        auto feedbackHash=[&] {
            D3D11_TEXTURE2D_DESC td{};buffer->GetDesc(&td);td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&staging)),"Feedback readback");
            context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
            Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Feedback map");
            std::uint64_t hash=1469598103934665603ull;
            for(unsigned y=346;y<374;++y)for(unsigned x=32;x<132;++x) {
                auto* c=static_cast<const unsigned char*>(mapped.pData)+mapped.RowPitch*y+4*x;
                for(unsigned channel=0;channel<3;++channel){hash^=c[channel];hash*=1099511628211ull;}
            }
            context->Unmap(staging.Get(),0);return hash;
        };
        auto captureMenu=[&](const fs::path& path) {
            if(ansi)return;
                D3D11_TEXTURE2D_DESC td{};buffer->GetDesc(&td);td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&staging)),"Menu preview texture");
                context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
                Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Menu preview map");
                BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
                info.biSize=sizeof(info);info.biWidth=static_cast<LONG>(td.Width);info.biHeight=-static_cast<LONG>(td.Height);
                info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
                header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(info);
                header.bfSize=header.bfOffBits+td.Width*td.Height*4;
                std::ofstream image(path,std::ios::binary);
                image.write(reinterpret_cast<const char*>(&header),sizeof(header));
                image.write(reinterpret_cast<const char*>(&info),sizeof(info));
                std::vector<unsigned char> row(td.Width*4);
                for(unsigned y=0;y<td.Height;++y) {
                    auto* source=static_cast<const unsigned char*>(mapped.pData)+mapped.RowPitch*y;
                    for(unsigned x=0;x<td.Width;++x){row[x*4]=source[x*4+2];row[x*4+1]=source[x*4+1];row[x*4+2]=source[x*4];row[x*4+3]=source[x*4+3];}
                    image.write(reinterpret_cast<const char*>(row.data()),static_cast<std::streamsize>(row.size()));
                }
                context->Unmap(staging.Get(),0);
        };
        present();
        if(rejectScene) {
            pixel(204,102,51);
            std::ifstream log(root/"CGE.log");std::string text((std::istreambuf_iterator<char>(log)),{});
            Check(text.find("scene hook unavailable")!=std::string::npos,"Missing/invalid/conflicting scene hook is reported");
        }else if(cycle==0) {
            pixel(102,51,26);
            drawScene=false;present();pixel(204,102,51);
            drawScene=true;present();pixel(102,51,26);physicalForeground=window;present();
            physicalKeys[VK_F8]=SHORT(0x8000);present();pixel(204,102,51);
            present();pixel(204,102,51);
            physicalKeys[VK_F8]=0;present();
            physicalKeys[VK_F8]=SHORT(0x8000);present();pixel(102,51,26);
            physicalKeys[VK_F8]=0;present();
            physicalKeys[VK_F8]=SHORT(0x8000);SendMessageW(window,WM_KEYDOWN,VK_F8,0);present();pixel(204,102,51);
            SendMessageW(window,WM_KEYDOWN,VK_F8,0);present();pixel(204,102,51);
            physicalKeys[VK_F8]=0;SendMessageW(window,WM_KEYUP,VK_F8,0);present();
            physicalKeys[VK_F8]=SHORT(0x8000);present();pixel(102,51,26);
            physicalKeys[VK_F8]=0;present();
            physicalKeys[VK_F10]=SHORT(0x8000);present();physicalKeys[VK_F10]=0;present();
            key('W');Check(forwarded==0,"Menu captures window keyboard input");
            auto click=[&](int x,int y) {
                physicalPointer={x*2,y*2};present();physicalKeys[VK_LBUTTON]=SHORT(0x8000);present();
                physicalKeys[VK_LBUTTON]=0;present();
            };

            auto setting=[&](const char* name) {
                std::ifstream input(config);std::string contents((std::istreambuf_iterator<char>(input)),{});
                auto at=contents.find(std::string(name)+"=");Check(at!=std::string::npos,"Saved setting exists");
                auto value=contents.substr(at+strlen(name)+1);
                if(value.starts_with("true"))return 1.0f;
                if(value.starts_with("false"))return 0.0f;
                return std::stof(value);
            };
            click(350,234);
            physicalKeys[VK_CONTROL]=physicalKeys['A']=SHORT(0x8000);present();
            physicalKeys[VK_CONTROL]=physicalKeys['A']=0;present();
            physicalKeys[VK_NUMPAD2]=SHORT(0x8000);present();
            physicalKeys[VK_NUMPAD2]=0;present();
            physicalKeys[VK_RETURN]=SHORT(0x8000);present();
            physicalKeys[VK_RETURN]=0;present();
            key(VK_F6);present();
            Check(setting("Gamma")==2,"Precise keyboard entry commits actual gamma");

            click(350,234);
            physicalKeys[VK_CONTROL]=physicalKeys['A']=SHORT(0x8000);present();
            physicalKeys[VK_CONTROL]=physicalKeys['A']=0;present();
            physicalKeys[VK_NUMPAD9]=SHORT(0x8000);
            SendMessageW(window,WM_KEYDOWN,VK_NUMPAD9,0);
            SendMessageW(window,WM_CHAR,'9',LPARAM(0x490000));present();
            physicalKeys[VK_NUMPAD9]=0;SendMessageW(window,WM_KEYUP,VK_NUMPAD9,0);present();
            physicalKeys[VK_RETURN]=SHORT(0x8000);present();
            physicalKeys[VK_RETURN]=0;present();
            key(VK_F6);present();Check(setting("Gamma")==2.5f,"Precise entry respects the allowed range");
            click(350,234);
            physicalKeys[VK_CONTROL]=physicalKeys['A']=SHORT(0x8000);present();
            physicalKeys[VK_CONTROL]=physicalKeys['A']=0;present();
            for(int digit:{VK_NUMPAD1,VK_DECIMAL,VK_NUMPAD2,VK_NUMPAD5}) {
                physicalKeys[digit]=SHORT(0x8000);
                if(digit==VK_NUMPAD1) {
                    SendMessageW(window,WM_KEYDOWN,digit,LPARAM(0x4f0000));
                    SendMessageW(window,WM_CHAR,'1',LPARAM(0x4f0000));
                }
                present();physicalKeys[digit]=0;present();
            }
            physicalKeys[VK_RETURN]=SHORT(0x8000);present();physicalKeys[VK_RETURN]=0;present();
            key(VK_F6);present();Check(setting("Gamma")==1.25f,"Physical numeric and decimal input works without WM_CHAR");
            click(350,234);
            physicalKeys[VK_CONTROL]=physicalKeys['A']=SHORT(0x8000);present();
            physicalKeys[VK_CONTROL]=physicalKeys['A']=0;present();
            for(wchar_t character:std::wstring(L"1.75")) {
                const auto mapped=VkKeyScanExW(character,GetKeyboardLayout(0));
                Check(mapped!=-1,"Current keyboard layout can type numeric value");
                const int vk=mapped&255;
                physicalKeys[VK_SHIFT]=(mapped&0x100)?SHORT(0x8000):0;
                physicalKeys[VK_CONTROL]=(mapped&0x200)?SHORT(0x8000):0;
                physicalKeys[VK_MENU]=(mapped&0x400)?SHORT(0x8000):0;
                physicalKeys[vk]=SHORT(0x8000);present();
                physicalKeys[vk]=physicalKeys[VK_SHIFT]=physicalKeys[VK_CONTROL]=physicalKeys[VK_MENU]=0;present();
            }
            physicalKeys[VK_RETURN]=SHORT(0x8000);present();physicalKeys[VK_RETURN]=0;present();
            // Let ImGui consume interleaved Shift/text/Enter events before saving.
            present();present();present();
            key(VK_F6);present();Check(setting("Gamma")==1.75f,"Number-row and punctuation follow the active keyboard layout");
            click(432,234);key(VK_F6);present();
            Check(setting("Gamma")==1 && setting("ExposureStops")==-1,"Per-control reset leaves other values intact");
            click(200,310);
            physicalKeys[VK_F6]=SHORT(0x8000);present();
            physicalKeys[VK_F6]=0;present();
            physicalKeys[VK_F11]=SHORT(0x8000);present();
            physicalKeys[VK_F11]=0;present();
            key(VK_F6);present();
            Check(setting("ToggleKey")==122,"Custom compare key is saved; reserved keys do not finish binding");
            Check(setting("Enabled")==1,"Binding capture does not accidentally toggle effects");

            click(200,310);key(VK_ESCAPE);present();key(VK_F6);present();
            Check(setting("ToggleKey")==122,"Escape cancels rebinding without changing the shortcut");
            click(44,74); // The visible On/Off button bypasses the entire pass.

            key(VK_F6);present();
            std::ifstream file(config);std::string saved((std::istreambuf_iterator<char>(file)),{});
            Check(saved.find("Enabled=false")!=std::string::npos,"Own checkbox and Save change CGE.ini");

            file.close();
            const auto savedFeedback=feedbackHash();
            const auto attributes=GetFileAttributesW(config.c_str());
            Check(attributes!=INVALID_FILE_ATTRIBUTES && SetFileAttributesW(config.c_str(),attributes|FILE_ATTRIBUTE_READONLY),"Make test settings read-only");
            key(VK_F6);present();
            const auto failedFeedback=feedbackHash();
            const bool temporaryRemoved=!fs::exists(config.wstring()+L".tmp");
            Check(SetFileAttributesW(config.c_str(),attributes)!=FALSE,"Restore test settings attributes");
            Check(failedFeedback!=savedFeedback,"Save failure changes visible button feedback");
            Check(temporaryRemoved && setting("Enabled")==0,"Failed save preserves settings and cleans its temporary file");
            key(VK_F6);present();
            Check(feedbackHash()!=failedFeedback,"Successful retry clears failure feedback");
            captureMenu(root/L"CGE-menu.bmp");
            key(VK_ESCAPE);present();pixel(204,102,51);
            physicalKeys[VK_F11]=SHORT(0x8000);present();pixel(102,51,26);
            present();pixel(102,51,26);
            physicalKeys[VK_F11]=0;present();
            physicalKeys[VK_F11]=SHORT(0x8000);present();pixel(204,102,51);
            physicalKeys[VK_F11]=0;present();
            physicalKeys[VK_F8]=SHORT(0x8000);present();pixel(204,102,51);
            physicalKeys[VK_F8]=0;present();
            key('W');Check(forwarded==1,"Close releases window keyboard input");
            physicalForeground=nullptr;
        }else if(cycle==1) {
            pixel(204,102,51);key(VK_F10);key(VK_F7);present();key(VK_ESCAPE);present();pixel(204,102,51);
        }else if(cycle==2)pixel(204,102,51);
        else if(cycle==3)pixel(120,120,120);
        else if(cycle==4)pixel(128,128,128);
        else if(cycle==5)pixel(255,102,0);
        else if(cycle==6)pixel(228,161,114);
        else if(cycle==7)pixel(243,102,43);
        else if(cycle==8)pixel(172,102,61);
        else {
            pixel(102,51,26);
            SetCell(true);present();pixel(255,204,102);
            void* missing=nullptr;memcpy(playerFixture+0x60,&missing,sizeof(missing));
            present();pixel(255,204,102);
            SetCell(false);present();pixel(102,51,26);
            physicalForeground=window;key(VK_F10);present();
            auto clickProfile=[&](int x,int y) {
                physicalPointer={x*2,y*2};present();physicalKeys[VK_LBUTTON]=SHORT(0x8000);present();
                physicalKeys[VK_LBUTTON]=0;present();
            };
            clickProfile(360,110);clickProfile(350,144);
            physicalKeys[VK_CONTROL]=physicalKeys['A']=SHORT(0x8000);present();
            physicalKeys[VK_CONTROL]=physicalKeys['A']=0;present();
            for(int digit:{VK_SUBTRACT,VK_NUMPAD2}) {
                physicalKeys[digit]=SHORT(0x8000);present();physicalKeys[digit]=0;present();
            }
            physicalKeys[VK_RETURN]=SHORT(0x8000);present();physicalKeys[VK_RETURN]=0;present();
            present();present();key(VK_F6);present();
            std::ifstream savedFile(config);std::string contents((std::istreambuf_iterator<char>(savedFile)),{});
            Check(contents.find("[Interior]\nExposureStops=-2")!=std::string::npos &&
                contents.find("[Exterior]\nExposureStops=-1")!=std::string::npos,"Editing and saving one profile leaves the other unchanged");
            captureMenu(root/L"CGE-profiles-menu.bmp");
            key(VK_ESCAPE);present();pixel(102,51,26);
            SetCell(true);present();pixel(51,26,13);
            playerSingletonFixture=nullptr;present();pixel(51,26,13);
            playerSingletonFixture=playerFixture;
            void* invalid=reinterpret_cast<void*>(1);memcpy(playerFixture+0x60,&invalid,sizeof(invalid));
            present();pixel(51,26,13);
            SetCell(false);present();pixel(102,51,26);
            key(VK_F10);present();
            clickProfile(350,74);key(VK_ESCAPE);present();pixel(204,102,51);
            key(VK_F10);present();clickProfile(350,74);key(VK_ESCAPE);present();pixel(102,51,26);
            physicalForeground=nullptr;
        }
        context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();buffer.Reset();
        Check(SUCCEEDED(chain->ResizeBuffers(1,80,48,DXGI_FORMAT_UNKNOWN,0)),"Resize without retained game resources");
        buffers();present();
        if(cycle==3)pixel(120,120,120);
        if(cycle==5)pixel(255,102,0);
        context->ClearState();target.Reset();buffer.Reset();chain.Reset();
        Check(reinterpret_cast<WNDPROC>(ansi?GetWindowLongPtrA(window,GWLP_WNDPROC):GetWindowLongPtrW(window,GWLP_WNDPROC))==Window,"Restore original procedure");
        context.Reset();device.Reset();
    }
    DestroyWindow(window);
    std::puts("PASS: SKSE load, executable IAT interception, CGE native pixels, alpha, bypass, saturation, state, resize, own menu, Save and reset.");
}

