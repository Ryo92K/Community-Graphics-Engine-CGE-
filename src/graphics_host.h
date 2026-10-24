#pragma once
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <functional>
#include <string>
#include "settings.h"
#include "motion_blur.h"
namespace cge::graphics {
using Microsoft::WRL::ComPtr;
using Reporter=std::function<void(const std::string&)>;
bool Attach(IDXGISwapChain*,ID3D11Device*,const std::filesystem::path&,Reporter);
void Detach(IDXGISwapChain*);
void SetEnvironmentProbe(Environment(*probe)() noexcept);
void SetGameHourProbe(float(*probe)() noexcept);
void SetMotionProbe(MotionInputs(*probe)() noexcept);
void Scene() noexcept;
void Frame(IDXGISwapChain*) noexcept;
void UpdateDimensions(IDXGISwapChain*);
IDXGISwapChain* Wrap(IDXGISwapChain*) noexcept;
}
