#pragma once
#include <cstddef>
#include "runtime.h"
#include <cstdint>
                                                           
                                                                                             
namespace cge::skse {
constexpr std::uint32_t Runtime=runtime::DefaultVersion;
constexpr std::uint32_t Version=(8u<<16);
struct Interface {
    std::uint32_t skseVersion,runtimeVersion,editorVersion,isEditor;
    void* (*queryInterface)(std::uint32_t){};
    std::uint32_t (*getPluginHandle)(){};
    std::uint32_t (*getReleaseIndex)(){};
    const void* (*getPluginInfo)(const char*){};
};
struct Message {const char* sender;std::uint32_t type,dataLength;void* data;};
struct Messaging {
    std::uint32_t version;
    bool (*registerListener)(std::uint32_t,const char*,void(*)(Message*));
    bool (*dispatch)(std::uint32_t,std::uint32_t,void*,std::uint32_t,const char*);
    void* (*getEventDispatcher)(std::uint32_t);
};
struct Info {
    std::uint32_t infoVersion;
    const char* name;
    std::uint32_t version;
};
struct Metadata {
    std::uint32_t dataVersion,pluginVersion;
    char name[256],author[256],supportEmail[252];
    std::uint32_t versionIndependenceEx,versionIndependence;
    std::uint32_t compatibleVersions[16],minimumSkse;
};
constexpr Metadata BuildMetadata() noexcept {
    Metadata result{1,Version,"Community Graphics Engine","CGE contributors","",2,0,{},0};
    constexpr std::uint32_t versions[]={
        runtime::Pack(5,3),runtime::Pack(5,16),runtime::Pack(5,23),runtime::Pack(5,39),runtime::Pack(5,50),
        runtime::Pack(5,53),runtime::Pack(5,62),runtime::Pack(5,73),runtime::Pack(5,80),runtime::Pack(5,97),
        runtime::Pack(6,317),runtime::Pack(6,318),runtime::Pack(6,323),runtime::Pack(6,342),runtime::Pack(6,353),
        runtime::Pack(6,629),runtime::Pack(6,640),runtime::Pack(6,659),runtime::Pack(6,1130),runtime::Pack(6,1170),
        runtime::Pack(6,1179),runtime::Pack(7,99),runtime::Pack(7,104),runtime::Pack(6,659)|1u,runtime::Pack(6,1179)|1u};
    unsigned at=0;
    for(auto version:versions) {
        if constexpr(runtime::CompiledMinor==0)if(((version>>16)&255)==5)continue;
        if(runtime::Select(version))result.compatibleVersions[at++]=version;
    }
    return result;
}
static_assert(sizeof(Metadata)==848);
static_assert(offsetof(Metadata,versionIndependenceEx)==772);
static_assert(offsetof(Metadata,compatibleVersions)==780);
inline bool Compatible(const Interface* api) noexcept {
    return api && !api->isEditor && (api->skseVersion>>24)==2 && runtime::Select(api->runtimeVersion).has_value();
}
}
