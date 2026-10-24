# Roadmap

These are development priorities, not release dates or confirmed compatibility claims.

## 0.1alpha: record the first trial and extend coverage

Initial user feedback on 2026-10-24 reports no in-game issues. Continue collecting
specific package versions and test cases while developing the native effects.

- Install through MO2 and Vortex and confirm SKSE reaches the graphics hook.
- Test startup, loading saves, indoors/outdoors, alt-tab, display modes and resizing.
- Check the menu, input release, settings persistence and clean uninstallation.
- Confirm migration from the private root injector and preserve useful failure logs.
- Measure disabled, neutral and enabled settings against a baseline on the same scene.
- Record detailed test coverage in VALIDATION.md; keep the initial positive user report separate from measured compatibility results.

## 0.1.x: diagnostics and fixes as needed

Improve diagnostics and menu usability, and resolve crashes or resource lifetime
problems alongside effect development. Add targeted regression tests for confirmed bugs.
Review the release package, source history and contributor notices before publishing.

## 0.2alpha: expand CGE-native post-processing

Define an effect interface and settings format for CGE's own effects.
Add color grading/LUT support and effect ordering with explicit GPU cost controls.
Keep defaults neutral and support per-effect bypass. External preset conversion
is outside the current scope.

## Later: scene resources and game integration

Capture and validate scene depth and, where available, scene HDR resources at
appropriate points in rendering. Final-output SDR processing alone cannot provide
a full HDR lighting pipeline.

Use those inputs for separately tested depth or lighting effects. Automatic time,
weather and interior information needs a game-data integration layer. Address
Library may become useful if future features access internal game structures;
the current build does not require it.

## Compatibility and performance

Expand runtime support only after each executable is tested. Add GPU timing,
quality controls and a documented compatibility matrix. Investigate transparency
issues with reproducible cases; do not advertise fixes or FPS gains in advance.
