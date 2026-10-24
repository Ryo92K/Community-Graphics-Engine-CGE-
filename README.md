# Community Graphics Engine (CGE)

CGE 0.5.0 is an open-source SKSE plugin providing color controls, native motion
blur, HDR bloom and depth-based distance fog. Effects run before the HUD and game menus are drawn.

## Requirements

- Skyrim SE/AE Steam **1.7.104**
- **SKSE 2.3.1**
- **Address Library for SKSE Plugins**, including versionlib-1-7-104-0.bin (V5)
- **[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)**,
  version **3.14 or newer**, and its listed requirements

Other game versions are not supported by this build. Motion blur additionally
requires native TAA at native render resolution.

The settings UI belongs to SKSE Menu Framework. CGE does not bundle its runtime,
Dear ImGui, a font, a private menu renderer or an input capture layer.
Install the framework separately through MO2 or Vortex.

## Installation

1. Close Skyrim and back up Data/SKSE/Plugins/CGE.ini.
2. Install SKSE Menu Framework and its listed requirements with MO2 or Vortex.
3. Replace the older CGE mod with the complete CGE archive, then restore your
   saved CGE.ini if you want to keep your settings.
4. Keep Address Library and the framework enabled, then launch Skyrim through SKSE.
5. Open the framework's Mod Control Panel (**F1 by default**) and select
   **Community Graphics Engine**.

No CGE files belong beside SkyrimSE.exe.
CGE's previous menu, font and keyboard/mouse hooks have been removed.
Its old menu, save, reset and comparison shortcuts no longer apply.
The framework's own settings control its opening key, theme, font and input.

Existing CGE.ini files remain readable. Obsolete [Input] shortcut entries are
ignored and omitted on the next save. Color, profile, timing, bloom and motion
settings are retained.

## Settings pages

| Category | Controls |
| --- | --- |
| General | Master effect switch, effect status and Reset All |
| Effects / Color | Exposure, contrast, saturation, sharpness, gamma and temperature |
| Effects / Bloom | Bloom switch, intensity, threshold and radius |
| Effects / Distance Fog | Fog switch, density and start distance |
| Effects / Motion Blur | Global motion settings, quality and Natural/Strong defaults |
| Profiles | Automatic mode, profile transitions, dawn and dusk |

**Effects enabled** bypasses all CGE effects when unchecked.
On **Profiles**, **Automatic profiles** switches between shared values and
location/time profiles. On its first activation, shared values seed all automatic
profiles.

On **Color**, **Bloom** and **Distance Fog**, **Edit Profile** selects Shared, Interior, Exterior Day
or Exterior Night. These pages keep the same selection. Editing a profile does
not force the game to use it. Automatic profile controls are disabled while
shared mode is selected.

Each slider has a numeric field and a Reset button. Labels appear above their
controls. **Save** writes all configured settings to CGE.ini for the next launch.
**Reset Color** resets only color controls, preserving bloom and fog.
**Reset Bloom** resets only bloom. **Reset Fog** resets only fog.
**Reset All**, on General, restores neutral color controls, shared mode and all optional
effects disabled. Save afterward to keep the reset.

The page reports unsaved changes and whether the save succeeded.

## Color controls

Exposure adjusts brightness in stops. Contrast and saturation are neutral at 1;
sharpness is neutral at 0. Gamma is neutral at 1. Color Temperature is a relative
adjustment: negative values cool the image and positive values warm it.

## Automatic profiles

Interior is independent of time. Outdoors, day and night blend over one game hour
around the configured dawn and dusk. Defaults are 06:00 and 20:00.
Decimal hours are accepted: 6.5 means 06:30.

Transition (sec) controls the real-time blend after a doorway, mode change or time
jump. It ranges from 0 to 5 seconds, with 1.5 seconds by default.
A missing player cell or clock retains the last valid profile state.

Older Exterior settings seed both Exterior Day and Exterior Night.

## Bloom

Enable **Bloom enabled** on the selected profile's page.

- **Bloom Intensity:** 0 to 2; default 0.15. Controls the strength of scattered light.
- **Bloom Threshold:** 0 to 1; default 1. Lower values include dimmer sources.
- **Bloom Radius:** 1 to 64; default 32. Spreads the halo over a larger area.

Bloom runs on linear HDR highlights before Skyrim's tone mapping. Each profile
has its own switch and values. The master Effects enabled checkbox also bypasses
bloom. Skyrim's built-in bloom remains unchanged.

