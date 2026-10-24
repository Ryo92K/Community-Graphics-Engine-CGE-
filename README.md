# Community Graphics Engine (CGE)

CGE is an SKSE plugin for adjusting Skyrim's exposure, contrast, saturation and sharpening from an in-game menu.

**0.1alpha is the version label for the first planned public release.** It keeps the existing implementation. An initial in-game trial was reported with no issues; broader compatibility testing remains limited.

## Requirements

- Skyrim SE/AE on Steam, version **1.7.104**
- **SKSE 2.3.1**

Other game versions are not supported by this build. Address Library and additional helper plugins are not required.

## Installation

1. Install CGE-0.1alpha-SKSE-MO2-Vortex.zip with MO2 or Vortex.
2. Enable the mod. Deploy it if you use Vortex.
3. Launch Skyrim through SKSE.
4. Press **F10** or **Shift+Enter** to open the menu.

The archive installs SKSE/Plugins/CGE.dll and SKSE/Plugins/CGE.ini under Data. Nothing needs to be copied beside SkyrimSE.exe.

If you used CGE 0.8.0, close the game, back up and remove its old root-level d3d11.dll first. Do not remove a DLL belonging to another mod. This alpha refuses to start alongside a non-system d3d11.dll.

## Controls

| Action | Shortcut |
| --- | --- |
| Open menu | F10 or Shift+Enter |
| Close menu | Escape |
| Save settings | F6 |
| Reset settings | F7 |

The menu also has Save, Reset and Close buttons. Default settings leave the image unchanged.

Settings are saved to Data/SKSE/Plugins/CGE.ini. The log is Data/SKSE/Plugins/CGE.log. With MO2, modified files may appear in Overwrite.

To uninstall, close the game and disable the mod in your manager.

## Current scope

CGE uses its own SDR effects and configuration. It does not load external presets.

Lighting, weather, HDR rendering, depth effects and ambient occlusion are not implemented. Compatibility with other graphics engines has not been tested. FPS gains and transparency fixes have not been established.

See [installation details](docs/INSTALLATION.txt), [test coverage](docs/VALIDATION.md) and the [roadmap](docs/ROADMAP.md).

## Building

You need Windows x64, Visual Studio with C++ tools, the Windows SDK, Python 3 and CMake 3.24 or later.

    cmake -S . -B build -A x64 -DBUILD_TESTING=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
    cmake --install build --config Release --prefix dist-skse

The mod is CGE.dll. A separate Windows relay DLL is built for regression tests only; it is not included in the mod.

The GitHub Actions workflow runs the build and tests on Windows. It has not yet been run on the remote repository.

## License and credits

CGE code is licensed under [MIT](LICENSE). Dear ImGui and its bundled components retain their own licenses; see [third-party notices](THIRD-PARTY-NOTICES.txt).

Development includes substantial AI-generated code and UI implementation. [Dependencies and provenance](docs/DEPENDENCIES.md) records the source history and upstream references.
