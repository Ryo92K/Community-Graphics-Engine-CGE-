# Community Graphics Engine (CGE)

CGE 0.5.0 is an open-source SKSE plugin providing color controls, native motion
blur, HDR bloom and depth-based distance fog.

## Requirements

- Skyrim SE/AE Steam **1.7.104**
- **SKSE 2.3.1**
- **Address Library for SKSE Plugins**
- **[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)**,
  version **3.14 or newer**, and its listed requirements

Other game versions are not supported by this alpha. Motion blur additionally
requires native TAA at native render resolution.

The settings UI belongs to SKSE Menu Framework. CGE does not bundle its runtime,
Dear ImGui, a font, a private menu renderer or an input capture layer.
Install the framework separately through MO2 or Vortex.

## Installation

1. Install Requirements and this mod with MO2 or Vortex.
2. Launch Skyrim through SKSE.
3. Open the framework's Mod Control Panel (**F1 by default**) and select
   **Community Graphics Engine**.

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
