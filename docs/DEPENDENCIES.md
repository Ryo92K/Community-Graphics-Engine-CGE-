# Dependencies

## Runtime

- Skyrim SE/AE Steam 1.7.104.
- SKSE 2.3.1, installed separately.
- Address Library V5 for 1.7.104, installed separately.

CGE reads relocation ID 82084 from versionlib-1-7-104-0.bin to locate the
interface boundary and ID 403521 to locate the player singleton. It checks the database version, address bounds and function
prefix before installing the hook. Address Library is not bundled.

## Dear ImGui

- Version: 1.93.0
- Commit: c1aaac65e896a45f81f47a4d68f9959e0f86c859
- Source: https://github.com/ocornut/imgui/tree/c1aaac65e896a45f81f47a4d68f9959e0f86c859
- License: MIT

CGE statically links the core library and Direct3D 11 backend.
The 13 vendored upstream files are unmodified. Their hashes are recorded in
third_party/imgui/UPSTREAM-MANIFEST.json.

The package retains the Dear ImGui MIT license, the MIT licensing alternative
for stb, font notices and the upstream Japanese glyph-data attribution.
See THIRD-PARTY-NOTICES.txt and licenses/.

## SKSE interface

CGE declares the public loader fields, query information and version metadata
needed by its plugin interface. Reference:

https://github.com/ianpatt/skse64/blob/25b72352adb6543fa6d0bd3795780672b2e238e0/skse64/PluginAPI.h

No SKSE implementation or binary is included.

## Build dependencies

Windows x64, Visual Studio C++ tools, the Windows SDK and CMake 3.24 or later.
Python 3 is used by the distribution check.

Windows system runtime and Direct3D compiler DLLs are not redistributed.
CGE does not link CommonLib or load external renderer presets.

## Cell detection

The 1.7.104 adapter reads the player's parent-cell pointer at offset 0x60 and the
cell interior flag at offset 0x40. It checks the player form ID and copies bounded
fields with ReadProcessMemory, rechecking the pointers before accepting a sample.
The build declares exact runtime compatibility and does not claim independence
from game object layouts.

Layout and singleton references:
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/T/TESObjectREFR.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/T/TESObjectCELL.h
https://github.com/alandtse/CommonLibVR/blob/ng/src/RE/P/PlayerCharacter.cpp

## Game clock

The 1.7.104 clock adapter resolves Calendar singleton ID 400447. It reads
Calendar::gameHour at offset 0x20 and TESGlobal::value at offset 0x34 using
bounded copies and pointer consistency checks. Values outside 0..24 and
non-finite samples are rejected. No system clock is used.

References:
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/C/Calendar.h
https://github.com/alandtse/CommonLibVR/blob/ng/src/RE/C/Calendar.cpp
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/T/TESGlobal.h

## Native motion inputs

The optional motion adapter targets Steam 1.7.104 and resolves Renderer ID 411393,
ImageSpaceManager ID 414660 and UI ID 400327. Native motion target 7 is read from
Renderer + 0xBC0 (SRV), and main depth from Renderer + 0x20A0 (SRV). The TAA helper
is read at ImageSpaceManager + 0x1F0, with enabled state at helper + 0x18.
UI + 0x160 provides the pause count.

Pointers are copied with bounded reads. Only native d3d11.dll COM views are
queried; the renderer also checks device ownership, view formats and dimensions.
No reference mod files, ReShade runtime or third-party shader implementation are
bundled. Motion uses CGE's own shader and configuration.

ABI and native vector convention references:
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/Renderer.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/R/RenderTargetData.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/I/ImageSpaceManager.h
https://github.com/alandtse/CommonLibVR/blob/ng/include/RE/U/UI.h
https://github.com/community-shaders/skyrim-community-shaders/blob/main/package/Shaders/Common/MotionBlur.hlsli

## Inter menu font

Copyright 2020 The Inter Project Authors (https://github.com/rsms/inter).
The unmodified variable font is distributed under SIL Open Font License 1.1;
see licenses/Inter-OFL.txt. CGE renders its default regular face at 16px.

Source commit: 0b58fb370093f9a9f4ff785d94405710b79de67c
Source: https://github.com/google/fonts/tree/0b58fb370093f9a9f4ff785d94405710b79de67c/ofl/inter
Font SHA256: 29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031

Build/install copies assets/fonts/Inter.ttf to SKSE/Plugins/CGE/Fonts/Inter.ttf.
The menu uses the embedded Dear ImGui font if this optional file is missing.
No fonts are installed into Windows.
