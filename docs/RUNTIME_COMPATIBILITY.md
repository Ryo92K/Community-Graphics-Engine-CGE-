# Runtime compatibility

CGE 0.8.0 includes the following SE/AE adapters. Each executable requires its
matching SKSE64 and Address Library file. Older-runtime compatibility remains
experimental; listed adapters do not establish successful testing on every
executable. VR, classic Skyrim, Epic, Microsoft Store and unknown builds are
excluded.

| Family | Executable versions | Library |
| --- | --- | --- |
| SE | 1.5.3, 1.5.16, 1.5.23, 1.5.39, 1.5.50, 1.5.53, 1.5.62, 1.5.73, 1.5.80, 1.5.97 | version-1-5-*-0.bin, V1 |
| AE | 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629, 1.6.640, 1.6.659, 1.6.1130, 1.6.1170, 1.6.1179 | versionlib-1-6-*-0.bin, V2 |
| AE | 1.7.99, 1.7.104 | versionlib-1-7-*-0.bin, V5 |

GOG 1.6.659 and 1.6.1179 use the AE adapter. Library names and file-version
components must match exactly. A suffixed backup such as
versionlib-1-6-1170-0-1.bin is not substituted for the canonical filename.

## Runtime-specific DLLs

The 1.5.x, 1.6.x and 1.7.x packages contain separately compiled CGE.dll files.
The compiled family limits runtime selection, native graphics adapters and
static SKSE compatibility metadata. Query and Load both reject another family
before rendering hooks are installed. The effects and INI format remain shared.
Install one package matching SkyrimSE.exe, regardless of the store's SE/AE label.

The 1.6.x build retains separate depth layouts before and after 1.6.1130. The
1.7.x build uses its own world-render call offset. GOG 1.6.659 and 1.6.1179 belong
to the 1.6.x package. Unknown builds and VR remain excluded.

The internal multi-runtime build is retained for regression testing. It is not
one of the three runtime-specific download packages. Separate compilation does
not establish in-game validation of every executable or native shader archive.

## Requirements

Use the SKSE64 release built for the executable. CGE accepts the stable SKSE 2
plugin interface; it does not require SKSE 2.3.1 on every runtime. A mismatched
SKSE/game pair remains incompatible.

SKSE Menu Framework 3.14 or newer and its listed requirements provide the menu
and optional FPS HUD. Effects can use CGE.ini without a fallback private menu.
CommonLibSSE-NG is linked statically; players need no separate CommonLib DLL.
CGE retains its own V1/V2/V5 Address Library reader for the listed formats.

Color grading, LUTs, motion blur and lens distortion require single-sampled,
8-bit RGBA/BGRA or 10-bit RGB SDR output in G22 P709. HDR/scRGB and wide-gamut
output spaces are bypassed. Successful SetColorSpace1 changes are tracked by
the swapchain wrapper.
Motion blur also needs native TAA at native render resolution. These conditions
are independent of whether the executable is recognized.

## Native rendering

The interface detour copies a complete safe prefix of at least five bytes into
a nearby relay. Short prologues followed by RIP-relative loads can be supported
without moving those loads. Conflicting branches and unsupported prefixes are
rejected. HDR and opaque targets accept checked native entries with RIP-relative loads
and SSE initialization. These functions execute in place; their entry bytes
are not moved. Redirected entries and mismatched call targets remain rejected.

The camera is obtained through a typed CommonLib native delegate resolved by
Address Library. It no longer depends on one compiler-specific accessor byte
sequence. Null world roots, invalid projections and changing camera pointers
are still rejected. Renderer target offsets, strides and view fields are checked
at compile time against CommonLib types. Version-specific depth layout and
call sites remain explicit in the SE, early AE, AE 1.6.1130+ and AE 1.7 adapters.

SSAO has separate main-world opaque-pass hooks. It reads the native camera and
depth prepass, then binds occlusion for verified material shaders. Bindings end
before blended decals and water. It does not reuse the post-lighting HDR pass.
Invalid projections, unknown shader signatures and hook conflicts bypass the
corresponding processing. CGE does not apply scene effects over the HUD at Present.

CommonLibSSE-NG supplies the SKSE bridge and typed native delegates. Its presence
alone does not validate every manually selected graphics layout or call site.

## Versioned hook checks

The world-render call changes at 1.7.99, separately from the depth-buffer layout
change at 1.6.1130. The adapters use these call offsets relative to their
Address Library function entry:

| Call | SE 1.5 | AE 1.6 | AE 1.7.99+ |
| --- | --- | --- | --- |
| Main world | 0x831 | 0x841 | 0x85E |
| First opaque batch | 0x8E | 0x84 | 0x84 |
| Blended decals boundary | 0x319 | 0x308 | 0x308 |
| Tone mapping | 0x1EA and 0x230 | 0x178 | 0x178 |

For AE tone mapping, the Address Library target must match the original native
call. Equivalent argument setup is accepted without depending on one instruction
order. The second argument is preserved at full pointer width. SE retains its
verified argument-context check. External targets and native-target mismatches
are rejected; no search for an arbitrary replacement call is performed.

