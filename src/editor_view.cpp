#include "editor.h"
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include "gui_scope.h"
#include <imgui_impl_dx11.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
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
    io.Fonts->AddFontDefaultVector();
    ImGui::StyleColorsDark();
    auto& style=ImGui::GetStyle();
    style.WindowPadding={12,12};style.FramePadding={8,6};style.ItemSpacing={8,6};
    style.WindowRounding=3;style.FrameRounding=2;style.ChildRounding=0;
    style.Colors[ImGuiCol_TitleBgActive]={0.16f,0.18f,0.20f,1};
    style.Colors[ImGuiCol_TitleBg]={0.12f,0.13f,0.14f,1};
    style.Colors[ImGuiCol_WindowBg]={0.08f,0.09f,0.10f,0.98f};
    style.Colors[ImGuiCol_Header]={0.22f,0.24f,0.27f,1};
    style.Colors[ImGuiCol_Button]={0.22f,0.24f,0.27f,1};
    style.Colors[ImGuiCol_CheckMark]={0.65f,0.77f,0.88f,1};
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    lastFrame_=std::chrono::steady_clock::now();
    return ImGui_ImplDX11_Init(device,context.Get());
}
void Editor::DestroyView() {
    if(!gui_)return;
    auto* previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(gui_);
    if(ImGui::GetIO().BackendRendererUserData)ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext(gui_);ImGui::SetCurrentContext(previous==gui_?nullptr:previous);gui_=nullptr;
}
void Editor::Draw() {
    if(!IsOpen() || !gui_ || !settings_)return;
    GuiScope scope(gui_);auto& io=ImGui::GetIO();
    auto now=std::chrono::steady_clock::now();
    io.DeltaTime=std::clamp(std::chrono::duration<float>(now-lastFrame_).count(),0.001f,0.1f);lastFrame_=now;
    io.DisplaySize={float(renderWidth_),float(renderHeight_)};
    ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
    ImGui::SetNextWindowPos({20,20},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({500,settings_->separateProfiles?450.0f:380.0f},ImGuiCond_Always);
    bool visible=true;
    if(ImGui::Begin("CGE",&visible,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::SetCursorPos({12,42});
        if(ImGui::Button(settings_->enabled?"Effects: On###Effects":"Effects: Off###Effects",{236,25})) {
            settings_->enabled=!settings_->enabled;Changed();
        }
        Help("Toggle all CGE effects without changing their values.");
        ImGui::SameLine();
        if(ImGui::Button(settings_->separateProfiles?"Profiles: Separate###Profiles":"Profiles: Shared###Profiles",{232,25})) {
            settings_->SetSeparateProfiles(!settings_->separateProfiles);Changed();
        }
        Help("Use separate settings for interiors and exteriors, or one shared profile.");
        const float offset=settings_->separateProfiles?34.0f:0.0f;
        auto* color=static_cast<ColorSettings*>(settings_);
        if(settings_->separateProfiles) {
            ImGui::SetCursorPos({12,78});
            auto profileButton=[&](const char* label,int index,bool active) {
                char text[64]{};sprintf_s(text,"%s%s###Profile%d",label,active?" (active)":"",index);
                if(editProfile_==index)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                const bool clicked=ImGui::Button(text,{234,25});
                if(editProfile_==index)ImGui::PopStyleColor();
                if(clicked)editProfile_=index;
            };
            profileButton("Exterior",0,environment_==Environment::Exterior);ImGui::SameLine();
            profileButton("Interior",1,environment_==Environment::Interior);
            color=editProfile_==1?&settings_->interior:&settings_->exterior;
        }
        ImGui::SetCursorPos({12,78+offset});
        // Give each profile independent text-entry state while switching tabs.
        ImGui::PushID(settings_->separateProfiles?editProfile_+1:0);
        auto control=[&](const char* label,float& value,float minimum,float maximum,float fallback,const char* help) {
            ImGui::PushID(label);
            ImGui::TextUnformatted(label);Help(help);
            ImGui::SameLine(122);ImGui::SetNextItemWidth(160);
            if(ImGui::SliderFloat("##Slider",&value,minimum,maximum,"%.2f",ImGuiSliderFlags_AlwaysClamp))Changed();
            ImGui::SameLine();ImGui::SetNextItemWidth(82);
            float precise=value;
            if(ImGui::InputFloat("##Value",&precise,0,0,"%.3f",ImGuiInputTextFlags_EnterReturnsTrue) && std::isfinite(precise)) {
                value=std::clamp(precise,minimum,maximum);Changed();
            }
            Help("Type a value and press Enter.");
            ImGui::SameLine();
            if(ImGui::Button("Reset",{64,25})){value=fallback;Changed();}
            Help("Restore this setting to its neutral value.");
            ImGui::PopID();
        };
        control("Exposure",color->exposure,-3,3,0,"Brightness in stops. Zero is neutral.");
        control("Contrast",color->contrast,0,2,1,"Contrast around mid-gray. One is neutral.");
        control("Saturation",color->saturation,0,2,1,"Color intensity. One is neutral.");
        control("Sharpen",color->sharpen,0,1,0,"Edge sharpening. Zero is off.");
        control("Gamma",color->gamma,0.5f,2.5f,1,"Midtone brightness. One is neutral.");
        control("Temperature",color->temperature,-100,100,0,"Negative is cooler; positive is warmer. Zero is neutral.");
        ImGui::PopID();
        ImGui::SetCursorPos({12,278+offset});ImGui::TextUnformatted("Compare key");
        ImGui::SameLine(122);
        char key[48]{};sprintf_s(key,"F%d###CompareKey",settings_->toggleKey-0x70+1);
        if(ImGui::Button(capturingBinding_?"Press F1-F12###CompareKey":key,{160,25})) {
            if(capturingBinding_)capturingBinding_=false;else BeginBindingCapture();
        }
        Help("Choose an unused function key. F6, F7 and F10 are reserved. Escape cancels.");
        if(settings_->separateProfiles) {
            ImGui::SetCursorPos({12,346});ImGui::SetNextItemWidth(300);
            if(ImGui::SliderFloat("Transition (s)",&settings_->transitionSeconds,0,5,"%.1f",ImGuiSliderFlags_AlwaysClamp))Changed();
            Help("Blend between profiles when entering or leaving an interior.");
        }
        ImGui::SetCursorPos({12,settings_->separateProfiles?396.0f:326.0f});
        const char* save="Save###Save";
        if(saveFeedback_==SaveFeedback::Saved && now<savedUntil_)save="Saved###Save";
        else if(saveFeedback_==SaveFeedback::Failed)save="Save failed###Save";
        if(ImGui::Button(save,{100,28}))Save();
        if(saveFeedback_==SaveFeedback::Failed)Help("Could not write CGE.ini. Check its location and permissions.");
        else Help("Save settings to CGE.ini.");
        ImGui::SameLine();if(ImGui::Button("Reset all",{100,28}))Defaults();
        Help("Restore all settings and the compare key to their defaults.");
        ImGui::SameLine(388);if(ImGui::Button("Close",{100,28}))visible=false;
    }
    ImGui::End();if(!visible)Toggle(false);
    ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
}
