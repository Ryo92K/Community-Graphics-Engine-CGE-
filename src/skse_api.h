#pragma once
#include <cstddef>
#include <cstdint>
// Minimal declarations for the documented SKSE plugin ABI.
// No game object layouts, SDK implementation or external plugin source is linked.
namespace cge::skse {
constexpr std::uint32_t Runtime=(1u<<24)|(7u<<16)|(104u<<4);
constexpr std::uint32_t Version=1u<<16;
struct Interface {
    std::uint32_t skseVersion,runtimeVersion,editorVersion,isEditor;
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
