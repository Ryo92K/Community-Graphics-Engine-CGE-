# Dependencies

## Runtime

- Skyrim SE/AE Steam 1.7.104.
- SKSE 2.3.1, installed separately.
- Address Library V5 for 1.7.104, installed separately.

CGE reads relocation ID 82084 from versionlib-1-7-104-0.bin to locate the
interface boundary. It checks the database version, address bounds and function
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
