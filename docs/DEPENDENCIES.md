# Dependencies and native interfaces

## Build

Windows x64, MSVC C++20, the Windows SDK, CMake 3.24 or later.
Python 3 is used by the test-only distribution audit.
The MSVC runtime is linked statically.
CGE uses system Direct3D 11.1, DXGI and D3DCompiler_47.

## Runtime

Skyrim Steam 1.7.104, SKSE 2.3.1 and Address Library V5 for this executable.
SKSE Menu Framework 3.14 or newer provides the settings interface. Install the
framework and its listed requirements separately.

## SKSE Menu Framework bridge

CGE registers five pages on SKSE PostPostLoad, after all plugin DLLs have loaded.
It does not load an arbitrary framework DLL from disk or require a DLL load-order shim.

src/menu_framework.cpp contains CGE's minimal declarations of the public export
ABI. It resolves all required symbols before registering any page. Missing
modules, incompatible API generations and incomplete exports produce a log
message and no local fallback menu.

GetMenuFrameworkVersion reports an API generation: the inspected provider
returns 3.8, independently of the package's release version. CGE checks the
generation and required exports. The 3.14 package minimum is needed for the
framework's Skyrim 1.7 support.

Public API inspected at commit:
1dcb70179076aae4ab626f43c5baab2735ca5877
https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API

Provider implementation:
https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/src/SKSEMenuFramework.cpp

No consumer header or framework implementation is copied into CGE.
No Dear ImGui source, backend or font is compiled or distributed by CGE.
The separate framework controls its own styling, rendering and input.

## Native rendering interfaces

The exact 1.7.104 ABI and hook bytes are validated before modification.
CGE hooks DrawInterfaceStart (AE ID 82084) for color/motion effects before the HUD.
HDR bloom hooks the main cinematic tone-mapping call at ID 105674 + 0x178 and
validates its original target (105676). It modifies the HDR source passed by
that call, preserving the native tone mapper and built-in bloom.

Environment and clock readers use bounded field copies.
Motion resource adapters validate native d3d11.dll COM ownership, formats,
device ownership and dimensions before use. There is no CommonLib binary or
reference mod implementation linked into CGE.

References:
https://github.com/ianpatt/skse64/blob/master/skse64/PluginAPI.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/REL/IDDB.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/Renderer.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/RenderTargetData.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/I/ImageSpaceManager.h
https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Hooks.cpp

## Test-only components

The SMF fixture exports a synthetic host API to exercise the real CGE.dll,
including delayed registration and unsupported API cases.
The D3D11 relay is also a regression fixture.
Neither component is installed or included in the mod archive.
