#pragma once
#include <imgui.h>
namespace cge {
struct GuiScope {
    ImGuiContext* previous;
    explicit GuiScope(ImGuiContext* context):previous(ImGui::GetCurrentContext()){ImGui::SetCurrentContext(context);}
    ~GuiScope(){ImGui::SetCurrentContext(previous);}
};
}
