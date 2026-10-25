#pragma once
#include <cstddef>
#include <cstdint>
                                                           
                                                                                             
namespace cge::skse {
constexpr std::uint32_t Runtime=(1u<<24)|(7u<<16)|(104u<<4);
constexpr std::uint32_t Version=(5u<<16)|(1u<<4);
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
static_assert(sizeof(Metadata)==848);
static_assert(offsetof(Metadata,versionIndependenceEx)==772);
static_assert(offsetof(Metadata,compatibleVersions)==780);
inline bool Compatible(const Interface* api) noexcept {
    return api && !api->isEditor && api->runtimeVersion==Runtime;
}
}
