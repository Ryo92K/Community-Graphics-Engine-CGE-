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
int wmain(int argc,wchar_t** argv) {
    Check(argc==4,"DLL, fixture and window mode");
    bool ansi=std::wstring(argv[3])==L"--ansi";
    fs::path root=fs::absolute(argv[2]);fs::create_directories(root);
    fs::copy_file(argv[1],root/"d3d11.dll",fs::copy_options::overwrite_existing);
    auto config=root/"CGE.ini";
    auto module=LoadLibraryExW((root/"d3d11.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"Load CGE");
    for(const auto* name:{"ENBGetVersion","ENBGetSDKVersion","ENBGetParameter","DirtyHack","TwInit"})
        Check(!GetProcAddress(module,name),"No legacy renderer exports");
    Check(!GetModuleHandleW(L"KiENBExtender.dll") && !GetModuleHandleW(L"ENBHelperSE.dll"),"No external helpers");
    auto scene=Proc<void(*)()>(module,"CGE_TestScene");
    auto create=Proc<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(module,"D3D11CreateDeviceAndSwapChain");
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
    for(unsigned cycle=0;cycle<9;++cycle) {
        if(cycle==0)Write(config,"[CGE]\nEnabled=true\n[Color]\nExposureStops=-1\nContrast=1\nSaturation=1\n[Detail]\nSharpen=0\n");
        if(cycle==2)Write(config,"[CGE]\nEnabled=false\n[Color]\nExposureStops=2\n");
        if(cycle==3)Write(config,"[CGE]\nEnabled=true\n[Color]\nExposureStops=0\nSaturation=0\n");
        if(cycle==4)Write(config,"[CGE]\nEnabled=true\n[Color]\nContrast=0\n");
        if(cycle==5)Write(config,"[CGE]\nEnabled=true\n[Detail]\nSharpen=1\n");
        if(cycle==6)Write(config,"[Color]\nGamma=2\n");
        if(cycle==7)Write(config,"[Color]\nTemperature=100\n");
        if(cycle==8)Write(config,"[Color]\nTemperature=-100\n");
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
            scene();
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
            }context->Unmap(staging.Get(),0);
        };
        present();
        if(cycle==0) {
            pixel(102,51,26);physicalForeground=window;present();
            physicalKeys[VK_F10]=SHORT(0x8000);present();physicalKeys[VK_F10]=0;present();
            key('W');Check(forwarded==0,"Menu captures window keyboard input");
            auto click=[&](int x,int y) {
                physicalPointer={x*2,y*2};present();physicalKeys[VK_LBUTTON]=SHORT(0x8000);present();
                physicalKeys[VK_LBUTTON]=0;present();
            };
            click(44,74); // Checkbox is first control, relative to the title bar.
            key(VK_F6);present();
            std::ifstream file(config);std::string saved((std::istreambuf_iterator<char>(file)),{});
            Check(saved.find("Enabled=false")!=std::string::npos,"Own checkbox and Save change CGE.ini");
            key(VK_ESCAPE);present();pixel(204,102,51);
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
        else pixel(172,102,61);
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
    std::puts("PASS: CGE native pixels, alpha, bypass, saturation, state, resize, own menu, Save and reset.");
}
