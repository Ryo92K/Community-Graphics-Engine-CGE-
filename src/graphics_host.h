#pragma once
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <functional>
#include <string>
#include "settings.h"
#include "weather_profiles.h"
#include "motion_blur.h"
#include "depth_inputs.h"
namespace cge::graphics {
using Microsoft::WRL::ComPtr;
using Reporter=std::function<void(const std::string&)>;
bool Attach(IDXGISwapChain*,ID3D11Device*,const std::filesystem::path&,Reporter);
void Detach(IDXGISwapChain*);
void SetEnvironmentProbe(Environment(*probe)() noexcept);
void SetGameHourProbe(float(*probe)() noexcept);
void SetWeatherProbe(WeatherObservation(*probe)() noexcept);
void SetMotionProbe(MotionInputs(*probe)() noexcept);
void SetDepthProbe(DepthInputs(*probe)() noexcept);
struct SettingsView {
    Settings settings;
    Environment environment{Environment::Unknown};
    float hour{-1};
    MotionState motion{MotionState::Off};
    std::string bloom,fog,dof,lut,ssao;
    WeatherObservation weather;
};
bool ReadSettings(SettingsView&);
float OverlayFps() noexcept;
void ApplySettings(const Settings&,bool resetProfiles=false);
bool SaveSettings();
void HandleToggleKey(unsigned) noexcept;
std::filesystem::path LutDirectory();
void ReloadLuts();
void BeginOpaque() noexcept;
void EndOpaque() noexcept;
void ResetSsaoHistory() noexcept;
void Scene() noexcept;
void HDR(ID3D11ShaderResourceView*) noexcept;
void Frame(IDXGISwapChain*) noexcept;
void UpdateDimensions(IDXGISwapChain*);
void BeforeResize(IDXGISwapChain*);
void SetOutputColorSpace(IDXGISwapChain*,DXGI_COLOR_SPACE_TYPE);
IDXGISwapChain* Wrap(IDXGISwapChain*) noexcept;
}
