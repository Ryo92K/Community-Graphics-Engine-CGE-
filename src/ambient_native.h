#pragma once
#include <d3d11.h>
#include <string>
namespace cge::ambient {
bool Attach(ID3D11Device* device,std::string& error);
void Detach() noexcept;
void Begin(ID3D11ShaderResourceView* visibility) noexcept;
void End() noexcept;
std::string Diagnostics();
}
