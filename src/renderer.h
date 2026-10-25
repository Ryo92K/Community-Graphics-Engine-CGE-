#pragma once
#include "render_target_cache.h"
#include "settings.h"
#include "lut.h"
#include "motion_blur.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <string>
#include <chrono>
#include <optional>
#include <cstdint>
namespace cge {
using Microsoft::WRL::ComPtr;
class Renderer {
    RenderTargetCache output_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext1> context_;
    ComPtr<ID3DDeviceContextState> isolated_;
    ComPtr<ID3D11VertexShader> vertex_;
    ComPtr<ID3D11PixelShader> pixel_,motionPixel_,distortionPixel_;
    ComPtr<ID3D11Texture2D> graded_;
    ComPtr<ID3D11RenderTargetView> gradedTarget_;
    ComPtr<ID3D11ShaderResourceView> gradedView_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11Texture2D> source_;
    ComPtr<ID3D11ShaderResourceView> sourceView_;
    ComPtr<ID3D11RenderTargetView> sourceTarget_;
    std::filesystem::path lutDirectory_;
    std::string lutFile_,lutStatus_,lutFailure_,lutActiveStatus_;
    CubeLut lutData_;
    ComPtr<ID3D11ShaderResourceView> lutView_;
    bool lutLoaded_{};
    bool PrepareLut(const LutSettings&);
    unsigned width_{},height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    ComPtr<ID3D11Texture2D> motionReadback_;
    ComPtr<ID3D11Query> motionReadbackDone_;
    bool motionReadPending_{},motionDiagnosticActive_{};
    MotionSettings motionDiagnosticSettings_;
    unsigned motionDiagnosticBudget_{};
    std::chrono::steady_clock::time_point nextMotionSample_{};
    unsigned sampledWidth_{},sampledHeight_{};
    DXGI_FORMAT sampledFormat_{DXGI_FORMAT_UNKNOWN};
    float sampledExposure_{},sampledRadius_{};
    std::string motionDiagnostic_;
    std::chrono::steady_clock::time_point grainStart_;
    void PollMotionReadback();
    void CaptureMotionReadback(const MotionPass&,float exposure);
public:
    void SetLutDirectory(const std::filesystem::path& p){lutDirectory_=p;ReloadLut();}
    void ReloadLut(){lutFile_.clear();lutStatus_.clear();lutFailure_.clear();lutActiveStatus_.clear();lutLoaded_=false;lutView_.Reset();lutData_={};}
    const std::string& LutStatus() const noexcept{return lutStatus_;}
    void ReleaseOutput() noexcept {output_.Reset();}
    bool Initialize(ID3D11Device*,std::string& error);
    std::string TakeMotionDiagnostics();
    std::string Draw(ID3D11Texture2D*,const ColorSettings&,bool enabled,const MotionPass& motion={},MotionState* state=nullptr,std::optional<std::uint32_t> grainFrame={});
};
}
