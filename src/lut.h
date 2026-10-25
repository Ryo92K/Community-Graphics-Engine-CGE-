#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace cge {
inline bool ValidLutName(std::string_view name) noexcept {
    if(name.empty())return true;
    if(name.size()>128 || name.front()=='.' || name.front()==' ' || name.find("..")!=name.npos)return false;
    for(unsigned char c:name)if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ' || c=='_' || c=='-' || c=='.'))return false;
    if(name.size()<6)return false;
    const auto suffix=name.substr(name.size()-5);
    constexpr std::string_view expected=".cube";
    for(unsigned i=0;i<5;++i){auto c=suffix[i];if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');if(c!=expected[i])return false;}
    return true;
}
struct CubeLut {
    unsigned size{};
    std::array<float,3> minimum{0,0,0},maximum{1,1,1};
    std::vector<std::array<float,4>> values;
};
bool LoadCube(const std::filesystem::path&,CubeLut&,std::string&);
std::vector<std::string> ListLuts(const std::filesystem::path&);
}
