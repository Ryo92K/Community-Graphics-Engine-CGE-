# Runtime compatibility

CGE 0.6.3 includes experimental SE/AE runtime adapters.
Each executable requires its matching SKSE64 and Address Library file.
Skyrim VR, classic Skyrim, Epic, Microsoft Store and unknown builds are excluded.

| Family | Executable versions | Library |
| --- | --- | --- |
| SE | 1.5.3, 1.5.16, 1.5.23, 1.5.39, 1.5.50, 1.5.53, 1.5.62, 1.5.73, 1.5.80, 1.5.97 | version-1-5-*-0.bin, V1 |
| AE | 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629, 1.6.640, 1.6.659, 1.6.1130, 1.6.1170, 1.6.1179 | versionlib-1-6-*-0.bin, V2 |
| AE | 1.7.99, 1.7.104 | versionlib-1-7-*-0.bin, V5 |

GOG 1.6.659 and 1.6.1179 use the AE adapter. File version components and names
must match exactly. A suffixed backup such as versionlib-1-6-1170-0-1.bin is not
automatically substituted for the canonical filename.

## SKSE and menu

Use the SKSE64 release built for the executable. CGE accepts the stable SKSE 2
plugin interface rather than requiring SKSE 2.3.1 for every game version.
This does not make mismatched game/SKSE pairs compatible.

For the settings UI, install SKSE Menu Framework 3.14 or newer and its listed
requirements in versions compatible with the executable. If the framework is
missing or incompatible, effects can use CGE.ini without a fallback private menu.

## Validation

The 51-test synthetic suite exercises the production plugin across all 23 listed
runtime descriptors, including SKSE loading, native hooks, profiles, framework
controls, effect output and HUD preservation. SE tests include both cinematic
tone-mapping call sites and a distinct instruction prologue. A guarded legacy
SKSE interface fixture has no optional interface tail and uses messaging V1.

A separate read-only check parsed the 24 provided Address Library files,
including both copies of the 1.6.1170 table, and found every required identifier.
These checks validate table decoding and test fixtures. They do not validate
the real hook instructions, layouts or rendering on all game executables.

Earlier CGE releases ran on Steam 1.7.104. This new compatibility build still
needs in-game testing, especially 1.5.97, 1.6.640, 1.6.1170 and GOG.
No other runtime has been claimed as in-game validated.

## Checking a test installation

Back up CGE.ini and replace only CGE.dll to preserve existing settings.
Launch through the matching SKSE64. CGE.log reports the selected runtime adapter,
SKSE version, exact library path and initialization status of each native hook.
Check color, Clarity, Vibrance, the four finishing effects, motion blur, bloom,
fog, depth of field, profile transitions,
the framework controls and HUD preservation before reporting a runtime as tested.

A missing library, unsupported instruction sequence or competing hook is logged
and its corresponding effects remain disabled. CGE does not fall back to
applying effects over the composed HUD at Present.
