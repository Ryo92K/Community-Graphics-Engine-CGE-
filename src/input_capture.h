#pragma once
#include <windows.h>
#include <string>
namespace cge::input {
using Predicate=bool(*)();
bool Install(Predicate capture,std::string& status);
void Shutdown() noexcept;
bool HookDeviceTable(void** table);
void Configure(Predicate capture) noexcept;
}
