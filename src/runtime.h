#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <optional>
namespace cge::runtime {
constexpr std::uint32_t Pack(unsigned minor,unsigned build) noexcept {return (1u<<24)|(minor<<16)|(build<<4);}
enum class Family {SE,AE};
enum class Address {Interface,Player,Calendar,Renderer,UI,ImageSpace,WorldRoot,CameraGetter,Tonemap,Draw};
struct Layout {
    std::uint32_t version{};
    Family family{Family::AE};
    std::array<std::uint32_t,10> ids{};
    unsigned depthView{},renderTargets{0xa58},targetCount{114},camera{0x128},frustum{0x150},temporal{0x1f0};
    std::array<unsigned,2> tonemapCalls{};
    unsigned callCount{1};
    std::uint32_t Id(Address key) const noexcept {return ids[static_cast<unsigned>(key)];}
    std::string VersionString(char separator='.') const {
        return "1"+std::string(1,separator)+std::to_string((version>>16)&255)+separator+
            std::to_string((version>>4)&4095)+separator+"0";
    }
    std::string LibraryName() const {
        return std::string(family==Family::AE?"versionlib-":"version-")+VersionString('-')+".bin";
    }
};
inline std::optional<Layout> Select(std::uint32_t version) noexcept {
    if((version>>24)!=1 || (version&15))return {};
    const auto minor=(version>>16)&255,build=(version>>4)&4095;
    constexpr std::array se={3u,16u,23u,39u,50u,53u,62u,73u,80u,97u};
    constexpr std::array ae={317u,318u,323u,342u,353u,629u,640u,659u,1130u,1170u,1179u};
    auto contains=[build](const auto& values){for(auto v:values)if(v==build)return true;return false;};
    Layout result;result.version=version;
    if(minor==5 && contains(se))result.family=Family::SE;
    else if((minor==6 && contains(ae)) || (minor==7 && (build==99 || build==104)))result.family=Family::AE;
    else return {};
    const bool old=result.family!=Family::AE;
    result.ids=old?std::array<std::uint32_t,10>{79947,517014,514287,524907,514178,527731,517006,35601,99023,0}:
        std::array<std::uint32_t,10>{82084,403521,400447,411393,400327,414660,403513,36609,105674,105676};
    result.depthView=version>=Pack(6,1130)?0x20a0:0x2040;
    result.targetCount=version>=Pack(6,1130)?116:114;
    result.tonemapCalls=old?std::array<unsigned,2>{0x1ea,0x230}:std::array<unsigned,2>{0x178,0};
    result.callCount=old?2:1;
    return result;
}
inline Layout selected=*Select(Pack(7,104));
}
