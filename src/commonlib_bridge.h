#pragma once
#include "skse_api.h"
#include <cstdint>
namespace cge::commonlib {
bool Compatible(const skse::Interface*) noexcept;
bool RegisterListener(const skse::Interface*,void(*)(skse::Message*));
void InvokeInterface(std::uintptr_t,std::int64_t);
void InvokeImageSpace(std::uintptr_t,void*,std::uint32_t,std::uint32_t,std::uint32_t,void*);
}
