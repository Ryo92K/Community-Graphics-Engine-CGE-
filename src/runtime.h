#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#ifndef CGE_RUNTIME_MINOR
#define CGE_RUNTIME_MINOR 0
#endif
namespace cge::runtime {
inline constexpr unsigned CompiledMinor=CGE_RUNTIME_MINOR;
static_assert(CompiledMinor==0 || CompiledMinor==5 || CompiledMinor==6 || CompiledMinor==7);
inline constexpr const char* BuildFamily=CompiledMinor==5?"1.5.x":CompiledMinor==6?"1.6.x":CompiledMinor==7?"1.7.x":"internal multi-runtime";
constexpr std::uint32_t Pack(unsigned minor,unsigned build) noexcept {return (1u<<24)|(minor<<16)|(build<<4);}
enum class Family {SE,AE};
enum class Generation {SE,AE,AE1130,AE17};
enum class Address {Interface,Player,Calendar,Renderer,UI,ImageSpace,WorldRoot,CameraGetter,Tonemap,Draw,WorldOpaque,WorldFrame,InputDispatch,SkyGetter};
struct Layout {
    std::uint32_t version{};
    Family family{Family::AE};
    Generation generation{Generation::AE17};
    std::array<std::uint32_t,13> ids{};
    unsigned depthView{},renderTargets{0xa58},targetCount{114},camera{0x128},frustum{0x150},temporal{0x1f0};
    std::array<unsigned,2> tonemapCalls{};
    unsigned callCount{1};
    unsigned worldCall{},opaqueCall{},decalCall{};
    unsigned targetStride{0x30},targetView{0x18},velocityTarget{7};
    unsigned uiPauses{0x160},temporalEnabled{0x18};
    unsigned cameraProjection{0x110},cameraPosition{0xa0};
    const char* AdapterName() const noexcept {
        switch(generation) {
        case Generation::SE:return "SE 1.5";
        case Generation::AE:return "AE 1.6 before 1130";
        case Generation::AE1130:return "AE 1.6 from 1130";
        case Generation::AE17:return "AE 1.7";
        }
        return "Unknown";
    }
    std::uint32_t Id(Address key) const noexcept {return key==Address::SkyGetter?(family==Family::SE?13789u:13878u):ids[static_cast<unsigned>(key)];}
    std::string VersionString(char separator='.') const {
        return "1"+std::string(1,separator)+std::to_string((version>>16)&255)+separator+
            std::to_string((version>>4)&4095)+separator+"0";
    }
    std::string LibraryName() const {
        return std::string(family==Family::AE?"versionlib-":"version-")+VersionString('-')+".bin";
    }
};
inline constexpr std::optional<Layout> Select(std::uint32_t version) noexcept {
    if((version>>24)!=1)return {};
    const auto minor=(version>>16)&255,build=(version>>4)&4095;
    if constexpr(CompiledMinor!=0)if(minor!=CompiledMinor)return {};
    const auto variant=version&15;
    if(variant>1 || (variant==1 && !(minor==6 && (build==659 || build==1179))))return {};
    constexpr std::array se={3u,16u,23u,39u,50u,53u,62u,73u,80u,97u};
    constexpr std::array ae={317u,318u,323u,342u,353u,629u,640u,659u,1130u,1170u,1179u};
    auto contains=[build](const auto& values){for(auto v:values)if(v==build)return true;return false;};
    Layout result;result.version=version;
    if(minor==5 && contains(se))result.family=Family::SE;
    else if((minor==6 && contains(ae)) || (minor==7 && (build==99 || build==104)))result.family=Family::AE;
    else return {};
    const bool old=result.family!=Family::AE;
    result.generation=old?Generation::SE:minor>=7?Generation::AE17:
        build>=1130?Generation::AE1130:Generation::AE;
    result.worldCall=old?0x831:result.generation==Generation::AE17?0x85e:0x841;
    result.opaqueCall=old?0x8e:0x84;result.decalCall=old?0x319:0x308;
    result.ids=old?std::array<std::uint32_t,13>{79947,517014,514287,524907,514178,527731,517006,35601,99023,0,99938,35560,67315}:
        std::array<std::uint32_t,13>{82084,403521,400447,411393,400327,414660,403513,36609,105674,105676,106583,36559,68617};
    result.depthView=version>=Pack(6,1130)?0x20a0:0x2040;
    result.targetCount=version>=Pack(6,1130)?116:114;
    result.tonemapCalls=old?std::array<unsigned,2>{0x1ea,0x230}:std::array<unsigned,2>{0x178,0};
    result.callCount=old?2:1;
    return result;
}
inline constexpr std::uint32_t DefaultVersion=CompiledMinor==5?Pack(5,97):CompiledMinor==6?Pack(6,1170):Pack(7,104);
inline Layout selected=*Select(DefaultVersion);
}
