#include <SKSE/Impl/PCH.h>
#include <SKSE/Interfaces.h>
#include "commonlib_bridge.h"
namespace cge::commonlib {
static_assert(offsetof(skse::Interface,runtimeVersion)==offsetof(SKSE::detail::SKSEInterface,runtimeVersion));
static_assert(offsetof(skse::Interface,queryInterface)==offsetof(SKSE::detail::SKSEInterface,QueryInterface));
static_assert(offsetof(skse::Interface,getPluginHandle)==offsetof(SKSE::detail::SKSEInterface,GetPluginHandle));
static_assert(sizeof(skse::Info)==sizeof(SKSE::PluginInfo));
static_assert(sizeof(skse::Metadata)==sizeof(SKSE::PluginVersionData));
static_assert(offsetof(skse::Metadata,compatibleVersions)==offsetof(SKSE::PluginVersionData,compatibleVersions));
static_assert(sizeof(skse::Message)==sizeof(SKSE::MessagingInterface::Message));
static_assert(offsetof(skse::Message,data)==offsetof(SKSE::MessagingInterface::Message,data));
bool Compatible(const skse::Interface* api) noexcept {
    if(!api)return false;
    const auto* typed=reinterpret_cast<const SKSE::LoadInterface*>(api);
    return !typed->IsEditor() && (typed->SKSEVersion()>>24)==2 && runtime::Select(typed->RuntimeVersion().pack()).has_value();
}
bool RegisterListener(const skse::Interface* api,void(*callback)(skse::Message*)) {
    if(!api || !api->queryInterface || !api->getPluginHandle || !callback)return false;
    const auto* typed=reinterpret_cast<const SKSE::LoadInterface*>(api);
    const auto* messaging=static_cast<const SKSE::detail::SKSEMessagingInterface*>(typed->QueryInterface(SKSE::LoadInterface::kMessaging));
    if(!messaging || messaging->interfaceVersion<1 || !messaging->RegisterListener)return false;
    return messaging->RegisterListener(typed->GetPluginHandle(),"SKSE",reinterpret_cast<void*>(callback));
}
void InvokeInterface(std::uintptr_t address,std::int64_t manager) {
    REL::Relocation<void(std::int64_t)> call{address};call(manager);
}
void InvokeImageSpace(std::uintptr_t address,void* manager,std::uint32_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
    REL::Relocation<void(void*,std::uint32_t,std::uint32_t,std::uint32_t,void*)> call{address};call(manager,effect,source,target,parameters);
}
}