The hook locations were compared with Community Shaders at
[5db085e](https://github.com/community-shaders/skyrim-community-shaders/blob/5db085e77951b84bd6c191ff5b06d56893f08286/src/Deferred.h)
and its [version selector](https://github.com/community-shaders/skyrim-community-shaders/blob/5db085e77951b84bd6c191ff5b06d56893f08286/src/Utils/VersionedRelocation.h).
The renderer layouts agree with its pinned CommonLib reference: main depth SRV
at 0x2040 before 1.6.1130 and 0x20A0 from 1.6.1130, render targets at 0xA58,
0x30-byte entries and the SRV field at 0x18. Native TAA and camera-frustum accesses
also agree for the listed flat-game adapters. Temporal AO additionally reads
the validated NiCamera projection at 0x110 and world position at 0xA0. Unsupported
camera matrices bypass temporal accumulation while retaining spatial SSAO.

The projects do not use identical CommonLib revisions or rendering pipelines.
CGE retains its pinned CommonLib bridge and own V1/V2/V5 reader. Community
Shaders has adapters for its replacement shaders, full-screen blur chain and
additional depth copies. CGE does not install those shader replacements or
produce those same copies, so those adaptations cannot be transferred wholesale.
Matching selected addresses does not establish equivalent compatibility in game.

## Validation

The 70-test automated suite exercises production SKSE loading, hooks, profiles,
framework controls, GPU output, resource lifetimes and HUD preservation across
23 runtime fixtures. SE fixtures cover both tone-mapping call sites. A guarded
legacy SKSE fixture has no optional interface tail and uses messaging V1; another
covers short prologues with RIP-relative instructions. Further fixtures exercise
RIP-relative native HDR/opaque entries and an equivalent camera accessor using
a different register. GPU tests check 10-bit SDR exposure, alpha, color-space
transitions and unchanged HDR/wide-gamut bypass. HDR fixtures also cover
reordered arguments, pointer-width argument preservation and a call redirected
to a different native function.

The 24 supplied Address Library files were parsed and checked for every required
identifier. The three SSAO call sites were inspected in the installed executable.
These are fixture and binary checks, not in-game validation of all listed builds.
Working CGE rendering has been reported on Steam 1.7.104. Startup and missing-effect
problems have been reported on 1.6.1170; compatibility there is not confirmed.

## Troubleshooting

Back up CGE.ini before updating. Launch through the matching SKSE64 and inspect
CGE.log beside CGE.dll. It reports the adapter, library, startup stages and effect
status. Output rejection includes DXGI format, dimensions, samples, mip count
and array size. HDR target rejection distinguishes external modules, unreadable
entries and unsupported instruction bytes.

CGE-loader.log records DLL process attachment before SKSE Load. Neither log can
record a failure that prevents DLL initialization. With MO2, check the mod's
SKSE/Plugins directory and Overwrite. Keep skse64.log and a Crash Logger report
when investigating a startup crash. A successful fixture is not proof that a
reported real-game failure has been resolved.

## Motion TAA observation

Motion blur uses the TAA state captured once at the first main-world opaque
batch, provided it is consumed on the same thread within 100 ms. The interface
state is the fallback when that observation is unavailable. A temporary TAA
disable for UI rendering cannot override an enabled world observation; genuinely
disabled world TAA still bypasses the effect. No camera-only blur is substituted.

A 1.6.1170 / SKSE 2.2.6 native fixture checks rendered pixels with temporary UI
TAA disable and genuine world TAA disable. In-game confirmation remains pending.
The log identifies the observation source and unavailable inputs; unsupported
buffers include format, size, mip, array and sample information.

## Native entry relocation

The interface trampoline relocates supported RIP-relative loads, stores and
address calculations instead of rejecting every such entry. Displacements are
recomputed against the relay, with signed range validation. References into the
patched prefix, branch entries and incomplete or unrecognized instructions are
rejected without changing the function. This is a bounded decoder, not a general
x64 disassembler. Rejections include the entry bytes in the log.

Comparison reference: Community Shaders revision
`5db085e77951b84bd6c191ff5b06d56893f08286`, including its SE/AE/1.7 relocation
selection and native frame boundaries. CGE retains its own adapters and shaders.
Community Shaders uses the CommonLibVR fork; CGE currently uses its pinned
CommonLibSSE-NG bridge and its checked V1/V2/V5 Address Library reader. These are
not identical implementations, and neither dependency alone verifies every game
binary. Runtime fixtures use synthetic entries; confirmation on real older
executables is still required.

## Native keyboard toggle

The optional keyboard observer uses CommonLib input event types at the native
dispatch call in IDs 67315 (SE) / 68617 (AE), offset 0x7B. The call and its native
target are checked before patching. Original dispatcher and event-list arguments
are always forwarded unchanged. Keyboard button events are observed without
swallowing input, installing a window procedure or polling Windows key state.

The adapter shares the SE/AE runtime selection and Address Library reader used
by rendering. Native fixture tests cover the supported generations, remapping,
disabling, held/repeated keys, text/mouse events, paused/framework input, saving
and hook conflicts. Real-executable compatibility still requires in-game tests.
