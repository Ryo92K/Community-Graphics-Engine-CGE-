# Community Graphics Engine (CGE)



CGE is an open source SKSE plugin for graphics modifications



## Requirements



- Skyrim SE/AE on Steam, version **1.7.104**

- **SKSE 2.3.1**



Other game versions are not supported by this alpha.



## Installation



1. Install CGE with MO2 or Vortex.

2. Enable the mod. Deploy it if you use Vortex.

3. Launch Skyrim through SKSE.

4. Press **F10** or **Shift+Enter** to open the menu.



The archive contains SKSE/Plugins/CGE.dll and SKSE/Plugins/CGE.ini.


## Controls



| Action | Shortcut |

| --- | --- |

| Open menu | F10 or Shift+Enter |

| Close menu | Escape |

| Save settings | F6 |

| Reset settings | F7 |

| Toggle Effets | F8 |



The menu also has Save, Reset and Close buttons. Default settings leave the image unchanged.



Settings are saved to SKSE/Plugins/CGE.ini. The log is SKSE/Plugins/CGE.log.




## Current scope



CGE uses its own SDR effects and configuration. It does not load external presets.



Lighting, weather, HDR rendering, depth effects and ambient occlusion are not implemented. Compatibility with other graphics engines has not been tested. FPS gains have not been established.




## Building



You need Windows x64, Visual Studio with C++ tools, the Windows SDK, Python 3 and CMake 3.24 or later.



    cmake -S . -B build -A x64 -DBUILD_TESTING=ON

    cmake --build build --config Release

    ctest --test-dir build -C Release --output-on-failure

    cmake --install build --config Release --prefix dist-skse





## License and credits



CGE code is licensed under [MIT](LICENSE). Dear ImGui and its bundled components retain their own licenses; see [third-party notices](THIRD-PARTY-NOTICES.txt).

