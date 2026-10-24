# Dependencies and provenance

## Dear ImGui

- Version: **1.93.0**
- Commit: c1aaac65e896a45f81f47a4d68f9959e0f86c859
- Source: https://github.com/ocornut/imgui/tree/c1aaac65e896a45f81f47a4d68f9959e0f86c859

The 13 vendored upstream files were checked against that commit by SHA256 on 2026-10-24. Their hashes are recorded in third_party/imgui/UPSTREAM-MANIFEST.json.

CGE builds the core library and Direct3D 11 backend. The upstream source files are unchanged. PROVENANCE.txt and UPSTREAM-MANIFEST.json are project metadata.

The package includes the ImGui MIT license, the MIT licensing alternative for stb, font notices and the upstream Japanese glyph-data attribution. See THIRD-PARTY-NOTICES.txt and licenses/.

## SKSE interface

CGE uses minimal declarations for the public plugin interface documented in:

https://github.com/ianpatt/skse64/blob/25b72352adb6543fa6d0bd3795780672b2e238e0/skse64/PluginAPI.h

These cover the loader fields, query information and version metadata. No SKSE implementation or binary is included. Users install SKSE separately.

CGE does not use CommonLib, Address Library, game object layouts or game opcode signatures.

## Project code

CGE's code and shaders were developed with substantial code generation by an AI coding assistant. Generic input handling and DXGI forwarding code were retained from an earlier private prototype.

The current build does not include the prototype's external renderer SDK, preset parser, compiler adapter, assembly compatibility hooks, weather mappings or reference assets. The Windows import hook, menu input layer and native rendering pass are part of CGE.

This record documents the build inputs and their history. It is not a clean-room certification or legal clearance.
