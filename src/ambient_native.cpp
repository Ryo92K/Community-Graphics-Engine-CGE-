#include "ambient_native.h"
#include "ambient_shader.h"
#include <windows.h>
#include <wrl/client.h>
#include <mutex>
#include <atomic>
namespace cge::ambient {
namespace {
using Microsoft::WRL::ComPtr;
using CreatePS=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
struct State {
    std::mutex mutex;
    ID3D11Device* device{};
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ShaderResourceView> visibility;
    ComPtr<ID3D11Buffer> enabled;
    void* volatile* createSlot{};
    CreatePS create{};
    std::atomic<ID3D11Device*> filter{};
    unsigned accepted{},rejected{},failures{},passes{},conflicts{};
};
State& Data(){static auto* state=new State;return *state;}
bool Swap(void* volatile* slot,void* expected,void* replacement) noexcept {
    DWORD protection{},ignored{};
    if(!VirtualProtect(const_cast<void**>(slot),sizeof(void*),PAGE_READWRITE,&protection))return false;
    const bool changed=InterlockedCompareExchangePointer(slot,replacement,expected)==expected;
    VirtualProtect(const_cast<void**>(slot),sizeof(void*),protection,&ignored);return changed;
}
HRESULT STDMETHODCALLTYPE Create(ID3D11Device* device,const void* bytes,SIZE_T length,ID3D11ClassLinkage* linkage,ID3D11PixelShader** shader) {
    auto& state=Data();
    if(shader && !linkage && state.filter.load()==device && bytes && length<=4*1024*1024) {
        try {
            auto modified=AddAmbientOcclusion({static_cast<const std::uint8_t*>(bytes),length});
            if(!modified.bytecode.empty()) {
                ComPtr<ID3D11PixelShader> replacement;
                const auto status=state.create(device,modified.bytecode.data(),modified.bytecode.size(),nullptr,&replacement);
                std::lock_guard lock(state.mutex);
                if(SUCCEEDED(status) && replacement) {++state.accepted;*shader=replacement.Detach();return status;}
                ++state.failures;
            }else {std::lock_guard lock(state.mutex);++state.rejected;}
        }catch(...){std::lock_guard lock(state.mutex);++state.failures;}
    }
    return state.create(device,bytes,length,linkage,shader);
}
bool SystemMethod(void* function) {
    HMODULE owner{};
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(function),&owner) && owner==GetModuleHandleW(L"d3d11.dll");
}
}
bool Attach(ID3D11Device* device,std::string& error) {
    if(!device){error="Native ambient device unavailable";return false;}
    auto& state=Data();std::lock_guard lock(state.mutex);
    if(state.device){error="Native ambient device already attached";return false;}
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    if(!context){error="Native ambient context unavailable";return false;}
    auto* deviceTable=*reinterpret_cast<void***>(device);
    auto* createSlot=reinterpret_cast<void* volatile*>(deviceTable+15);
    if(!SystemMethod(*createSlot)){error="Native ambient API hook conflict";return false;}
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    const float values[]={1,0,0,0};const D3D11_SUBRESOURCE_DATA init{values,0,0};
    if(FAILED(device->CreateBuffer(&desc,&init,&state.enabled))){error="Native ambient control buffer unavailable";return false;}
    state.create=reinterpret_cast<CreatePS>(*createSlot);state.device=device;state.context=context;state.filter=device;
    if(!Swap(createSlot,reinterpret_cast<void*>(state.create),reinterpret_cast<void*>(Create))) {
        state.filter=nullptr;state.device=nullptr;state.context.Reset();state.enabled.Reset();error="Native ambient shader hook unavailable";return false;
    }
    state.createSlot=createSlot;return true;
}
void Begin(ID3D11ShaderResourceView* visibility) noexcept {
    auto& state=Data();std::lock_guard lock(state.mutex);
    if(!state.device || !visibility)return;
    ComPtr<ID3D11Device> owner;visibility->GetDevice(&owner);
    if(owner.Get()!=state.device){++state.conflicts;return;}
    ComPtr<ID3D11ShaderResourceView> occupied;state.context->PSGetShaderResources(126,1,&occupied);
    ComPtr<ID3D11Buffer> occupiedBuffer;state.context->PSGetConstantBuffers(13,1,&occupiedBuffer);
    if((occupied && occupied.Get()!=state.visibility.Get()) || (occupiedBuffer && occupiedBuffer.Get()!=state.enabled.Get())){++state.conflicts;return;}
    state.visibility=visibility;state.context->PSSetShaderResources(126,1,&visibility);
    auto* buffer=state.enabled.Get();state.context->PSSetConstantBuffers(13,1,&buffer);++state.passes;
}
void End() noexcept {
    auto& state=Data();std::lock_guard lock(state.mutex);
    if(state.context && state.visibility) {
        ComPtr<ID3D11ShaderResourceView> currentView;state.context->PSGetShaderResources(126,1,&currentView);
        if(currentView.Get()==state.visibility.Get()) {ID3D11ShaderResourceView* empty{};state.context->PSSetShaderResources(126,1,&empty);}
    }
    if(state.context && state.enabled) {
        ComPtr<ID3D11Buffer> buffer;state.context->PSGetConstantBuffers(13,1,&buffer);
        if(buffer.Get()==state.enabled.Get()){ID3D11Buffer* empty{};state.context->PSSetConstantBuffers(13,1,&empty);}
    }
    state.visibility.Reset();
}
void Detach() noexcept {
    End();auto& state=Data();std::lock_guard lock(state.mutex);state.filter=nullptr;
    if(state.createSlot)Swap(state.createSlot,reinterpret_cast<void*>(Create),reinterpret_cast<void*>(state.create));
    state.createSlot=nullptr;state.device=nullptr;state.context.Reset();state.enabled.Reset();
    state.accepted=state.rejected=state.failures=state.passes=state.conflicts=0;
}
std::string Diagnostics() {
    auto& state=Data();std::lock_guard lock(state.mutex);
    return "native shaders="+std::to_string(state.accepted)+", bypassed="+std::to_string(state.rejected)+
        ", failures="+std::to_string(state.failures)+", opaque passes="+std::to_string(state.passes)+", slot conflicts="+std::to_string(state.conflicts);
}
}
