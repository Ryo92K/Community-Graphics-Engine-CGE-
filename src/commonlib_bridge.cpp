#include <SKSE/Impl/PCH.h>
#include <SKSE/Interfaces.h>
#include <RE/N/NiCamera.h>
#include <RE/S/Sky.h>
#include <RE/T/TESWeather.h>
#include <RE/R/Renderer.h>
#include "commonlib_bridge.h"
#include "hotkey.h"
#include <RE/B/ButtonEvent.h>
namespace cge::commonlib {
static_assert(offsetof(skse::Interface,runtimeVersion)==offsetof(SKSE::detail::SKSEInterface,runtimeVersion));
static_assert(offsetof(skse::Interface,queryInterface)==offsetof(SKSE::detail::SKSEInterface,QueryInterface));
static_assert(offsetof(skse::Interface,getPluginHandle)==offsetof(SKSE::detail::SKSEInterface,GetPluginHandle));
static_assert(sizeof(skse::Info)==sizeof(SKSE::PluginInfo));
static_assert(sizeof(skse::Metadata)==sizeof(SKSE::PluginVersionData));
static_assert(offsetof(skse::Metadata,compatibleVersions)==offsetof(SKSE::PluginVersionData,compatibleVersions));
static_assert(sizeof(skse::Message)==sizeof(SKSE::MessagingInterface::Message));
static_assert(offsetof(skse::Message,data)==offsetof(SKSE::MessagingInterface::Message,data));
static_assert(runtime::Layout{}.renderTargets==offsetof(RE::BSGraphics::Renderer,data)+offsetof(RE::BSGraphics::RendererData,renderTargets));
static_assert(runtime::Layout{}.targetStride==sizeof(RE::BSGraphics::RenderTargetData));
static_assert(runtime::Layout{}.targetView==offsetof(RE::BSGraphics::RenderTargetData,SRV));
static_assert(runtime::Layout{}.cameraProjection==sizeof(RE::NiAVObject));
static_assert(runtime::Layout{}.cameraPosition==offsetof(RE::NiAVObject,world)+offsetof(RE::NiTransform,translate));
static_assert(offsetof(RE::Sky,currentWeather)==0x48);
static_assert(offsetof(RE::Sky,lastWeather)==0x50);
static_assert(offsetof(RE::Sky,currentWeatherPct)==0x1b8);
static_assert(offsetof(RE::TESWeather,data)+offsetof(RE::TESWeather::Data,flags)==0x66f);
void* InvokeSky(std::uintptr_t address) {
    REL::Relocation<RE::Sky*()> call{address};return call();
}
void* InvokeCamera(std::uintptr_t address) {
    REL::Relocation<RE::NiCamera*()> call{address};return call();
}
void ProcessKeyboard(const void* events,void(*press)(unsigned) noexcept) noexcept {
    static KeyboardPresses keys;
    if(!events || !press)return;
    const auto* list=static_cast<RE::InputEvent* const*>(events);
    unsigned count=0;
    for(auto* event=*list;event && count++<4096;event=event->next) {
        if(event->GetDevice()!=RE::INPUT_DEVICE::kKeyboard || event->GetEventType()!=RE::INPUT_EVENT_TYPE::kButton)continue;
        const auto* button=static_cast<const RE::ButtonEvent*>(event);
        if(keys.Observe(button->GetIDCode(),button->Value(),button->HeldDuration()))press(button->GetIDCode());
    }
}
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
void InvokeImageSpace(std::uintptr_t address,void* manager,std::uintptr_t effect,std::uint32_t source,std::uint32_t target,void* parameters) {
    REL::Relocation<void(void*,std::uintptr_t,std::uint32_t,std::uint32_t,void*)> call{address};call(manager,effect,source,target,parameters);
}
}
