#include "editor.h"
#include "gui_scope.h"
#include <windowsx.h>
#include <cstring>
namespace cge {
namespace {constexpr wchar_t property[]=L"CGE.Editor.v1";}
void Editor::Toggle(bool value) {
    if(open_.exchange(value)==value)return;
    if(value && GetForegroundWindow()==window_) {
        clipSaved_=GetClipCursor(&oldClip_)!=FALSE;ClipCursor(nullptr);
        CURSORINFO cursor{};cursor.cbSize=sizeof(cursor);
        if(GetCursorInfo(&cursor) && !(cursor.flags&CURSOR_SHOWING))
            for(unsigned i=0;i<16;++i){++cursorCredits_;if(ShowCursor(TRUE)>=0)break;}
        SetCursor(LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)));
    }else {
        while(cursorCredits_>0){ShowCursor(FALSE);--cursorCredits_;}
        if(clipSaved_ && GetForegroundWindow()==window_)ClipCursor(&oldClip_);
        clipSaved_=false;resetInput_=true;
    }
}
LRESULT CALLBACK Editor::Procedure(HWND window,UINT type,WPARAM wparam,LPARAM lparam) {
    auto* editor=static_cast<Editor*>(GetPropW(window,property));
    if(!editor)return DefWindowProcW(window,type,wparam,lparam);
    const auto original=editor->original_;
    if(editor->active_) {
        const bool key=type==WM_KEYDOWN || type==WM_SYSKEYDOWN;
        const bool up=type==WM_KEYUP || type==WM_SYSKEYUP;
        const auto shortcut=wparam==VK_F10?EditorKeys::F10:
            wparam==VK_RETURN?EditorKeys::ShiftEnter:wparam==VK_ESCAPE?EditorKeys::Escape:
            wparam==VK_F6?EditorKeys::Save:wparam==VK_F7?EditorKeys::Defaults:EditorKeys::Count;
        if(up && shortcut!=EditorKeys::Count)editor->keys_.Release(shortcut);
        if(key && !(lparam&(1LL<<30)) && (wparam==VK_F10 ||
           (wparam==VK_RETURN && (GetKeyState(VK_SHIFT)&0x8000)))) {
            if(editor->keys_.Press(shortcut))editor->Toggle(!editor->IsOpen());
            return 0;
        }
        if(type==WM_KILLFOCUS)editor->Toggle(false);
        if(editor->IsOpen()) {
            if(key && (wparam==VK_F6 || wparam==VK_F7)) {
                std::lock_guard lock(editor->queueMutex_);
                if(editor->keys_.Press(shortcut) && editor->queue_.size()<512)editor->queue_.push_back({window,type,wparam,lparam});return 0;
            }
            if(key && wparam==VK_ESCAPE){editor->keys_.Press(EditorKeys::Escape);editor->Toggle(false);return 0;}
            if(type==WM_SETCURSOR){SetCursor(LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)));return TRUE;}
            const bool mouse=type>=WM_MOUSEFIRST && type<=WM_MOUSELAST;
            const bool keyboard=type>=WM_KEYFIRST && type<=WM_KEYLAST;
            if(mouse || keyboard) {
                std::lock_guard lock(editor->queueMutex_);
                if(editor->queue_.size()<512)editor->queue_.push_back({window,type,wparam,lparam});
                return 0;
            }
        }
    }
    const auto result=editor->unicode_?CallWindowProcW(original,window,type,wparam,lparam):CallWindowProcA(original,window,type,wparam,lparam);
    if(type==WM_NCDESTROY) {
        editor->Toggle(false);editor->active_=false;editor->window_=nullptr;editor->original_=nullptr;
        RemovePropW(window,property);
    }
    return result;
}
void Editor::Save(){saved_=settings_ && settings_->Save(settingsPath_);}
void Editor::Defaults(){if(settings_)*settings_=Settings{};saved_=true;}
bool Editor::Initialize(HWND window,Settings& settings,const std::filesystem::path& path,ID3D11Device* device) {
    Shutdown();settings_=&settings;settingsPath_=path;
    if(!CreateView(device))return false;
    if(!window)return true;
    if(window_!=window || !original_) {
        window_=window;
        // Preserve the game's window encoding. Installing a W procedure on an
        // ANSI window exposes a 0xFFFFxxxx conversion token to later A hooks;
        // that token cannot be invoked directly as a native procedure.
        unicode_=IsWindowUnicode(window)!=FALSE;SetPropW(window,property,this);
        original_=reinterpret_cast<WNDPROC>(unicode_?
            SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(Procedure)):
            SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(Procedure)));
        if(!original_){RemovePropW(window,property);window_=nullptr;return false;}
    }
    active_=true;return true;
}
void Editor::Shutdown() {
    Toggle(false);active_=false;keys_.Reset();
    if(window_ && reinterpret_cast<WNDPROC>(unicode_?GetWindowLongPtrW(window_,GWLP_WNDPROC):GetWindowLongPtrA(window_,GWLP_WNDPROC))==Procedure) {
        if(unicode_)SetWindowLongPtrW(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original_));
        else SetWindowLongPtrA(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original_));
        RemovePropW(window_,property);window_=nullptr;original_=nullptr;
    }
    DestroyView();settings_=nullptr;renderWidth_=renderHeight_=0;
    for(auto& button:mouseDown_)button=false;
    std::lock_guard lock(queueMutex_);queue_.clear();
}
void Editor::MovePointer(LONG x,LONG y) {
    RECT client{};
    if(window_ && GetClientRect(window_,&client) && client.right>0 && client.bottom>0 && renderWidth_ && renderHeight_) {
        x=MulDiv(x,static_cast<int>(renderWidth_),client.right);
        y=MulDiv(y,static_cast<int>(renderHeight_),client.bottom);
    }
    if(gui_)ImGui::GetIO().AddMousePosEvent(float(x),float(y));
}
void Editor::MouseButton(unsigned button,bool down) {
    if(button>=3 || mouseDown_[button]==down)return;
    mouseDown_[button]=down;
    if(gui_)ImGui::GetIO().AddMouseButtonEvent(int(button),down);
}
void Editor::Pump(unsigned width,unsigned height) {
    if(!gui_)return;
    GuiScope scope(gui_);
    if(width && height && (width!=renderWidth_ || height!=renderHeight_)) {
        renderWidth_=width;renderHeight_=height;
    }
    const auto down=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
    const EditorKeys::Snapshot snapshot={down(VK_F10),down(VK_RETURN)&&down(VK_SHIFT),
        down(VK_ESCAPE),down(VK_F6),down(VK_F7)};
    const auto actions=keys_.Poll(active_ && window_ && GetForegroundWindow()==window_,snapshot);
    if(actions&EditorKeys::LostFocus)Toggle(false);
    if(actions&((1u<<EditorKeys::F10)|(1u<<EditorKeys::ShiftEnter)))Toggle(!IsOpen());
    if(IsOpen()) {
        if(actions&(1u<<EditorKeys::Escape))Toggle(false);
        else {
            if(actions&(1u<<EditorKeys::Save))Save();
            if(actions&(1u<<EditorKeys::Defaults))Defaults();
        }
    }
    if(resetInput_.exchange(false)){
        for(unsigned i=0;i<3;++i)MouseButton(i,false);
    }
    std::vector<Message> messages;
    {std::lock_guard lock(queueMutex_);messages.swap(queue_);}
    if(IsOpen())for(const auto& message:messages) {
        if((message.type==WM_KEYDOWN || message.type==WM_SYSKEYDOWN) && message.wparam==VK_F6)Save();
        else if((message.type==WM_KEYDOWN || message.type==WM_SYSKEYDOWN) && message.wparam==VK_F7)Defaults();
        else if(message.type==WM_MOUSEMOVE)MovePointer(GET_X_LPARAM(message.lparam),GET_Y_LPARAM(message.lparam));
        else if(message.type==WM_LBUTTONDOWN || message.type==WM_LBUTTONDBLCLK || message.type==WM_LBUTTONUP ||
                message.type==WM_RBUTTONDOWN || message.type==WM_RBUTTONDBLCLK || message.type==WM_RBUTTONUP ||
                message.type==WM_MBUTTONDOWN || message.type==WM_MBUTTONDBLCLK || message.type==WM_MBUTTONUP) {
            MovePointer(GET_X_LPARAM(message.lparam),GET_Y_LPARAM(message.lparam));
            const unsigned button=message.type==WM_LBUTTONDOWN || message.type==WM_LBUTTONDBLCLK || message.type==WM_LBUTTONUP?0:
                message.type==WM_RBUTTONDOWN || message.type==WM_RBUTTONDBLCLK || message.type==WM_RBUTTONUP?1:2;
            const bool pressed=message.type!=WM_LBUTTONUP && message.type!=WM_RBUTTONUP && message.type!=WM_MBUTTONUP;
            MouseButton(button,pressed);
        }else if(message.type==WM_MOUSEWHEEL)ImGui::GetIO().AddMouseWheelEvent(0,float(GET_WHEEL_DELTA_WPARAM(message.wparam))/WHEEL_DELTA);
        else if(message.type==WM_CHAR)ImGui::GetIO().AddInputCharacterUTF16(static_cast<ImWchar16>(message.wparam));
    }
    if(IsOpen()) {
        auto& io=ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl,down(VK_CONTROL));io.AddKeyEvent(ImGuiMod_Shift,down(VK_SHIFT));
        const std::pair<int,ImGuiKey> keys[]={{VK_BACK,ImGuiKey_Backspace},{VK_DELETE,ImGuiKey_Delete},{VK_LEFT,ImGuiKey_LeftArrow},
            {VK_RIGHT,ImGuiKey_RightArrow},{VK_HOME,ImGuiKey_Home},{VK_END,ImGuiKey_End},{VK_RETURN,ImGuiKey_Enter},
            {VK_TAB,ImGuiKey_Tab},{'A',ImGuiKey_A},{'C',ImGuiKey_C},{'V',ImGuiKey_V},{'X',ImGuiKey_X},{'Z',ImGuiKey_Z}};
        for(auto key:keys)io.AddKeyEvent(key.second,down(key.first));
    }
    if(IsOpen() && window_ && GetForegroundWindow()==window_) {
        POINT point{};
        if(GetCursorPos(&point) && ScreenToClient(window_,&point)) {
            MovePointer(point.x,point.y);
            const int buttons[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON};
            for(unsigned i=0;i<3;++i)MouseButton(i,down(buttons[i]));
        }
    }
}
}
