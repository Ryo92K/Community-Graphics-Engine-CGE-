#include <string_view>
#include "menu_framework.h"
#include "graphics_host.h"
#include "presets.h"
#include "finishing_settings.h"
#include <optional>
#include <windows.h>
#include <algorithm>
#include <cmath>
namespace cge::menu {
namespace {
                                                                           
                                                                                
struct Vec2 {float x{},y{};};
using Page=void(__stdcall*)();
struct Api {
    void (*addPage)(const char*,Page){};
    float (*version)(){};
    bool (*blocking)(){};
    bool (*checkbox)(const char*,bool*){};
    bool (*slider)(const char*,float*,float,float,const char*,int){};
    bool (*input)(const char*,float*,float,float,const char*,int){};
    bool (*button)(const char*,Vec2){};
    bool (*combo)(const char*,int*,const char* const*,int,int){};
    void (*text)(const char*,const char*){};
    void (*separator)(){};
    void (*sameLine)(float,float){};
    void (*width)(float){};
    void (*pushId)(const char*){};
    void (*popId)(){};
    void (*beginDisabled)(bool){};
    void (*endDisabled)(){};
};
Api api;
bool registered{},dirty{};
int selectedProfile{};
std::optional<Settings> beforePreset;
const char* appliedPreset{};
enum class Feedback {None,Saved,Failed};
Feedback feedback{};
template<class T>bool Bind(HMODULE module,const char* name,T& target) {
    target=reinterpret_cast<T>(GetProcAddress(module,name));return target!=nullptr;
}
void Changed(){dirty=true;feedback=Feedback::None;appliedPreset=nullptr;}
void Text(const char* text){api.text(text,nullptr);}
bool Button(const char* label){return api.button(label,{});}
void Control(const char* label,float& value,float low,float high,float neutral) {
    Text(label);
    api.pushId(label);
    api.width(-190);
    float next=value;
    bool edited=api.slider("##Slider",&next,low,high,"",0);
    api.sameLine(0,-1);api.width(90);
    edited=api.input("##Value",&next,0,0,"%.3f",0) || edited;
    api.sameLine(0,-1);
    if(Button("Reset")){next=neutral;edited=true;}
    if(edited && std::isfinite(next)){value=std::clamp(next,low,high);Changed();}
    api.popId();
}
void Footer(Settings& settings,bool general) {
    graphics::ApplySettings(settings);
    api.separator();
    if(Button("Save")) {
        if(graphics::SaveSettings()){feedback=Feedback::Saved;dirty=false;}
        else feedback=Feedback::Failed;
    }
    if(general) {
        api.sameLine(0,-1);
        if(Button("Reset All")){settings=Settings{};selectedProfile=0;beforePreset.reset();graphics::ApplySettings(settings);Changed();}
    }
    if(feedback==Feedback::Saved) {api.sameLine(0,-1);Text("Saved");}
    else if(feedback==Feedback::Failed){api.sameLine(0,-1);Text("Save failed: CGE.ini could not be written.");}
    else if(dirty){api.sameLine(0,-1);Text("Unsaved changes");}
}
void Render(unsigned page) noexcept {
    try {
        graphics::SettingsView view;
        if(!graphics::ReadSettings(view)){Text("Renderer not ready");return;}
        auto settings=view.settings;
        if(api.checkbox("Effects enabled",&settings.enabled))Changed();
        api.separator();
        if(page==0) {
            Text("Bloom");api.sameLine(0,-1);
            Text(view.bloom.empty()?"HDR pass not reached":view.bloom.c_str());
            Text("Motion Blur");api.sameLine(0,-1);Text(MotionLabel(view.motion));
            Text("Distance Fog");api.sameLine(0,-1);Text(view.fog.empty()?"HDR pass not reached":view.fog.c_str());
            Text("Depth of Field");api.sameLine(0,-1);Text(view.dof.empty()?"HDR pass not reached":view.dof.c_str());
        }else if(page==7) {
            Text("Presets replace all CGE settings. Save to keep changes.");
            const auto choices=presets::List();
            for(std::size_t i=0;i<choices.size();++i) {
                if(Button(choices[i].name)) {
                    const auto previous=settings;
                    if(presets::Apply(i,settings)) {
                        beforePreset=previous;selectedProfile=0;Changed();appliedPreset=choices[i].name;
                        graphics::ApplySettings(settings,true);
                    }
                }
                Text(choices[i].description);
            }
            api.separator();
            api.beginDisabled(!beforePreset.has_value());
            if(Button("Undo Preset") && beforePreset) {
                settings=*beforePreset;beforePreset.reset();selectedProfile=0;Changed();
                graphics::ApplySettings(settings,true);
            }
            api.endDisabled();
            if(appliedPreset){Text("Applied");api.sameLine(0,-1);Text(appliedPreset);}
        }else if(page==1) {
            bool automatic=settings.separateProfiles;
            if(api.checkbox("Automatic profiles",&automatic)){settings.SetSeparateProfiles(automatic);Changed();}
            api.beginDisabled(!settings.enabled || !settings.separateProfiles);
            Control("Transition (sec)",settings.transitionSeconds,0,5,1.5f);
            Control("Dawn (hour)",settings.dayHour,0,11,6);
            Control("Dusk (hour)",settings.nightHour,13,23,20);
            api.endDisabled();
        }else if(page==4) {
            if(api.checkbox("Motion blur enabled",&settings.motion.enabled))Changed();
            api.beginDisabled(!settings.enabled || !settings.motion.enabled);
            Control("Intensity",settings.motion.strength,0,2,0.5f);
            Control("Blur Radius (px)",settings.motion.radius,1,64,24);
            constexpr const char* choices[]={"8 samples","12 samples","16 samples"};
            int quality=settings.motion.samples==8?0:settings.motion.samples==16?2:1;
            if(api.combo("Quality",&quality,choices,3,-1) && quality>=0 && quality<3) {
                settings.motion.samples=quality==0?8:quality==2?16:12;Changed();
            }
            if(Button("Natural")){settings.motion={true,0.5f,24,12};Changed();}
            api.sameLine(0,-1);
            if(Button("Strong")){settings.motion={true,1.5f,48,16};Changed();}
            api.endDisabled();
            Text(MotionLabel(view.motion));
        }else {
            constexpr const char* choices[]={"Shared","Interior","Exterior Day","Exterior Night"};
            Text("Edit Profile");
            api.width(-1);
            int selection=selectedProfile;
            if(api.combo("##Profile",&selection,choices,4,-1) && selection>=0 && selection<4)selectedProfile=selection;
            api.separator();
            auto* profile=static_cast<ColorSettings*>(&settings);
            if(selectedProfile==1)profile=&settings.interior;
            else if(selectedProfile==2)profile=&settings.exterior;
            else if(selectedProfile==3)profile=&settings.exteriorNight;
            const bool disabled=selectedProfile>0 && !settings.separateProfiles;
            if(disabled)Text("Enable Automatic profiles on the Profiles page to edit this profile.");
            api.beginDisabled(disabled);
            if(page==2) {
                api.beginDisabled(!settings.enabled);
                Control("Exposure",profile->exposure,-3,3,0);
                Control("Contrast",profile->contrast,0,2,1);
                Control("Saturation",profile->saturation,0,2,1);
                Control("Sharpness",profile->sharpen,0,1,0);
                Control("Gamma",profile->gamma,0.5f,2.5f,1);
                Control("Color Temperature",profile->temperature,-100,100,0);
                if(Button("Reset Color")) {
                    const auto bloom=profile->bloom;const auto fog=profile->fog;const auto dof=profile->dof;const auto clarity=profile->clarity;const auto vibrance=profile->vibrance;
                    const auto vignette=profile->vignette;const auto grain=profile->filmGrain;
                    const auto split=profile->splitToning;const auto aberration=profile->chromaticAberration;
                    *profile=ColorSettings{};profile->bloom=bloom;profile->fog=fog;profile->dof=dof;profile->clarity=clarity;profile->vibrance=vibrance;
                    profile->vignette=vignette;profile->filmGrain=grain;profile->splitToning=split;
                    profile->chromaticAberration=aberration;Changed();
                }
                api.endDisabled();
            }else if(page>=10 && page<=13) {
                constexpr const char* keys[]={"Vignette","FilmGrain","SplitToning","ChromaticAberration"};
                constexpr const char* names[]={"Vignette","Film Grain","Split Toning","Chromatic Aberration"};
                const auto* key=keys[page-10];
                FinishingEffects(*profile,[&](const char* effect,bool& value) {
                    if(std::string_view(effect)==key && api.checkbox((std::string(names[page-10])+" enabled").c_str(),&value))Changed();
                });
                bool enabled=false;
                FinishingEffects(*profile,[&](const char* effect,bool value){if(std::string_view(effect)==key)enabled=value;});
                api.beginDisabled(!settings.enabled || !enabled);
                FinishingParameters(*profile,[&](const char* effect,auto,const char* label,float& value,float low,float high,float neutral) {
                    if(std::string_view(effect)==key)Control(label,value,low,high,neutral);
                });
                if(Button((std::string("Reset ")+names[page-10]).c_str())) {
                    FinishingEffects(*profile,[&](const char* effect,bool& value){if(std::string_view(effect)==key)value=false;});
                    FinishingParameters(*profile,[&](const char* effect,auto,auto,float& value,auto,auto,float neutral) {
                        if(std::string_view(effect)==key)value=neutral;
                    });
                    Changed();
                }
                api.endDisabled();
            }else if(page==9) {
                if(api.checkbox("Vibrance enabled",&profile->vibrance.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->vibrance.enabled);
                Control("Strength",profile->vibrance.strength,0,1,0.25f);
                if(Button("Reset Vibrance")){profile->vibrance=VibranceSettings{};Changed();}
                api.endDisabled();
            }else if(page==8) {
                if(api.checkbox("Clarity enabled",&profile->clarity.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->clarity.enabled);
                Control("Strength",profile->clarity.strength,0,1,0.25f);
                if(Button("Reset Clarity")){profile->clarity=ClaritySettings{};Changed();}
                api.endDisabled();
            }else if(page==6) {
                if(api.checkbox("Depth of field enabled",&profile->dof.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->dof.enabled);
                Control("Focus Distance (units)",profile->dof.focusDistance,1,50000,2000);
                Control("Blur Strength",profile->dof.strength,0,1,0.5f);
                if(Button("Reset Depth of Field")){profile->dof=DofSettings{};Changed();}
                api.endDisabled();
            }else if(page==5) {
                if(api.checkbox("Fog enabled",&profile->fog.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->fog.enabled);
                Control("Density",profile->fog.density,0,1,0.35f);
                Control("Start Distance (units)",profile->fog.startDistance,0,50000,2000);
                if(Button("Reset Fog")){profile->fog=FogSettings{};Changed();}
                api.endDisabled();
            }else {
                if(api.checkbox("Bloom enabled",&profile->bloom.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->bloom.enabled);
                Control("Intensity",profile->bloom.intensity,0,2,0.15f);
                Control("Threshold",profile->bloom.threshold,0,1,1);
                Control("Radius",profile->bloom.radius,1,64,32);
                if(Button("Reset Bloom")){profile->bloom=BloomSettings{};Changed();}
                api.endDisabled();
            }
            api.endDisabled();
            if(page==3 && !view.bloom.empty())Text(view.bloom.c_str());
            if(page==5 && !view.fog.empty())Text(view.fog.c_str());
            if(page==6 && !view.dof.empty())Text(view.dof.c_str());
        }
        Footer(settings,page==0);
    }catch(...){                                                                    }
}
void __stdcall General(){Render(0);}
void __stdcall Profiles(){Render(1);}
void __stdcall Color(){Render(2);}
void __stdcall Bloom(){Render(3);}
void __stdcall Motion(){Render(4);}
void __stdcall Fog(){Render(5);}
void __stdcall Dof(){Render(6);}
void __stdcall Presets(){Render(7);}
void __stdcall Clarity(){Render(8);}
void __stdcall Vibrance(){Render(9);}
void __stdcall Vignette(){Render(10);}
void __stdcall FilmGrain(){Render(11);}
void __stdcall SplitToning(){Render(12);}
void __stdcall ChromaticAberration(){Render(13);}
}
bool Register(std::string& status) {
    if(registered){status="CGE SKSE Menu Framework already registered";return true;}
    auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
    if(!module){status="CGE menu unavailable: install SKSE Menu Framework 3.14 or newer; INI effects remain available";return false;}
    Api candidate;
    const bool complete=
        Bind(module,"AddSectionItem",candidate.addPage) &&
        Bind(module,"GetMenuFrameworkVersion",candidate.version) &&
        Bind(module,"IsAnyBlockingWindowOpened",candidate.blocking) &&
        Bind(module,"igCheckbox",candidate.checkbox) &&
        Bind(module,"igSliderFloat",candidate.slider) &&
        Bind(module,"igInputFloat",candidate.input) &&
        Bind(module,"igButton",candidate.button) &&
        Bind(module,"igCombo_Str_arr",candidate.combo) &&
        Bind(module,"igTextUnformatted",candidate.text) &&
        Bind(module,"igSeparator",candidate.separator) &&
        Bind(module,"igSameLine",candidate.sameLine) &&
        Bind(module,"igSetNextItemWidth",candidate.width) &&
        Bind(module,"igPushID_Str",candidate.pushId) &&
        Bind(module,"igPopID",candidate.popId) &&
        Bind(module,"igBeginDisabled",candidate.beginDisabled) &&
        Bind(module,"igEndDisabled",candidate.endDisabled);
    if(!complete){status="CGE menu unavailable: SKSE Menu Framework exports are incomplete";return false;}
                                                                           
                                                                                
    const float version=candidate.version();
    if(!std::isfinite(version) || version<3.0f) {
        status="CGE menu unavailable: SKSE Menu Framework 3 API required; update the framework";return false;
    }
    api=candidate;
    constexpr const char* paths[]={
        "Community Graphics Engine/General","Community Graphics Engine/Presets","Community Graphics Engine/Profiles",
        "Community Graphics Engine/Effects/Color","Community Graphics Engine/Effects/Bloom",
        "Community Graphics Engine/Effects/Distance Fog","Community Graphics Engine/Effects/Depth of Field",
        "Community Graphics Engine/Effects/Motion Blur","Community Graphics Engine/Effects/Clarity","Community Graphics Engine/Effects/Vibrance",
        "Community Graphics Engine/Effects/Vignette","Community Graphics Engine/Effects/Film Grain",
        "Community Graphics Engine/Effects/Split Toning","Community Graphics Engine/Effects/Chromatic Aberration"};
    const Page pages[]={General,Presets,Profiles,Color,Bloom,Fog,Dof,Motion,Clarity,Vibrance,Vignette,FilmGrain,SplitToning,ChromaticAberration};
    for(unsigned i=0;i<14;++i)api.addPage(paths[i],pages[i]);
    registered=true;
    status="CGE SKSE Menu Framework ready: 14 pages registered; legacy menu and hotkeys removed";
    return true;
}
bool IsBlocking() noexcept {return registered && api.blocking();}
}
