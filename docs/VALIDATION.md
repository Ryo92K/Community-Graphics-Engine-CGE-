# Test coverage

**CGE 0.1alpha — 11 automated tests passed on 2026-10-24.**

The renamed 0.1alpha build was compiled and tested before packaging. The preceding private build also passed a separate build of the exported sources; that historical check does not establish in-game compatibility.

## Covered by the tests

- D3D11 forwarding and error handling
- Keyboard/mouse capture, shortcut handling and focus changes
- Settings validation, Unicode paths and save failures
- Production DLL exports/imports and pinned dependency hashes
- Simulated SKSE loading and interception of the executable's D3D11 creation import
- Exposure, contrast, saturation and sharpening output
- Alpha preservation, GPU state restoration and resizing
- Menu controls, saving, reloading and resetting in ANSI and Unicode windows
- Rejection of missing graphics imports and conflicting root injectors

The rendering tests use synthetic images and the Windows software rasterizer. The SKSE fixture passes the documented loader fields to SKSEPlugin_Load, then creates a D3D11 device through the executable's import table.

The production DLL exports SKSEPlugin_Query, SKSEPlugin_Load and SKSEPlugin_Version. Its metadata targets Skyrim Steam 1.7.104; other runtimes and the editor are rejected.

## In-game feedback

On 2026-10-24, the user reported no in-game problems with the current CGE implementation.
The specific package version and individual test cases were not recorded.
This is an initial positive trial, not a full hardware or mod compatibility matrix.

## Further coverage

These tests do not run SKSE itself or Skyrim. Broader coverage of startup timing, other mods, visual results and performance remains to be recorded. The remote GitHub workflow has not run yet.

For additional trials, install with MO2 or Vortex and launch through SKSE. These messages confirm renderer activation:

    CGE SKSE loaded; graphics creation hook ready
    CGE SKSE renderer attached
    CGE menu ready: F10

CGE refuses to initialize if a non-system d3d11.dll is already loaded. Remove the old CGE 0.8.0 root injector before upgrading.
