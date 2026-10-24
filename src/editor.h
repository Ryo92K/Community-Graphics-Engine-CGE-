#pragma once
#include "settings.h"
#include "editor_keys.h"
#include <d3d11.h>
#include <imgui.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
namespace cge {
class Editor {
    struct Message {HWND window;UINT type;WPARAM wparam;LPARAM lparam;};
    std::atomic<bool> open_{false},active_{false};
    std::atomic<bool> resetInput_{false};
    HWND window_{};
    WNDPROC original_{};
    bool unicode_{};
    EditorKeys keys_;
    unsigned renderWidth_{},renderHeight_{};
    bool mouseDown_[3]{};
    void MovePointer(LONG x,LONG y);
    void MouseButton(unsigned button,bool down);
    int cursorCredits_{};
    RECT oldClip_{};
    bool clipSaved_{};
    std::mutex queueMutex_;
    std::vector<Message> queue_;

    ImGuiContext* gui_{};
    Settings* settings_{};
    std::filesystem::path settingsPath_;
    bool saved_{true};
    std::chrono::steady_clock::time_point lastFrame_{};
    bool CreateView(ID3D11Device*);
    void DestroyView();
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
    void Toggle(bool);
    void Save();
    void Defaults();
public:
    bool Initialize(HWND,Settings&,const std::filesystem::path&,ID3D11Device*);
    void Shutdown();
    void Pump(unsigned width,unsigned height);
    void Draw();
    bool IsOpen() const noexcept {return open_.load();}
};
}
