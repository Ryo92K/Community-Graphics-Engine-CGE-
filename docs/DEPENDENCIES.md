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

CGE registers fourteen pages on SKSE PostPostLoad, after all plugin DLLs have loaded.
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
Clarity, and Reset Clarity changes only the selected profile. The 0.6.1 built-in presets use restrained Clarity strengths; Soft Focus keeps
Clarity disabled.

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


## Vibrance (0.6.1)

Vibrance expands chroma around luminance after the global saturation control and
before gamma in the existing SDR color pass. The boost decreases with the square
of the remaining saturation headroom. A shared chroma scale is limited by the
available RGB gamut, retaining chroma direction and luminance before gamma.
Neutral grays, black, white and fully saturated colors are unchanged.
This is a per-pixel color adjustment without skin, face or material detection.

The effect adds no texture samples, render targets, native hooks or dependencies.
Disabled or zero Strength bypasses it. Defaults are disabled and Strength 0.25.
The valid range is 0 through 1. Shared INI values use [Vibrance], Enabled and
Strength; profile sections use VibranceEnabled and VibranceStrength.
Save, profile transitions, resets and preset replacement include the effect.
Reset Color preserves it; Reset Vibrance affects only the selected profile.
Existing configurations keep Vibrance disabled. The 0.6.1 built-in presets
use style-specific Clarity and Vibrance values, with reduced sharpness.
In-game appearance remains to be checked.

Preset detail and color balance were retuned for 0.6.1. Natural uses light
enhancement, Nordic adds texture definition with a restrained color boost,
Warm Fantasy has the strongest Vibrance, Cinematic keeps it subtle, and
Soft Focus disables Clarity and uses minimal sharpness. Night strengths are
lower than day strengths. Existing saved settings are not changed until the
user selects a preset; applying still requires Save for persistence.


Effect parameter controls use nested disabled scopes after each enable checkbox.
Disabled effects keep their slider, numeric input, reset and motion quality
controls unavailable. The master switch disables parameter editing across
effects, and automatic-profile timing controls are unavailable when automatic
profiles are off. Effect switches, profile selection and Save remain outside
the dependent parameter scope. Inactive profile scopes remain authoritative
when nested inside enabled effects. Re-enabling restores the stored values.
Framework tests cover blocked edits and resets, reactivation, master state,
inactive profiles and balanced disabled scopes.


## Finishing effects (0.6.2)

Vignette, Film Grain, Split Toning and Chromatic Aberration are implemented in
CGE.hlsl and share the existing pre-HUD color pipeline. They default to disabled.
The CPU/HLSL control buffer is 144 bytes with explicit float4 groups.

finishing_settings.h defines each numeric field, its bounds and default once.
INI parsing, writing, finite-value validation and menu controls use this schema.
Profile fields prefix the effect name, for example VignetteStrength or
SplitToningShadowRed. Shared values use separate effect sections.
All effect strengths fade through zero at disabled transition endpoints.
Reset Color preserves these settings. Each effect reset changes only its own
selected profile, and inactive controls remain grayed out.

Vignette uses corner-normalized radial distance with pixel aspect correction.
Radius preserves the central region and Softness controls the edge transition.
Radius 1 or Strength 0 bypasses it. No texture reads are added.

Film Grain uses an integer spatial hash and a steady-clock tick at 24 Hz.
The same tick retains the same pattern; no temporal history texture is needed.
Size is measured in render pixels, from 1 to 4. Strength is at most 1.
Monochrome noise is attenuated near black and white. It follows gamma and is
applied after motion filtering, including stationary/invalid-vector fallbacks.
A supplied tick allows deterministic rendering tests without sleeping.

Split Toning blends user-defined shadow/highlight RGB palettes around Balance.
The tint's luminance is removed and a shared scale limits the chroma offset to
the available gamut. Black and white are protected. It follows Vibrance and
precedes gamma. It performs no texture reads.

Chromatic Aberration offsets red and blue symmetrically along the radial axis.
Green and source alpha remain centered. A protected central radius and squared
edge mask restrict the displacement. Maximum extent at Strength 1 is eight
pixels at 1080p, with a one-pixel minimum at low resolution. The shader reads
bilinear source samples through clamped integer loads, requiring up to eight
additional texture reads only where the effect is active. It needs no sampler,
depth resource or new render target.

The five presets retain their 0.6.1 color/detail balance. Natural leaves the four
new treatments off. Nordic and Warm Fantasy use modest Split Toning. Cinematic
and Soft Focus use restrained vignette, grain, toning and aberration, with lower
strengths at night. Disabled treatments retain usable starting values for later
activation. Soft Focus still keeps Clarity off. Optional INIs are exported
from the same definitions and checked after reloading. Existing user settings
are not replaced until a preset is selected and saved.

49 automated tests include separate GPU checks for all four effects, grain after
stationary and moving motion filtering, fixed-tick combinations, profile
persistence and transitions, disabled controls, resets, native scene edges,
HUD preservation and exact preset export/reload. In-game appearance and GPU
timings remain to be checked.
