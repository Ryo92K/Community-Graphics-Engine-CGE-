# Dependencies and native interfaces

## Build dependencies

CGE requires Windows x64, MSVC with C++23 support, the Windows SDK, CMake 3.24 or
later and Git. Python 3 runs the test-only distribution audit. The MSVC runtime
and CommonLibSSE-NG are linked statically.

Pinned sources are declared in cmake/Dependencies.cmake:

| Dependency | Commit |
| --- | --- |
| CommonLibSSE-NG | b93280e832f263dbef44e44cbe2936622a02f91a |
| spdlog | 6fa36017cfd5731d617e1a934f0e5ea9c4445b13 |
| rapidcsv | 3c8131af7596329c50380ef3f8975e45742b1d60 |

Upstream URLs and license texts are recorded in THIRD-PARTY-NOTICES.txt and
embedded in CGE.dll together with the project license. No external AO shader,
LUT runtime or graphics mod implementation is bundled.

## Runtime dependencies

Use a listed SE/AE executable, its matching SKSE64 and corresponding Address
Library. See RUNTIME_COMPATIBILITY.md for the exact adapter list and limits.
CGE uses the system Direct3D 11.1, DXGI and D3DCompiler_47 libraries.
Motion Blur additionally requires native TAA and native render resolution.

SKSE Menu Framework 3.14 or newer provides the settings UI. Install it and its
listed requirements separately in versions compatible with your executable.
No separate CommonLib DLL, Dear ImGui package or CGE-specific font is required.

## Framework bridge

CGE registers twenty-two pages on SKSE PostPostLoad. The SSAO page is labeled
SSAO. The bridge resolves all required public exports before registering
any page. Missing, old or incomplete APIs produce a log message without a
private fallback menu. Effects can still use the INI.

The provider owns rendering, styling and input capture. CGE includes no consumer
SDK header, provider implementation, ImGui backend, font or input hook.

Public API reference: QTR-Modding/SKSE-Menu-Framework-3-API, commit
1dcb70179076aae4ab626f43c5baab2735ca5877.
The framework export generation and package version are checked separately.

## Native interfaces

CommonLibSSE-NG provides typed SKSE interface access and native delegates. CGE
retains its bounded V1/V2/V5 Address Library reader and per-runtime render
adapters. Unknown executable builds and unsupported instruction sequences are
rejected. Linking CommonLib does not establish compatibility by itself.

The interface boundary uses SE ID 79947 or AE ID 82084. Its trampoline accepts
whole supported prologue instructions and refuses relative instructions or
existing conflicting branches. Effects run before HUD and menu composition.

The HDR boundary uses SE ID 99023 at +0x1EA and +0x230, or AE ID 105674 at +0x178.
Call arguments, executable ownership and target prologues are validated before
patching; AE also checks target ID 105676. SE validates both call sites first.
The game's tone mapper and built-in bloom remain in the pipeline.

Environment, calendar and camera readers use bounded memory copies. Native COM
views are checked for system D3D11 ownership, device, dimensions, format and
sampling. Missing or invalid resources bypass the corresponding effect.

## Render path

SSAO computes visibility from the depth prepass and modulates native directional
ambient light during opaque material rendering. Its bindings end before blended
decals and water. HDR order afterwards: distance fog, depth of field, bloom with
optional Lens Dirt, then the native tone mapper. SDR order: detail and color
adjustments, 3D LUT, vignette and grain, motion blur, lens distortion, then HUD.

SSAO uses depth-derived geometric normals. Recognized material bytecode is
transformed in memory; no game shaders or external AO implementation are bundled.
Unknown signatures retain native shading. See SSAO.md for limits and testing.

The five built-in visual presets keep Motion Blur settings when applied.
Optional preset INIs are complete replacements. Motion Blur starts off; SSAO uses
the same Balanced settings as the built-in presets.
Warm Fantasy and Soft Focus reference the included Warm.cube asset. Other .cube
files are loaded only from CGE's LUT directory, with path and table validation.

Output views are cached while their resources remain unchanged and released
before swap-chain resizing. Combined SDR passes reuse intermediate textures.
Context isolation restores native state after each effect. See PERFORMANCE.md.

## Test-only components

The framework fixture hosts a synthetic public API to exercise CGE.dll. The
D3D11 relay, captures and GPU tests are development tools. None are installed
or included in the Nexus archive. Tests use synthetic data, not game assets.
111 automated tests pass; real executable compatibility and GPU cost still
require in-game checks. The reported 1.6.1170 startup crash is unconfirmed.

## References

- https://github.com/ianpatt/skse64/blob/master/skse64/PluginAPI.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG
- https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
- https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/src/SKSEMenuFramework.cpp
- https://github.com/alandtse/CommonLibVR/blob/ng/include/REL/IDDB.h
- https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Hooks.cpp
