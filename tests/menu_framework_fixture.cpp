                                                                      
#include <windows.h>
#include <string>
#include <vector>
#include <utility>
#include <cmath>
using Page=void(__stdcall*)();
struct Vec2 {float x,y;};
namespace {
std::vector<std::pair<std::string,Page>> pages;
float version=3.8f;
bool blocking{},disabled{};
std::string id,actionLabel;
int action{};float value{};
bool Match(int type,const char* label) {
    if(!disabled && action==type && actionLabel==label){action=0;return true;}return false;
}
}
extern "C" {
__declspec(dllexport) void AddSectionItem(const char* path,Page page){pages.emplace_back(path,page);}
__declspec(dllexport) float GetMenuFrameworkVersion(){return version;}
__declspec(dllexport) bool IsAnyBlockingWindowOpened(){return blocking;}
__declspec(dllexport) bool igCheckbox(const char* label,bool* p){if(Match(1,label)){*p=value!=0;return true;}return false;}
#ifndef CGE_INCOMPLETE_FRAMEWORK
__declspec(dllexport) bool igSliderFloat(const char*,float* p,float,float,const char*,int) {
    if(Match(2,id.c_str())){*p=value;return true;}return false;
}
#endif
__declspec(dllexport) bool igInputFloat(const char*,float* p,float,float,const char*,int) {
    if(Match(3,id.c_str())){*p=value;return true;}return false;
}
__declspec(dllexport) bool igButton(const char* label,Vec2){return Match(5,label);}
__declspec(dllexport) bool igCombo_Str_arr(const char* label,int* p,const char* const*,int,int) {
    if(Match(4,label)){*p=static_cast<int>(value);return true;}return false;
}
__declspec(dllexport) void igTextUnformatted(const char*,const char*){}
__declspec(dllexport) void igSeparator(){}
__declspec(dllexport) void igSameLine(float,float){}
__declspec(dllexport) void igSetNextItemWidth(float){}
__declspec(dllexport) void igPushID_Str(const char* label){id=label;}
__declspec(dllexport) void igPopID(){id.clear();}
__declspec(dllexport) void igBeginDisabled(bool p){disabled=p;}
__declspec(dllexport) void igEndDisabled(){disabled=false;}
__declspec(dllexport) void TestSetVersion(float p){version=p;}
__declspec(dllexport) void TestSetBlocking(bool p){blocking=p;}
__declspec(dllexport) void TestAction(int type,const char* label,float p){action=type;actionLabel=label;value=p;}
__declspec(dllexport) unsigned TestPageCount(){return static_cast<unsigned>(pages.size());}
__declspec(dllexport) bool TestRender(const char* path) {
    for(auto& page:pages)if(page.first==path){page.second();return true;}return false;
}
}
