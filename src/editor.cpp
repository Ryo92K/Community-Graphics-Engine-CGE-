#include "editor.h"
#include "gui_scope.h"
#include <windowsx.h>
#include <cstring>
namespace cge {
namespace {constexpr wchar_t property[]=L"CGE.Editor.v1";constexpr UINT compareMessage=WM_APP+49;}
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
        clipSaved_=false;resetInput_=true;capturingBinding_=false;
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
            wparam==VK_F6?EditorKeys::Save:wparam==VK_F7?EditorKeys::Defaults:
            wparam==static_cast<WPARAM>(editor->compareKey_.load())?EditorKeys::Compare:EditorKeys::Count;
        if(up && shortcut!=EditorKeys::Count)editor->keys_.Release(shortcut);
        if(key && editor->IsOpen() && editor->capturingBinding_) {
            std::lock_guard lock(editor->queueMutex_);
            if(editor->queue_.size()<512)editor->queue_.push_back({window,type,wparam,lparam});
            return 0;
        }
        if((key || up) && shortcut==EditorKeys::Compare) {
            if(key && !(lparam&(1LL<<30)) && editor->keys_.Press(EditorKeys::Compare)) {
                std::lock_guard lock(editor->queueMutex_);
                if(editor->queue_.size()<512)editor->queue_.push_back({window,compareMessage,0,0});
            }
            return 0;
        }
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
                if(editor->queue_.size()<512) {
                    Message message{window,type,wparam,lparam};
                    if(key) {
                        message.layout=GetKeyboardLayout(0);
                        message.shift=(GetKeyState(VK_SHIFT)&0x8000)?0x80:0;
                        message.control=(GetKeyState(VK_CONTROL)&0x8000)?0x80:0;
                        message.alt=(GetKeyState(VK_MENU)&0x8000)?0x80:0;
                        message.caps=static_cast<BYTE>(GetKeyState(VK_CAPITAL)&1);
                    }
                    editor->queue_.push_back(message);
                }
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
void Editor::Changed(){saveFeedback_=SaveFeedback::None;}
void Editor::Save() {
    const bool success=settings_ && settings_->Save(settingsPath_);
    saveFeedback_=success?SaveFeedback::Saved:SaveFeedback::Failed;
    savedUntil_=std::chrono::steady_clock::now()+std::chrono::seconds(2);
}
void Editor::Defaults() {
    if(settings_){*settings_=Settings{};compareKey_=settings_->toggleKey;}
    capturingBinding_=false;Changed();
}
void Editor::BeginBindingCapture() {
    for(int i=0;i<12;++i)bindingHeld_[i]=(GetAsyncKeyState(VK_F1+i)&0x8000)!=0;
    capturingBinding_=true;
}
void Editor::ApplyBinding(int key) {
    if(!settings_ || !Settings::ValidToggleKey(key))return;
    settings_->toggleKey=key;compareKey_=key;capturingBinding_=false;
    keys_.Press(EditorKeys::Compare);Changed();
}
bool Editor::Initialize(HWND window,Settings& settings,const std::filesystem::path& path,ID3D11Device* device) {
    Shutdown();settings_=&settings;settingsPath_=path;compareKey_=settings.toggleKey;saveFeedback_=SaveFeedback::None;
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
    Toggle(false);active_=false;keys_.Reset();capturingBinding_=false;
    if(window_ && reinterpret_cast<WNDPROC>(unicode_?GetWindowLongPtrW(window_,GWLP_WNDPROC):GetWindowLongPtrA(window_,GWLP_WNDPROC))==Procedure) {
        if(unicode_)SetWindowLongPtrW(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original_));
        else SetWindowLongPtrA(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original_));
        RemovePropW(window_,property);window_=nullptr;original_=nullptr;
    }
    DestroyView();settings_=nullptr;renderWidth_=renderHeight_=0;
    for(auto& button:mouseDown_)button=false;
    for(auto& held:textHeld_)held=false;
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
void Editor::TranslateText(unsigned key,const BYTE* state,HKL layout) {
    if(state[VK_CONTROL]&0x80 || state[VK_MENU]&0x80)return;
    wchar_t text[8]{};int count{};
    if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9){text[0]=wchar_t(L'0'+key-VK_NUMPAD0);count=1;}
    else if(key==VK_DECIMAL){text[0]=L'.';count=1;}
    else if(key==VK_SUBTRACT){text[0]=L'-';count=1;}
    else if(key==VK_ADD){text[0]=L'+';count=1;}
    else {
        // Flag 4 leaves Windows' dead-key state untouched. Use the game's
        // keyboard layout rather than assuming US number-row punctuation.
        count=ToUnicodeEx(key,MapVirtualKeyExW(key,MAPVK_VK_TO_VSC,layout),state,text,8,4,layout);
    }
    for(int i=0;i<count && i<8;++i) {
        if(text[i]==L',')text[i]=L'.';
        if(text[i]>=32)ImGui::GetIO().AddInputCharacterUTF16(static_cast<ImWchar16>(text[i]));
    }
}
void Editor::Pump(unsigned width,unsigned height) {
    if(!gui_)return;
    GuiScope scope(gui_);
    if(width && height && (width!=renderWidth_ || height!=renderHeight_)) {
        renderWidth_=width;renderHeight_=height;
    }
    const auto down=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
    const EditorKeys::Snapshot snapshot={down(VK_F10),down(VK_RETURN)&&down(VK_SHIFT),
        down(VK_ESCAPE),down(VK_F6),down(VK_F7),down(compareKey_.load())};
    const bool focused=active_ && window_ && GetForegroundWindow()==window_;
    const bool capture=capturingBinding_.load();
    const auto actions=keys_.Poll(focused,snapshot);
    if(actions&EditorKeys::LostFocus)Toggle(false);
    if(!capture && (actions&((1u<<EditorKeys::F10)|(1u<<EditorKeys::ShiftEnter))))Toggle(!IsOpen());
    if(!capture && focused && (actions&(1u<<EditorKeys::Compare)) && settings_){settings_->enabled=!settings_->enabled;Changed();}
    if(IsOpen() && !capture) {
        if(actions&(1u<<EditorKeys::Escape))Toggle(false);
        else {
            if(actions&(1u<<EditorKeys::Save))Save();
            if(actions&(1u<<EditorKeys::Defaults))Defaults();
        }
    }
    if(resetInput_.exchange(false)){
        for(unsigned i=0;i<3;++i)MouseButton(i,false);
    }
    if(IsOpen()) {
        auto& io=ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl,down(VK_CONTROL));
        io.AddKeyEvent(ImGuiMod_Shift,down(VK_SHIFT));
    }
    bool textMessage[256]{};
    const auto now=std::chrono::steady_clock::now();
    const bool textInput=focused && IsOpen() && !capture && ImGui::GetIO().WantTextInput;
    std::vector<Message> messages;
    {std::lock_guard lock(queueMutex_);messages.swap(queue_);}
    for(const auto& message:messages) {
        if(message.type==compareMessage) {
            if(focused && !capture && settings_){settings_->enabled=!settings_->enabled;Changed();}
            continue;
        }
        if(!IsOpen())continue;
        if(capture && (message.type==WM_KEYDOWN || message.type==WM_SYSKEYDOWN)) {
            if(message.wparam==VK_ESCAPE)capturingBinding_=false;
            else if(capturingBinding_ && Settings::ValidToggleKey(static_cast<int>(message.wparam)))ApplyBinding(static_cast<int>(message.wparam));
            continue;
        }
        if(textInput && message.wparam<256 && (message.type==WM_KEYDOWN || message.type==WM_SYSKEYDOWN)) {
            const auto key=static_cast<unsigned>(message.wparam);
            BYTE state[256]{};state[VK_SHIFT]=message.shift;state[VK_CONTROL]=message.control;
            state[VK_MENU]=message.alt;state[VK_CAPITAL]=message.caps;state[key]=0x80;
            TranslateText(key,state,message.layout?message.layout:GetKeyboardLayout(GetWindowThreadProcessId(window_,nullptr)));
            textMessage[key]=true;
            textRepeat_[key]=now+std::chrono::milliseconds((message.lparam&(1LL<<30))?40:500);
        }
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
        else if(message.type==WM_CHAR && !(message.lparam&0x00ff0000) && textInput)
            ImGui::GetIO().AddInputCharacterUTF16(static_cast<ImWchar16>(message.wparam==','?'.':message.wparam));
    }
    // Skyrim may poll DirectInput without translating WM_KEYDOWN to WM_CHAR.
    // Merge physical edges with queued key events and avoid duplicate characters.
    BYTE keyboard[256]{};
    for(int i=0;i<256;++i)if(down(i))keyboard[i]=0x80;
    keyboard[VK_CAPITAL]=static_cast<BYTE>(GetKeyState(VK_CAPITAL)&1);
    const auto layout=GetKeyboardLayout(GetWindowThreadProcessId(window_,nullptr));
    for(unsigned i=VK_SPACE;i<256;++i) {
        const bool pressed=(keyboard[i]&0x80)!=0;
        if(textInput && pressed && !textMessage[i] && (!textHeld_[i] || now>=textRepeat_[i])) {
            TranslateText(i,keyboard,layout);
            textRepeat_[i]=now+std::chrono::milliseconds(textHeld_[i]?40:500);
        }
        textHeld_[i]=pressed;
    }
    if(capture && focused && IsOpen()) {
        if(actions&(1u<<EditorKeys::Escape))capturingBinding_=false;
        for(int i=0;i<12;++i) {
            const bool pressed=down(VK_F1+i);
            if(capturingBinding_ && pressed && !bindingHeld_[i] && Settings::ValidToggleKey(VK_F1+i))ApplyBinding(VK_F1+i);
            bindingHeld_[i]=pressed;
        }
    }
    if(IsOpen()) {
        auto& io=ImGui::GetIO();
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
