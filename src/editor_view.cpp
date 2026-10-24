#include "editor.h"
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include "gui_scope.h"
#include <imgui_impl_dx11.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace cge {
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
    ImGui::SetNextWindowSize({360,300},ImGuiCond_Always);
    bool visible=true;
    if(ImGui::Begin("CGE",&visible,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::Checkbox("Enable effects",&settings_->enabled);
        ImGui::SetNextItemWidth(200);ImGui::SliderFloat("Exposure",&settings_->exposure,-3,3,"%.2f");
        ImGui::SetNextItemWidth(200);ImGui::SliderFloat("Contrast",&settings_->contrast,0,2,"%.2f");
        ImGui::SetNextItemWidth(200);ImGui::SliderFloat("Saturation",&settings_->saturation,0,2,"%.2f");
        ImGui::SetNextItemWidth(200);ImGui::SliderFloat("Sharpen",&settings_->sharpen,0,1,"%.2f");
        ImGui::SetCursorPos({12,260});
        if(ImGui::Button(saved_?"Save":"Save failed",{100,28}))Save();
        ImGui::SameLine();if(ImGui::Button("Reset",{100,28}))Defaults();
        ImGui::SameLine();if(ImGui::Button("Close",{100,28}))visible=false;
    }
    ImGui::End();if(!visible)Toggle(false);
    ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
}
