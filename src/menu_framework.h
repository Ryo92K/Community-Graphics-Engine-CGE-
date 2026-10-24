#pragma once
#include <string>
namespace cge::menu {
bool Register(std::string& status);
bool IsBlocking() noexcept;
}
