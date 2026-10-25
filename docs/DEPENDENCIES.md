# Dependencies and native interfaces

## Build

Windows x64, MSVC C++20, the Windows SDK, CMake 3.24 or later.
Python 3 is used by the test-only distribution audit.
The MSVC runtime is linked statically.
CGE uses system Direct3D 11.1, DXGI and D3DCompiler_47.

## Runtime

A listed SE/AE executable, its matching SKSE64, and the corresponding Address Library.
See RUNTIME_COMPATIBILITY.md for the exact adapter list and validation limits.
SKSE Menu Framework 3.14 or newer provides the settings interface. Install the
framework and its listed requirements separately.

## SKSE Menu Framework bridge

CGE registers nine pages on SKSE PostPostLoad, after all plugin DLLs have loaded.
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

CGE selects SE or AE IDs and field layouts from the executable version.
DrawInterfaceStart uses SE ID 79947 or AE ID 82084. Its trampoline copies only
whole supported prologue instructions; relative or unsupported instructions and
pre-existing branch hooks are refused. No arbitrary byte sequence is relocated.

HDR uses SE ID 99023 at +0x1EA and +0x230, or AE ID 105674 at +0x178.
Call arguments, executable target ownership and supported target prologues are
checked before patching. AE also validates the target against ID 105676.
SE validates both sites before installing either. Effects modify the native HDR
source, preserving the tone mapper and built-in bloom.

Environment and clock readers use bounded field copies.
Motion resource adapters validate native d3d11.dll COM ownership, formats,
device ownership and dimensions before use. There is no CommonLib binary or
reference mod implementation linked into CGE.

References:
https://github.com/ianpatt/skse64/blob/master/skse64/PluginAPI.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/REL/IDDB.h
https://github.com/alandtse/CommonLibVR/blob/ng/src/REL/IDDB.cpp
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/Renderer.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/RenderTargetData.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/I/ImageSpaceManager.h
https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Hooks.cpp

## Test-only components

The SMF fixture exports a synthetic host API to exercise the real CGE.dll,
including delayed registration and unsupported API cases.
The D3D11 relay is also a regression fixture.
Neither component is installed or included in the mod archive.


## Distance fog (0.5.0)

No new external dependency is required. The fog pass uses native main-depth SRV
at Renderer + 0x2040 before 1.6.1130, or +0x20A0 from 1.6.1130 onward,
independently of the motion blur/TAA adapter.

The world-root singleton is SE ID 517006 / AE ID 403513. Its camera is at +0x128.
CGE validates WorldRootCamera (SE ID 35601 / AE ID 36609) against this access path before enabling
the adapter. NiCamera near/far values at +0x160/+0x164 and the orthographic flag
at +0x168 are read through bounded copies and checked for validity.
The getter and frustum-copy offsets were checked on the installed 1.7.104 executable.

Forward-Z depth is reconstructed as:
distance = near * far / (far * (1 - depth) + near * depth).

Fog opacity is 1 - exp(-max(distance - start, 0) * density / 5000).
The distance is in game units along the camera view axis.
Depth clear values (sky) and invalid samples are unchanged.
A fixed linear RGB (0.65, 0.70, 0.75) tint is used for this initial implementation.
The pass preserves alpha and the native device-context state.

The pipeline is HDR scene -> distance fog -> depth of field -> bloom -> native tone mapping.
Color grading and motion blur retain their existing pre-HUD boundary.

Camera layout references:
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/N/NiCamera.h
https://github.com/alandtse/CommonLibVR/blob/ng/src/RE/M/Main.cpp

The shader, resource management, camera adapter and tests are CGE implementations.


## Manual depth of field (0.5.1)

No new external dependency is required. Fog and depth of field share the
DepthInputs adapter in src/depth_inputs.h, with one native probe per HDR frame
when either effect needs it.

The background circle-of-confusion radius is:
16 * strength * saturate(1 - focusDistance / viewDistance), in native output pixels.
Pixels at or in front of the focus distance remain unchanged.
A deterministic 32-sample disk gathers color and matching point depth. Focused
or substantially nearer samples are rejected to protect foreground boundaries.
The pass uses no temporal history or autofocus.

The project license is embedded as the CGE_LICENSE RCDATA resource in CGE.dll.
Nexus RAR packages contain only SKSE/ and are named Community Graphics Engine (CGE).rar.
Source packages retain the LICENSE file and full build sources.


## Built-in presets (0.5.1)

The five preset definitions are embedded in src/presets.cpp.
They require no additional file, library or mod. They match the optional preset
INI files distributed separately.
Applying a preset updates all settings and resets the displayed profile blend
so the selected look appears on the next frame. Doorway/day/night transitions
continue to use the preset's normal timing.
A one-step session snapshot provides Undo Preset. Neither operation writes the
INI until the user selects Save.


## Clarity (0.5.2)

Clarity runs in the existing SDR color shader at the verified boundary before
the native interface. It uses the unmodified source luminance and a 5 by 5 tent
kernel with luminance range weights. Sample spacing scales with render height,
with a minimum of one pixel. At 1080p the kernel extends eight pixels each way.
The luminance difference is limited to 0.08 and attenuated near black and white.
The adjustment moves RGB towards black or white within the available gamut,
preserving neutral colors and source alpha.

The effect shares the color pass, source copy and context isolation. It adds no
render target, temporal history, depth input or TAA requirement. Enabled strength
above zero adds 24 neighboring texture reads per pixel; performance in game
depends on the GPU and render resolution. Disabled or zero strength skips them.

Clarity defaults to disabled with Strength 0.25. Shared values use [Clarity],
Enabled and Strength; profiles use ClarityEnabled and ClarityStrength. Strength
is restricted to 0 through 1. Missing keys retain the prior appearance. Profile
transitions fade disabled endpoints through zero strength. Reset Color preserves
Clarity, and Reset Clarity changes only the selected profile. Built-in presets
retain their original appearance with Clarity disabled.

GPU tests cover local contrast, strength, exact bypasses, exposure composition,
alpha, bounded output, neutral colors, black/white edges, render-resolution
scaling and state restoration. Settings and framework tests cover profile
selection, numeric input, persistence, inheritance, reset isolation and the
native scene/HUD boundary. In-game appearance and GPU timing remain to be checked.


## Runtime adapters (0.6.0)

runtime.h contains the explicit executable allowlist, versioned library filename,
relocation IDs, render-target bounds and field offsets. Unknown builds, VR and
classic Skyrim are refused. Matching Address Library headers are mandatory;
CGE does not try a different version's table when its own file is missing.

address_library.cpp reads V1 and V2 sparse delta encodings and the V5 dense
table. Input length, counts, name, version, pointer width, arithmetic overflow,
duplicate IDs and final file position are checked. The parsed table is cached
during initialization. Game addresses must remain inside readable executable
memory; hook targets must also belong to executable code in the game module.

SKSEPlugin_Query remains exported for older SKSE64 releases. The AE metadata
contains the explicit AE allowlist and V5 encoding flag. CGE does not read the
optional GetPluginInfo tail of the SKSE interface or pin a particular SKSE
package version. The installed SKSE itself must match the game executable.
UI registration accepts the required messaging V1 prefix without reading later
fields, and checks framework exports separately. A guard-page test verifies that
the optional SKSE interface tail is not accessed.

The project's distribution contains no Address Library tables or reference-mod
code. Installed library checks run against read-only local files and are not
required for the portable automated test suite.
