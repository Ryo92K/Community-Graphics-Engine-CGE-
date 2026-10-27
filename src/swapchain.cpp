#include "graphics_host.h"
#include <atomic>
#include <new>

namespace cge::graphics {
class SwapChain final : public IDXGISwapChain4 {
    std::atomic<ULONG> references_{1};
    ComPtr<IDXGISwapChain> base_;
    ComPtr<IDXGISwapChain1> v1_;
    ComPtr<IDXGISwapChain2> v2_;
    ComPtr<IDXGISwapChain3> v3_;
    ComPtr<IDXGISwapChain4> v4_;
    bool Supports(REFIID iid) const {
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||
            iid==__uuidof(IDXGIDeviceSubObject)||iid==__uuidof(IDXGISwapChain)) return true;
        return (iid==__uuidof(IDXGISwapChain1)&&v1_)||
            (iid==__uuidof(IDXGISwapChain2)&&v2_)||
            (iid==__uuidof(IDXGISwapChain3)&&v3_)||
            (iid==__uuidof(IDXGISwapChain4)&&v4_);
    }
public:
    explicit SwapChain(IDXGISwapChain* chain):base_(chain) {
        chain->QueryInterface(IID_PPV_ARGS(&v1_));
        chain->QueryInterface(IID_PPV_ARGS(&v2_));
        chain->QueryInterface(IID_PPV_ARGS(&v3_));
        chain->QueryInterface(IID_PPV_ARGS(&v4_));
    }
    ~SwapChain(){Detach(base_.Get());}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override {
        if(!object)return E_POINTER;
        *object=nullptr;
        if(!Supports(iid))return E_NOINTERFACE;
        *object=static_cast<IDXGISwapChain4*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++references_;}
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count=--references_;
        if(!count)delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID name,UINT size,const void* data) override{return base_->SetPrivateData(name,size,data);}
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID name,const IUnknown* object) override{return base_->SetPrivateDataInterface(name,object);}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID name,UINT* size,void* data) override{return base_->GetPrivateData(name,size,data);}
    HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** object) override{return base_->GetParent(iid,object);}
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid,void** device) override{return base_->GetDevice(iid,device);}
    HRESULT STDMETHODCALLTYPE Present(UINT interval,UINT flags) override {
        if(!(flags&DXGI_PRESENT_TEST)){Frame(base_.Get());}
        const auto result=base_->Present(interval,flags);return result;
    }
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT index,REFIID iid,void** buffer) override{return base_->GetBuffer(index,iid,buffer);}
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL state,IDXGIOutput* output) override{return base_->SetFullscreenState(state,output);}
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* state,IDXGIOutput** output) override{return base_->GetFullscreenState(state,output);}
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* desc) override{return base_->GetDesc(desc);}
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) override {
        BeforeResize(base_.Get());
        const auto result=base_->ResizeBuffers(count,width,height,format,flags);
        if(SUCCEEDED(result))UpdateDimensions(base_.Get());
        return result;
    }
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* mode) override{return base_->ResizeTarget(mode);}
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** output) override{return base_->GetContainingOutput(output);}
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* statistics) override{return base_->GetFrameStatistics(statistics);}
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* count) override{return base_->GetLastPresentCount(count);}
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* desc) override{return v1_?v1_->GetDesc1(desc):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc) override{return v1_?v1_->GetFullscreenDesc(desc):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND* hwnd) override{return v1_?v1_->GetHwnd(hwnd):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID iid,void** window) override{return v1_?v1_->GetCoreWindow(iid,window):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE Present1(UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters) override {
        if(!v1_)return E_NOINTERFACE;
        if(!(flags&DXGI_PRESENT_TEST)){Frame(base_.Get());}
        const auto result=v1_->Present1(interval,flags,parameters);return result;
    }
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override{return v1_?v1_->IsTemporaryMonoSupported():FALSE;}
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** output) override{return v1_?v1_->GetRestrictToOutput(output):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* color) override{return v1_?v1_->SetBackgroundColor(color):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* color) override{return v1_?v1_->GetBackgroundColor(color):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION rotation) override{return v1_?v1_->SetRotation(rotation):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* rotation) override{return v1_?v1_->GetRotation(rotation):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT width,UINT height) override{return v2_?v2_->SetSourceSize(width,height):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* width,UINT* height) override{return v2_?v2_->GetSourceSize(width,height):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT value) override{return v2_?v2_->SetMaximumFrameLatency(value):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* value) override{return v2_?v2_->GetMaximumFrameLatency(value):E_NOINTERFACE;}
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override{return v2_?v2_->GetFrameLatencyWaitableObject():nullptr;}
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* matrix) override{return v2_?v2_->SetMatrixTransform(matrix):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* matrix) override{return v2_?v2_->GetMatrixTransform(matrix):E_NOINTERFACE;}
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override{return v3_?v3_->GetCurrentBackBufferIndex():0;}
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE color,UINT* flags) override{return v3_?v3_->CheckColorSpaceSupport(color,flags):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE color) override {
        if(!v3_)return E_NOINTERFACE;
        const auto result=v3_->SetColorSpace1(color);
        if(SUCCEEDED(result))SetOutputColorSpace(base_.Get(),color);
        return result;
    }
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown*const* queues) override {
        if(!v3_)return E_NOINTERFACE;
        BeforeResize(base_.Get());
        const auto result=v3_->ResizeBuffers1(count,width,height,format,flags,masks,queues);
        if(SUCCEEDED(result))UpdateDimensions(base_.Get());
        return result;
    }
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE type,UINT size,void* data) override{return v4_?v4_->SetHDRMetaData(type,size,data):E_NOINTERFACE;}
};
IDXGISwapChain* Wrap(IDXGISwapChain* chain) noexcept {
    auto* wrapper=new(std::nothrow) SwapChain(chain);
    if(!wrapper)return chain;
    chain->Release();                                                           
    return wrapper;
}
}
