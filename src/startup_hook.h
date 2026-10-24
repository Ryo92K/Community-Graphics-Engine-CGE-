#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
namespace cge::startup {
bool HookCreation(HMODULE executable,PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN replacement,
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN& original,std::string& error);
void Shutdown() noexcept;
}
