# Runtime compatibility

CGE 0.7.1 includes the following SE/AE adapters. Each executable requires its
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

## Requirements

Use the SKSE64 release built for the executable. CGE accepts the stable SKSE 2
plugin interface; it does not require SKSE 2.3.1 on every runtime. A mismatched
SKSE/game pair remains incompatible.

SKSE Menu Framework 3.14 or newer and its listed requirements provide the menu
and optional FPS HUD. Effects can use CGE.ini without a fallback private menu.
CommonLibSSE-NG is linked statically; players need no separate CommonLib DLL.
CGE retains its own V1/V2/V5 Address Library reader for the listed formats.

Color grading, LUTs, motion blur and lens distortion require single-sampled,
8-bit RGBA/BGRA SDR output. HDR/10-bit output buffers are not supported.
Motion blur also needs native TAA at native render resolution. These conditions
are independent of whether the executable is recognized.

## Native rendering

The interface detour copies a complete safe prefix of at least five bytes into
a nearby relay. Short prologues followed by RIP-relative loads can be supported
without moving those loads. Conflicting branches and unsupported prefixes are
rejected. HDR target validation checks the entry instruction; the native target
is called in place rather than copied.

SSAO has separate main-world opaque-pass hooks. It reads the native camera and
depth prepass, then binds occlusion for verified material shaders. Bindings end
before blended decals and water. It does not reuse the post-lighting HDR pass.
Invalid projections, unknown shader signatures and hook conflicts bypass the
corresponding processing. CGE does not apply scene effects over the HUD at Present.

CommonLibSSE-NG supplies the SKSE bridge and typed native delegates. Its presence
alone does not validate every manually selected graphics layout or call site.

## Validation

The 63-test automated suite exercises production SKSE loading, hooks, profiles,
framework controls, GPU output, resource lifetimes and HUD preservation across
23 runtime fixtures. SE fixtures cover both tone-mapping call sites. A guarded
legacy SKSE fixture has no optional interface tail and uses messaging V1; another
covers short prologues with RIP-relative instructions.

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
