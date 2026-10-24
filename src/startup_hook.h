#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <filesystem>
namespace cge::startup {
bool HookCreation(HMODULE executable,PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN replacement,
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN& original,std::string& error);
bool HookInterface(HMODULE executable,const std::filesystem::path& library,void(*before)() noexcept,std::string& error);
void Shutdown() noexcept;
}
