#include "editor.h"
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include "gui_scope.h"
#include "profiles.h"
#include <imgui_impl_dx11.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
namespace cge {
namespace {
void Help(const char* text) {
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))ImGui::SetTooltip("%s",text);
}
}
bool Editor::CreateView(ID3D11Device* device) {
    if(!device)return false;
    auto* previous=ImGui::GetCurrentContext();
    gui_=ImGui::CreateContext();ImGui::SetCurrentContext(previous);
    GuiScope scope(gui_);
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    interFont_=false;fontData_.clear();
    std::ifstream font(settingsPath_.parent_path()/"CGE"/"Fonts"/"Inter.ttf",std::ios::binary|std::ios::ate);
    const auto length=font?font.tellg():std::streampos(-1);
    if(length>0 && length<=4*1024*1024) {
        fontData_.resize(static_cast<std::size_t>(length));font.seekg(0);
        if(font.read(reinterpret_cast<char*>(fontData_.data()),static_cast<std::streamsize>(fontData_.size()))) {
            ImFontConfig config;config.FontDataOwnedByAtlas=false;
            interFont_=io.Fonts->AddFontFromMemoryTTF(fontData_.data(),static_cast<int>(fontData_.size()),16,&config)!=nullptr;
        }
    }
    if(!interFont_){ImFontConfig fallback;fallback.SizePixels=16;io.Fonts->AddFontDefaultVector(&fallback);}
    ImGui::StyleColorsDark();
    auto& style=ImGui::GetStyle();
    style.FontSizeBase=16;
    style.WindowPadding={12,12};style.FramePadding={8,4};style.ItemSpacing={8,6};
    style.WindowRounding=6;style.FrameRounding=4;style.ChildRounding=4;
    style.Colors[ImGuiCol_TitleBgActive]={0.16f,0.18f,0.20f,1};
    style.Colors[ImGuiCol_TitleBg]={0.12f,0.13f,0.14f,1};
    style.Colors[ImGuiCol_WindowBg]={0.08f,0.09f,0.10f,0.98f};
    style.Colors[ImGuiCol_Header]={0.22f,0.24f,0.27f,1};
    style.Colors[ImGuiCol_Button]={0.22f,0.24f,0.27f,1};
    style.Colors[ImGuiCol_FrameBg]={0.12f,0.16f,0.17f,1};
    style.Colors[ImGuiCol_FrameBgHovered]={0.16f,0.23f,0.24f,1};
    style.Colors[ImGuiCol_FrameBgActive]={0.20f,0.28f,0.29f,1};
    style.Colors[ImGuiCol_CheckMark]={0.35f,0.78f,0.69f,1};
    style.Colors[ImGuiCol_ButtonHovered]={0.20f,0.35f,0.34f,1};
    style.Colors[ImGuiCol_ButtonActive]={0.18f,0.45f,0.40f,1};
    style.Colors[ImGuiCol_Header]={0.16f,0.32f,0.30f,1};
    style.Colors[ImGuiCol_HeaderHovered]={0.20f,0.40f,0.36f,1};
    style.Colors[ImGuiCol_SliderGrab]={0.35f,0.70f,0.61f,1};
    style.Colors[ImGuiCol_SliderGrabActive]={0.45f,0.85f,0.73f,1};
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    lastFrame_=std::chrono::steady_clock::now();
    return ImGui_ImplDX11_Init(device,context.Get());
}
void Editor::DestroyView() {
    if(!gui_)return;
    auto* previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(gui_);
    if(ImGui::GetIO().BackendRendererUserData)ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext(gui_);ImGui::SetCurrentContext(previous==gui_?nullptr:previous);gui_=nullptr;
    fontData_.clear();interFont_=false;
}
void Editor::SetEnvironment(Environment value,float hour) noexcept {
    gameHour_=hour;
    if(!IsOpen() && value!=Environment::Unknown && settings_ && settings_->separateProfiles) {
        if(value==Environment::Interior)editProfile_=1;
        else if(ValidGameHour(hour))editProfile_=NightWeight(hour,settings_->dayHour,settings_->nightHour)>=0.5f?3:2;
    }
    environment_=value;
}
void Editor::Draw() {
    if(!IsOpen() || !gui_ || !settings_)return;
    GuiScope scope(gui_);auto& io=ImGui::GetIO();
    auto now=std::chrono::steady_clock::now();
    io.DeltaTime=std::clamp(std::chrono::duration<float>(now-lastFrame_).count(),0.001f,0.1f);lastFrame_=now;
    io.DisplaySize={float(renderWidth_),float(renderHeight_)};
    ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
    ImGui::SetNextWindowPos({20,20},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({600,520},ImGuiCond_Always);
    bool visible=true;
    if(ImGui::Begin("Community Graphics Engine###CGE",&visible,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::SetCursorPos({16,40});
        if(ImGui::Button(settings_->enabled?"Effects: On###Effects":"Effects: Off###Effects",{112,28})) {
            settings_->enabled=!settings_->enabled;Changed();
        }
        Help("Enable or bypass all CGE effects.");
        ImGui::SetCursorPos({136,40});
        if(ImGui::Button(settings_->separateProfiles?"Automatic###Profiles":"Shared###Profiles",{136,28})) {
            settings_->SetSeparateProfiles(!settings_->separateProfiles);
            editProfile_=settings_->separateProfiles?(environment_==Environment::Interior?1:
                ValidGameHour(gameHour_) && NightWeight(gameHour_,settings_->dayHour,settings_->nightHour)>=0.5f?3:2):0;Changed();
        }
        Help("Shared uses one profile everywhere. Automatic follows the location and game time.");
        const bool clockValid=std::isfinite(gameHour_) && gameHour_>=0 && gameHour_<=24;
        const float weight=clockValid?NightWeight(gameHour_,settings_->dayHour,settings_->nightHour):0;
        const char* active=!settings_->separateProfiles?"Shared":environment_==Environment::Interior?"Interior":
            environment_==Environment::Exterior?(clockValid?(weight>=0.5f?"Exterior Night":"Exterior Day"):"Exterior"):"No cell";
        char context[80]{};
        if(clockValid) {
            const int minutes=static_cast<int>(gameHour_*60)%1440;
            sprintf_s(context,"%s  %02d:%02d",active,minutes/60,minutes%60);
        }else sprintf_s(context,"%s",active);
        ImGui::SetCursorPos({296,47});ImGui::TextUnformatted(context);
        const auto origin=ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddRectFilled({origin.x+12,origin.y+84},{origin.x+156,origin.y+448},
            IM_COL32(20,26,29,255),4);
        auto navigation=[&](const char* label,int index,float y) {
            ImGui::SetCursorPos({20,y});
            if(index>0 && index<4 && !settings_->separateProfiles)ImGui::BeginDisabled();
            if(ImGui::Selectable(label,editProfile_==index,0,{128,28})) {
                editProfile_=index;
                if(index==0){settings_->SetSeparateProfiles(false);Changed();}
            }
            if(index>0 && index<4 && !settings_->separateProfiles)ImGui::EndDisabled();
        };
        if(!settings_->separateProfiles && editProfile_!=4)editProfile_=0;
        else if(editProfile_==0)editProfile_=environment_==Environment::Interior?1:2;
        navigation("Shared",0,90);navigation("Interior",1,126);
        navigation("Exterior Day",2,162);navigation("Exterior Night",3,198);
        navigation("Motion Blur",4,234);
        auto* color=static_cast<ColorSettings*>(settings_);
        if(settings_->separateProfiles)color=editProfile_==1?&settings_->interior:editProfile_==3?&settings_->exteriorNight:&settings_->exterior;
        ImGui::PushID(editProfile_);
        unsigned row{};
        auto control=[&](const char* label,float& value,float minimum,float maximum,float fallback,const char* help) {
            const float y=90+52.0f*static_cast<float>(row++);
            ImGui::PushID(label);
            ImGui::SetCursorPos({172,y+26});ImGui::TextUnformatted(label);Help(help);
            ImGui::SetCursorPos({408,y-4});ImGui::SetNextItemWidth(84);
            float precise=value;
            if(ImGui::InputFloat("##Value",&precise,0,0,"%.3f",ImGuiInputTextFlags_EnterReturnsTrue) && std::isfinite(precise)) {
                value=std::clamp(precise,minimum,maximum);Changed();
            }
            Help("Type a value and press Enter.");
            ImGui::SetCursorPos({500,y-4});
            if(ImGui::Button("Reset",{84,25})){value=fallback;Changed();}
            ImGui::SetCursorPos({172,y-4});ImGui::SetNextItemWidth(230);
            if(ImGui::SliderFloat("##Slider",&value,minimum,maximum,"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))Changed();
            ImGui::PopID();
        };
        if(editProfile_==4) {
            ImGui::SetCursorPos({172,90});ImGui::TextUnformatted("Motion Blur");
            ImGui::SetCursorPos({414,86});
            if(ImGui::Button(settings_->motion.enabled?"On###MotionEnabled":"Off###MotionEnabled",{170,28})) {
                settings_->motion.enabled=!settings_->motion.enabled;Changed();
            }
            Help("Native velocity blur. Requires native TAA and matching motion/depth buffers.");
            row=1;
            control("Intensity",settings_->motion.strength,0,2,0.5f,"Exposure strength, normalized for frame rate. 0.5 corresponds to 1/120 second.");
            control("Blur Radius (px)",settings_->motion.radius,1,64,24,"Maximum blur radius in screen pixels.");
            ImGui::SetCursorPos({172,266});ImGui::TextUnformatted("Quality");
            const int counts[]={8,12,16};
            for(int i=0;i<3;++i) {
                ImGui::SetCursorPos({300+94.0f*float(i),258});
                char text[16]{};sprintf_s(text,"%d",counts[i]);
                const bool selected=settings_->motion.samples==counts[i];
                if(selected)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                const bool pressed=ImGui::Button(text,{88,28});
                if(selected)ImGui::PopStyleColor();
                if(pressed){settings_->motion.samples=counts[i];Changed();}
                Help(i==0?"8 samples: lower cost.":i==1?"12 samples: balanced quality.":"16 samples: smoother blur.");
            }
            ImGui::SetCursorPos({172,322});ImGui::TextDisabled("%s",!settings_->enabled?"Enable effects to preview":
                motionState_==MotionState::Paused?"Close CGE to preview":MotionLabel(motionState_));
            Help("Motion blur pauses in CGE, paused menus, loading and unsupported render setups.");
            ImGui::SetCursorPos({172,370});
            if(ImGui::Button("Natural",{120,28})) {
                settings_->motion={true,0.5f,24,12};Changed();
            }
            Help("Enable motion blur with strength 0.5, radius 24 and 12 samples.");
            ImGui::SameLine();
            if(ImGui::Button("Strong",{120,28})) {
                settings_->motion={true,1.5f,48,16};Changed();
            }
            Help("Enable a stronger comparison: strength 1.5, radius 48 and 16 samples. Save to keep it.");
        }else {
            control("Exposure",color->exposure,-3,3,0,"Brightness in stops.");
            control("Contrast",color->contrast,0,2,1,"Contrast around mid-gray.");
            control("Saturation",color->saturation,0,2,1,"Color intensity.");
            control("Sharpness",color->sharpen,0,1,0,"Edge sharpening.");
            control("Gamma",color->gamma,0.5f,2.5f,1,"Midtone brightness.");
            control("Color Temperature",color->temperature,-100,100,0,"Negative is cooler; positive is warmer.");
        }
        ImGui::PopID();
        if(settings_->separateProfiles) {
            auto number=[&](const char* label,float& value,float y,float minimum,float maximum,const char* help) {
                ImGui::SetCursorPos({20,y});ImGui::TextUnformatted(label);Help(help);
                ImGui::SetCursorPos({20,y+20});ImGui::SetNextItemWidth(128);
                float precise=value;
                ImGui::PushID(label);
                if(ImGui::InputFloat("##Number",&precise,0,0,"%.2f",ImGuiInputTextFlags_EnterReturnsTrue) && std::isfinite(precise)) {
                    value=std::clamp(precise,minimum,maximum);Changed();
                }
                ImGui::PopID();
            };
            number("Transition (sec)",settings_->transitionSeconds,278,0,5,"Real-time blend duration for doors, waiting and time jumps.");
            number("Dawn (hour)",settings_->dayHour,338,0,11,"Game hour at the middle of dawn. 6.5 means 06:30.");
            number("Dusk (hour)",settings_->nightHour,398,13,23,"Game hour at the middle of dusk. Dawn and dusk blend over one game hour.");
        }
        ImGui::SetCursorPos({172,426});ImGui::TextUnformatted("Compare Key");
        ImGui::SetCursorPos({300,420});
        char key[48]{};sprintf_s(key,"F%d###CompareKey",settings_->toggleKey-0x70+1);
        if(ImGui::Button(capturingBinding_?"Press F1-F12###CompareKey":key,{132,25})) {
            if(capturingBinding_)capturingBinding_=false;else BeginBindingCapture();
        }
        Help("F6, F7 and F10 are reserved. Escape cancels.");
        ImGui::SetCursorPos({16,454});ImGui::Separator();
        ImGui::SetCursorPos({16,466});
        const char* save="Save###Save";
        if(saveFeedback_==SaveFeedback::Saved && now<savedUntil_)save="Saved###Save";
        else if(saveFeedback_==SaveFeedback::Failed)save="Save Failed###Save";
        if(ImGui::Button(save,{104,28}))Save();
        if(saveFeedback_==SaveFeedback::Failed)Help("Could not write CGE.ini. Check its location and permissions.");
        ImGui::SameLine();if(ImGui::Button("Reset All",{104,28}))Defaults();
        Help("Restore neutral settings, Shared mode and default timing.");
        ImGui::SetCursorPos({480,466});if(ImGui::Button("Close",{104,28}))visible=false;
    }
    ImGui::End();if(!visible)Toggle(false);
    ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
}
