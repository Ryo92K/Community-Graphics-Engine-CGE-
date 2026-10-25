#include <string_view>
#include "menu_framework.h"
#include "graphics_host.h"
#include "presets.h"
#include "finishing_settings.h"
#include "lut.h"
#include "profiles.h"
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
int selectedProfile{-1};
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
            Text("SSAO (BÊTA)");api.sameLine(0,-1);Text(view.ssao.empty()?"HDR pass not reached":view.ssao.c_str());
            Text("Bloom");api.sameLine(0,-1);
            Text(view.bloom.empty()?"HDR pass not reached":view.bloom.c_str());
            Text("Motion Blur");api.sameLine(0,-1);Text(MotionLabel(view.motion));
            Text("Distance Fog");api.sameLine(0,-1);Text(view.fog.empty()?"HDR pass not reached":view.fog.c_str());
            Text("Depth of Field");api.sameLine(0,-1);Text(view.dof.empty()?"HDR pass not reached":view.dof.c_str());
        }else if(page==7) {
            Text("Presets replace visual settings and keep Motion Blur. Save to keep changes.");
            const auto choices=presets::List();
            for(std::size_t i=0;i<choices.size();++i) {
                if(Button(choices[i].name)) {
                    const auto previous=settings;
                    if(presets::Apply(i,settings)) {
                        beforePreset=previous;selectedProfile=-1;Changed();appliedPreset=choices[i].name;
                        graphics::ApplySettings(settings,true);
                    }
                }
                Text(choices[i].description);
            }
            api.separator();
            api.beginDisabled(!beforePreset.has_value());
            if(Button("Undo Preset") && beforePreset) {
                settings=*beforePreset;beforePreset.reset();selectedProfile=-1;Changed();
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
            Control("Intensity",settings.motion.strength,0,4,1);
            Control("Blur Radius (px)",settings.motion.radius,1,64,24);
            constexpr const char* choices[]={"8 samples","12 samples","16 samples"};
            int quality=settings.motion.samples==8?0:settings.motion.samples==16?2:1;
            if(api.combo("Quality",&quality,choices,3,-1) && quality>=0 && quality<3) {
                settings.motion.samples=quality==0?8:quality==2?16:12;Changed();
            }
            if(Button("Natural")){settings.motion={true,1.0f,24,12};Changed();}
            api.sameLine(0,-1);
            if(Button("Strong")){settings.motion={true,2.5f,48,16};Changed();}
            api.endDisabled();
            Text(MotionLabel(view.motion));
        }else {
            constexpr const char* choices[]={"Shared","Interior","Exterior Day","Exterior Night"};
            Text("Edit Profile");
            api.width(-1);
            int active=0;
            if(settings.separateProfiles && view.environment==Environment::Interior)active=1;
            else if(settings.separateProfiles && view.environment==Environment::Exterior)active=ValidGameHour(view.hour) && NightWeight(view.hour,settings.dayHour,settings.nightHour)>0.5f?3:2;
            int editing=selectedProfile<0?active:selectedProfile;
            int selection=editing;
            if(api.combo("##Profile",&selection,choices,4,-1) && selection>=0 && selection<4){selectedProfile=editing=selection;}
            if(settings.separateProfiles && editing==0)Text("Shared is inactive while Automatic profiles are enabled.");
            else if(settings.separateProfiles && active>0 && editing!=active){Text("Active profile");api.sameLine(0,-1);Text(choices[active]);}
            api.separator();
            auto* profile=static_cast<ColorSettings*>(&settings);
            if(editing==1)profile=&settings.interior;
            else if(editing==2)profile=&settings.exterior;
            else if(editing==3)profile=&settings.exteriorNight;
            const bool disabled=editing>0 && !settings.separateProfiles;
            if(disabled)Text("Enable Automatic profiles on the Profiles page to edit this profile.");
            api.beginDisabled(disabled);
            if(page==2) {
                api.beginDisabled(!settings.enabled);
                Control("Exposure",profile->exposure,-3,3,0);
                Control("Contrast",profile->contrast,0,2,1);
                Control("Saturation",profile->saturation,0,2,1);
                Control("Gamma",profile->gamma,0.5f,2.5f,1);
                Control("Color Temperature",profile->temperature,-100,100,0);
                if(Button("Reset Color")) {
                    const auto sharpen=profile->sharpen;const auto sharpnessEnabled=profile->sharpnessEnabled;
                    const auto ssao=profile->ssao;const auto bloom=profile->bloom;const auto fog=profile->fog;const auto dof=profile->dof;const auto clarity=profile->clarity;const auto vibrance=profile->vibrance;
                    const auto vignette=profile->vignette;const auto grain=profile->filmGrain;
                    const auto split=profile->splitToning;const auto aberration=profile->chromaticAberration;const auto debanding=profile->debanding;const auto lgg=profile->liftGammaGain;const auto lut=profile->lut;const auto dirt=profile->lensDirt;const auto distortion=profile->lensDistortion;
                    *profile=ColorSettings{};profile->ssao=ssao;profile->sharpen=sharpen;profile->sharpnessEnabled=sharpnessEnabled;profile->bloom=bloom;profile->fog=fog;profile->dof=dof;profile->clarity=clarity;profile->vibrance=vibrance;
                    profile->vignette=vignette;profile->filmGrain=grain;profile->splitToning=split;
                    profile->chromaticAberration=aberration;profile->debanding=debanding;profile->liftGammaGain=lgg;profile->lut=lut;profile->lensDirt=dirt;profile->lensDistortion=distortion;Changed();
                }
                api.endDisabled();
            }else if(page==20) {
                if(api.checkbox("SSAO enabled",&profile->ssao.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->ssao.enabled);
                constexpr const char* qualities[]={"Balanced (half resolution)","High","Ultra"};
                Text("Quality");
                if(api.combo("##Quality",&profile->ssao.quality,qualities,3,-1)) {
                    profile->ssao.quality=std::clamp(profile->ssao.quality,0,2);Changed();
                }
                FinishingParameters(*profile,[&](const char* effect,auto,const char* label,float& value,float low,float high,float neutral) {
                    if(std::string_view(effect)=="SSAO")Control(label,value,low,high,neutral);
                });
                if(Button("Reset SSAO")){profile->ssao=SsaoSettings{};Changed();}
                api.endDisabled();
                if(!view.ssao.empty())Text(view.ssao.c_str());
            }else if(page==19) {
                if(api.checkbox("Sharpness enabled",&profile->sharpnessEnabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->sharpnessEnabled);
                Control("Sharpness",profile->sharpen,0,1,0);
                if(Button("Reset Sharpness")){profile->sharpen=0;profile->sharpnessEnabled=true;Changed();}
                api.endDisabled();
            }else if(page==18) {
                if(api.checkbox("Lens Distortion enabled",&profile->lensDistortion.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->lensDistortion.enabled);
                Control("Amount",profile->lensDistortion.amount,-1,1,0.2f);
                if(Button("Reset Lens Distortion")){profile->lensDistortion=LensDistortionSettings{};Changed();}
                api.endDisabled();
            }else if(page==17) {
                if(api.checkbox("Lens Dirt enabled",&profile->lensDirt.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->lensDirt.enabled);
                Control("Strength",profile->lensDirt.strength,0,1,0.35f);
                Control("Light Threshold",profile->lensDirt.threshold,0.01f,8,0.25f);
                if(Button("Reset Lens Dirt")){profile->lensDirt=LensDirtSettings{};Changed();}
                api.endDisabled();
                if(profile->lensDirt.enabled && (!profile->bloom.enabled || profile->bloom.intensity<=0))Text("Requires Bloom.");
            }else if(page==16) {
                if(api.checkbox("3D LUT enabled",&profile->lut.enabled))Changed();
                api.beginDisabled(!settings.enabled || !profile->lut.enabled);
                static std::filesystem::path directory;
                static std::vector<std::string> files;
                const auto currentDirectory=graphics::LutDirectory();
                if(directory!=currentDirectory){directory=currentDirectory;files=ListLuts(directory);}
                std::vector<std::string> names={"None"};names.insert(names.end(),files.begin(),files.end());
                if(!profile->lut.file.empty() && std::find(names.begin()+1,names.end(),profile->lut.file)==names.end())names.push_back(profile->lut.file);
                std::vector<const char*> lutChoices;int selected=0;
                for(std::size_t i=0;i<names.size();++i){lutChoices.push_back(names[i].c_str());if(names[i]==profile->lut.file)selected=static_cast<int>(i);}
                Text("LUT File");
                if(api.combo("##LUT File",&selected,lutChoices.data(),static_cast<int>(lutChoices.size()),-1) && selected>=0 && selected<static_cast<int>(names.size())) {
                    profile->lut.file=selected?names[selected]:"";Changed();
                }
                Control("Strength",profile->lut.strength,0,1,1);
                if(Button("Reload LUTs")){files=ListLuts(directory);graphics::ReloadLuts();}
                if(Button("Reset 3D LUT")){profile->lut=LutSettings{};Changed();}
                api.endDisabled();
                if(!view.lut.empty())Text(view.lut.c_str());
            }else if(page>=10 && page<=15) {
                constexpr const char* keys[]={"Vignette","FilmGrain","SplitToning","ChromaticAberration","Debanding","LiftGammaGain"};
                constexpr const char* names[]={"Vignette","Film Grain","Split Toning","Chromatic Aberration","Debanding","Lift Gamma Gain"};
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
                constexpr const char* focusModes[]={"Manual","Auto"};
                int focusMode=profile->dof.autofocus?1:0;
                if(api.combo("Focus Mode",&focusMode,focusModes,2,-1) && focusMode>=0 && focusMode<2) {
                    profile->dof.autofocus=focusMode==1;Changed();
                }
                api.beginDisabled(profile->dof.autofocus);
                Control("Focus Distance (units)",profile->dof.focusDistance,1,50000,2000);
                api.endDisabled();
                api.beginDisabled(!profile->dof.autofocus);
                Control("Focus Speed",profile->dof.focusSpeed,0.1f,20,5);
                Control("Focus Area",profile->dof.focusRadius,0,0.2f,0.03f);
                api.endDisabled();
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
void __stdcall Debanding(){Render(14);}
void __stdcall LiftGammaGain(){Render(15);}
void __stdcall Lut(){Render(16);}
void __stdcall LensDirt(){Render(17);}
void __stdcall LensDistortion(){Render(18);}
void __stdcall Sharpness(){Render(19);}
void __stdcall Ssao(){Render(20);}
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
        "Community Graphics Engine/Color/Color","Community Graphics Engine/Color/Lift Gamma Gain",
        "Community Graphics Engine/Color/3D LUT","Community Graphics Engine/Color/Vibrance",
        "Community Graphics Engine/Color/Split Toning","Community Graphics Engine/Color/Vignette",
        "Community Graphics Engine/Color/Debanding",
        "Community Graphics Engine/Effects/Bloom","Community Graphics Engine/Effects/Lens Dirt",
        "Community Graphics Engine/Effects/SSAO (BÊTA)","Community Graphics Engine/Effects/Distance Fog","Community Graphics Engine/Effects/Depth of Field",
        "Community Graphics Engine/Effects/Motion Blur","Community Graphics Engine/Effects/Sharpness",
        "Community Graphics Engine/Effects/Clarity","Community Graphics Engine/Effects/Film Grain",
        "Community Graphics Engine/Effects/Chromatic Aberration","Community Graphics Engine/Effects/Lens Distortion"};
    const Page pages[]={General,Presets,Profiles,Color,LiftGammaGain,Lut,Vibrance,SplitToning,Vignette,Debanding,
        Bloom,LensDirt,Ssao,Fog,Dof,Motion,Sharpness,Clarity,FilmGrain,ChromaticAberration,LensDistortion};
    for(unsigned i=0;i<21;++i)api.addPage(paths[i],pages[i]);
    registered=true;
    status="CGE SKSE Menu Framework ready: 21 pages registered; legacy menu and hotkeys removed";
    return true;
}
bool IsBlocking() noexcept {return registered && api.blocking();}
}