If the verified HDR call is unavailable or already replaced by another plugin,
CGE skips bloom and records the reason in CGE.log.

## Distance fog

Open **Effects / Distance Fog**, select **Edit Profile** and check **Fog enabled**.

- **Density:** 0 to 1; default 0.35. Higher values make distant surfaces more opaque.
- **Start Distance (units):** 0 to 50000; default 2000. Surfaces closer than this
  distance keep their original appearance.

Distance is reconstructed from native scene depth and the current camera's
near/far planes. It is measured along the camera's view axis, in game units.
Fog builds up progressively beyond the start distance using a fixed cool-gray
tint. The current controls are On/Off, density and start distance.

Fog follows Shared, Interior, Exterior Day and Exterior Night profiles, including
their transitions. Save keeps the values across launches. The effect is disabled
by default, so existing 0.4 configurations keep their previous appearance.

The pass runs before bloom and native tone mapping. Clear-depth sky pixels are
left unchanged, and the HUD is drawn afterward. Native TAA and motion blur do
not need to be enabled. Scene depth and a valid perspective world camera are
required; missing or unsupported inputs skip the pass and are reported in CGE.log.

This is a screen-space depth effect. Transparent surfaces can follow the depth
of the surface behind them. It adds a distance haze to the game's existing fog;
it does not provide height fog or volumetric lighting.

## Motion blur

Motion blur is global and disabled by default. It uses native velocity and depth
buffers, not a previous-frame image blend.

- **Intensity:** 0 to 2; default 0.5.
- **Blur Radius (px):** 1 to 64; default 24.
- **Quality:** 8, 12 or 16 samples; default 12.

Natural selects intensity 0.5, radius 24 and 12 samples.
Strong selects intensity 1.5, radius 48 and 16 samples.

Motion blur pauses while the framework blocks the game, in paused game menus,
during loading and when native data is unavailable. Dynamic resolution,
replacement temporal pipelines and external upscalers are unsupported for this
effect. Some transparent objects may have incomplete motion data.

## Files and troubleshooting

Settings: Data/SKSE/Plugins/CGE.ini.
Log: Data/SKSE/Plugins/CGE.log.
With MO2, changed files may appear in Overwrite.

The log reports whether the six framework pages registered, the selected native
profile, effect status and time-limited motion measurements.
If the framework is missing or its required exports are unavailable, CGE logs
the reason. Effects can still use the INI; there is no fallback CGE menu.

Compatibility with other graphics engines is not established.
CGE does not load external presets or provide weather, ambient occlusion or
depth of field. A non-system d3d11.dll blocks initialization.
To uninstall, close Skyrim and disable CGE.

## Building

Requires Windows x64, Visual Studio C++ tools, the Windows SDK, CMake 3.24 or later
and Python 3 for the distribution check.

    cmake -S . -B build -A x64 -DBUILD_TESTING=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
    cmake --install build --config Release --prefix dist-skse

No framework DLL or headers are required to compile CGE.
The bridge resolves a checked subset of the public export ABI at runtime.
The framework fixture DLLs and relay are for tests only and are never installed.

Automated tests cover GPU color/motion/bloom output, HUD preservation, settings,
profile transitions, SKSE registration, delayed framework loading, controls,
numeric input, saves, reattachment, missing/old/incomplete APIs and hook conflicts.
The suite also covers fog depth reconstruction, sky preservation, density,
distance, native camera validation, TAA independence and per-profile saves.
Framework tests use a dedicated export host. Distance fog has also been confirmed
working in game on Skyrim 1.7.104.

## License and credits

CGE code is licensed under [MIT](LICENSE).
See [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt) for external component details.

Thanks to the SKSE, Address Library and SKSE Menu Framework contributors, and to
the Community Shaders and CommonLib maintainers for their technical references.

## Technical references

- [SKSE plugin ABI](https://github.com/ianpatt/skse64/blob/master/skse64/PluginAPI.h)
- [SMF consumer API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API)
- [SMF export implementation](https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/src/SKSEMenuFramework.cpp)
- [Address Library V5 reader reference](https://github.com/alandtse/CommonLibVR/blob/ng/include/REL/IDDB.h)
- [Interface and tone-mapping boundaries](https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Hooks.cpp)

CGE uses its own V5 reader, verified hooks, shaders and filtering pipeline.
