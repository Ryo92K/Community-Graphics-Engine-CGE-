#include "input_capture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using State=HRESULT(WINAPI*)(void*,DWORD,LPVOID);
using Events=HRESULT(WINAPI*)(void*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
bool capture=false,fail=false,flushed=false;
bool Capture(){return capture;}
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
HRESULT WINAPI OriginalState(void*,DWORD size,LPVOID data) {
    if(fail)return DIERR_NOTACQUIRED;
    memset(data,0x81,size);return DI_OK;
}
HRESULT WINAPI OriginalEvents(void*,DWORD,LPDIDEVICEOBJECTDATA data,LPDWORD size,DWORD flags) {
    if(fail)return DIERR_NOTACQUIRED;
    flushed=!data && *size==INFINITE && flags==0;*size=flushed?4:1;return DI_OK;
}
int main() {
    void* table[11]{};table[9]=reinterpret_cast<void*>(OriginalState);table[10]=reinterpret_cast<void*>(OriginalEvents);
    struct Device {void** table;} device{table};
    Check(cge::input::HookDeviceTable(table),"Install on isolated table");cge::input::Configure(Capture);
    auto state=reinterpret_cast<State>(table[9]);auto events=reinterpret_cast<Events>(table[10]);
    unsigned char keys[256]{};
    Check(state(&device,sizeof(keys),keys)==DI_OK && keys[42]==0x81,"Closed menu preserves input");
    capture=true;Check(state(&device,sizeof(keys),keys)==DI_OK && keys[42]==0,"Open menu neutralizes input");
    fail=true;Check(state(&device,sizeof(keys),keys)==DIERR_NOTACQUIRED,"Preserve device errors");
    DWORD count=10;DIDEVICEOBJECTDATA data[10]{};
    Check(events(&device,sizeof(data[0]),data,&count,0)==DIERR_NOTACQUIRED,"Buffered error preserved");
    fail=false;count=10;
    Check(events(&device,sizeof(data[0]),data,&count,DIGDD_PEEK)==DI_OK && count==0 && flushed,"Flush buffered game events while menu open");
    capture=false;count=10;
    Check(events(&device,sizeof(data[0]),data,&count,0)==DI_OK && count==1,"Closed menu restores buffered input");
    std::string status;
    Check(cge::input::Install(Capture,status),"Install native A/W keyboard/mouse hooks without acquiring input devices");
    std::puts(status.c_str());cge::input::Shutdown();
    Check(table[9]==reinterpret_cast<void*>(OriginalState) && table[10]==reinterpret_cast<void*>(OriginalEvents),"Restore original device table");
    std::puts("PASS: independent DirectInput capture and release.");
}
