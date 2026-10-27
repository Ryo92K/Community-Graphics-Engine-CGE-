#pragma once
#include "skse_api.h"
#include <cstdint>
namespace cge::commonlib {
void ProcessKeyboard(const void*,void(*)(unsigned) noexcept) noexcept;
bool Compatible(const skse::Interface*) noexcept;
bool RegisterListener(const skse::Interface*,void(*)(skse::Message*));
void* InvokeCamera(std::uintptr_t);
void* InvokeSky(std::uintptr_t);
void InvokeInterface(std::uintptr_t,std::int64_t);
void InvokeImageSpace(std::uintptr_t,void*,std::uintptr_t,std::uint32_t,std::uint32_t,void*);
}
