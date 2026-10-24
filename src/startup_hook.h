#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <filesystem>
#include "settings.h"
#include "motion_blur.h"
#include "fog.h"
namespace cge::startup {
bool HookCreation(HMODULE executable,PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN replacement,
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN& original,std::string& error);
bool HookInterface(HMODULE executable,const std::filesystem::path& library,void(*before)() noexcept,std::string& error);
bool InitializeEnvironment(HMODULE executable,const std::filesystem::path& library,std::string& error);
Environment PollEnvironment() noexcept;
bool InitializeClock(HMODULE executable,const std::filesystem::path& library,std::string& error);
float PollGameHour() noexcept;
bool InitializeMotion(HMODULE executable,const std::filesystem::path& library,std::string& error);
MotionInputs PollMotion() noexcept;
bool InitializeFog(HMODULE,const std::filesystem::path&,std::string& error);
FogInputs PollFog() noexcept;
bool HookTonemap(HMODULE,const std::filesystem::path&,void(*before)(ID3D11ShaderResourceView*) noexcept,std::string& error);
void Shutdown() noexcept;
}
