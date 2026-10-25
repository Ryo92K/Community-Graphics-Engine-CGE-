#pragma once
#include "runtime.h"
#include <filesystem>
#include <string>
#include <vector>
#include <utility>
namespace cge {
class AddressLibrary {
    std::vector<std::pair<std::uint32_t,std::uint32_t>> entries_;
public:
    bool Load(const std::filesystem::path&,const runtime::Layout&,std::string&);
    std::uint32_t Offset(std::uint32_t) const noexcept;
    std::size_t Size() const noexcept {return entries_.size();}
};
}
