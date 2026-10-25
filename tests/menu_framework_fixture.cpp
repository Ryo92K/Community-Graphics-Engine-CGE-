                                                                      
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
std::vector<bool> disabledStack;
struct ObservedControl {int type;std::string label;bool disabled;};
std::vector<ObservedControl> controls;
std::string id,actionLabel;
int action{};float value{};
bool Match(int type,const char* label) {
    controls.push_back({type,label,disabled});
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
__declspec(dllexport) void igBeginDisabled(bool p){disabledStack.push_back(disabled);disabled=disabled || p;}
__declspec(dllexport) void igEndDisabled(){if(disabledStack.empty())return;disabled=disabledStack.back();disabledStack.pop_back();}
__declspec(dllexport) void TestSetVersion(float p){version=p;}
__declspec(dllexport) void TestSetBlocking(bool p){blocking=p;}
__declspec(dllexport) void TestAction(int type,const char* label,float p){action=type;actionLabel=label;value=p;}
__declspec(dllexport) int TestPageIndex(const char* path) {
    for(std::size_t i=0;i<pages.size();++i)if(pages[i].first==path)return static_cast<int>(i);
    return -1;
}
__declspec(dllexport) unsigned TestPageCount(){return static_cast<unsigned>(pages.size());}
__declspec(dllexport) int TestControlDisabled(int type,const char* label) {
    for(auto it=controls.rbegin();it!=controls.rend();++it)if(it->type==type && it->label==label)return it->disabled?1:0;
    return -1;
}
__declspec(dllexport) bool TestRender(const char* path) {
    for(auto& page:pages)if(page.first==path){
        if(disabled || !disabledStack.empty())return false;
        controls.clear();page.second();return !disabled && disabledStack.empty();
    }return false;
}
}
