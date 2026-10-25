#pragma once
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <functional>
#include <string>
#include "settings.h"
#include "motion_blur.h"
#include "depth_inputs.h"
namespace cge::graphics {
using Microsoft::WRL::ComPtr;
using Reporter=std::function<void(const std::string&)>;
bool Attach(IDXGISwapChain*,ID3D11Device*,const std::filesystem::path&,Reporter);
void Detach(IDXGISwapChain*);
void SetEnvironmentProbe(Environment(*probe)() noexcept);
void SetGameHourProbe(float(*probe)() noexcept);
void SetMotionProbe(MotionInputs(*probe)() noexcept);
void SetDepthProbe(DepthInputs(*probe)() noexcept);
struct SettingsView {
    Settings settings;
    Environment environment{Environment::Unknown};
    float hour{-1};
    MotionState motion{MotionState::Off};
    std::string bloom,fog,dof;
};
bool ReadSettings(SettingsView&);
void ApplySettings(const Settings&,bool resetProfiles=false);
bool SaveSettings();
void Scene() noexcept;
void HDR(ID3D11ShaderResourceView*) noexcept;
void Frame(IDXGISwapChain*) noexcept;
void UpdateDimensions(IDXGISwapChain*);
IDXGISwapChain* Wrap(IDXGISwapChain*) noexcept;
}
