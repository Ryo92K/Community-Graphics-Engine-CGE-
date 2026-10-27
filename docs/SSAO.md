# SSAO

CGE 0.8.0 calculates horizon-based screen-space occlusion from the current
native world camera and the depth prepass. Validated material shaders apply the
result to directional ambient lighting while drawing opaque geometry. The old
post-lighting HDR multiplier has been removed.

## Controls

Open Effects / SSAO, enable SSAO for the selected profile and save.
Strength blends from no occlusion at 0 to full computed occlusion at 1.
Radius sets the search distance in game units. Bias rejects near-tangent
samples; a larger Bias reduces occlusion rather than increasing its strength.
Radius controls spread and is not an intensity setting. Power shapes the
visibility response. Max Distance fades the effect
out over the final 20 percent of its range.

Balanced computes occlusion at half resolution. High and Ultra use full
resolution with more directions and steps. All modes resolve to a full-size
occlusion texture before material rendering. The base configuration keeps SSAO
disabled; visual presets enable Balanced quality with style-specific strengths.
The previous Light Protection setting is retained for INI compatibility but
is no longer used or exposed: lighting separation replaces the brightness
heuristic.

## Native lighting integration

Three verified call sites delimit the main-world opaque pass. CGE computes
visibility after the native depth prepass and ends its bindings before blended
decals and water. Reflection and first-person passes outside this scope remain
unchanged. The CommonLibSSE-NG bridge and versioned Address Library adapters
remain required. A missing identifier, unexpected instruction or hook conflict
bypasses this backend without replacing it with an HDR dimming pass.

The device shader-creation hook recognizes the directional ambient matrix in
loaded Shader Model 5.0 pixel shaders. It requires all three matching matrix
rows, a shared normal operand and a valid position signature. CGE adds its own
occlusion lookup and scales those three ambient results. Native direct lights,
material sampling, emission, alpha and other render-target outputs remain in
the original shader. Unknown signatures use the original bytecode.

The transformed shader has an explicit control buffer at b13 and reads the
occlusion texture at t126 only while enabled. A zero or unbound control buffer
leaves ambient lighting unchanged and skips the texture lookup. CGE reserves
these slots only for the opaque pass, checks for occupied bindings and releases
its bindings afterwards. No context shader-binding hook, shader source dump,
external graphics engine or redistributed game shader is required.

The container reader validates sizes, instruction operands, input declarations,
resource slots and the DXBC checksum. It rebuilds the checksum after patching.
The installed local archive contained 6,924 matching shader programs. All were
transformed successfully; all 6,924 variants were accepted by the WARP driver.
These counts describe the inspected archive, not every Skyrim distribution.

## Occlusion and filtering

View-space depth and geometric normals are reconstructed on the GPU. Normal
derivatives select the closest depth continuation at silhouettes. Narrow depth
minima use a balanced derivative when both neighboring samples belong to a
similar, more distant surface, avoiding a tilted normal on a one-pixel object.

The search distributes angular slices around the viewing direction and projects
them into the depth image. Each slice integrates the cosine-weighted blocked
solid angle up to its observed horizon. A first pixel-scale tap retains nearby
contacts when the world-space search radius is large. Distance falloff, tangent
bias and reconstructed-depth precision reject unrelated or coplanar samples.

Two bilateral passes use a squared depth-separation weight with normal
agreement. Shallow object tops retain their own lighting instead of receiving
the ground contact through the filter. Strength remains a final blend; a larger
radius changes the search extent rather than multiplying the scene brightness.
The same shader and filtering changes are compiled into all three runtime DLLs.
Directions that find no valid obstruction skip the horizon preparation and
angular integral. Coplanar samples are rejected before computing their length.
These execution changes retain the same rays, samples, radius, resolution,
filtering and temporal settings.
Balanced uses nearest-surface downsampling and a guided full-resolution resolve.
Sky and invalid depth produce zero occlusion; screen-edge fading avoids inventing
off-screen occluders. Spatial fallback uses a deterministic sample pattern.

All intermediate resources remain on the GPU. The calculation uses an isolated
D3D11 context state and restores the native state. It does not copy or modify the
HDR scene. Buffers are reused and resized when depth dimensions or quality change.
The search reads a separate 32-bit linear-depth target. Balanced caches its
nearest-surface guides; High and Ultra fuse the vertical filter and final
conversion while retaining half-float rounding. These changes keep the existing
sample budgets and quality resolutions. See PERFORMANCE.md for measurements.

## Temporal stabilization

Temporal stabilization is automatic when the native camera supplies a validated
perspective basis and position. It works without native TAA or Motion Blur.
Current-frame object motion vectors are not used: this AO pass runs before
opaque geometry has produced them.

The resolver projects current view-space positions into the previous camera.
History samples must agree in linear depth and geometric normal. Bilinear
weights exclude rejected taps. A current 3x3 neighborhood bounds history, and
large occlusion changes reduce its weight to limit trails. Sampling rotates
between frames while camera and effect parameters remain consistent.

The first valid frame seeds history. Camera cuts, large translations, projection
or AO-setting changes, resize, invalid inputs and skipped world frames drop it.
SKSE load/new-game messages, environment transitions, long frame gaps and large
game-time jumps also reset history. Missing or unrecognized camera data and
failed temporal allocations retain the spatial path instead of reusing stale AO.

A full-resolution pixel pass resolves history. Balanced and High can prepare
its neighborhood bounds in a shared-memory compute pass, reusing the existing
R32 scratch texture after the depth search. The two bounds come from R16 AO
values and are packed without reducing their precision. Ultra retains the
original pixel path; unavailable UAV support also uses that path. Geometry and
AO targets are exchanged rather than copied or read back to the CPU. Cached temporal
targets add 20 bytes per depth pixel, about 40 MiB at 1920x1080 and 158 MiB at
3840x2160. Disabling SSAO releases them.

CGE.log reports camera reprojection availability or spatial fallback. A temporal
GPU fixture checks lower sampling variance, camera translation and rotation,
disocclusion, all three qualities, settings, projection changes and resize.
These synthetic checks do not replace testing animated scenes in game. Camera
reprojection and rejection cannot perfectly track independently moving surfaces
that retain similar depth and normals.

## Limits and testing

The native backend has passed synthetic GPU tests and executable-call-site
inspection, and working SSAO has been reported on the development installation.
Visual quality and performance still depend on the scene and configuration.
SE/AE adapters have automated coverage; this does not mean every real executable
has been tested. VR is not supported.

This implementation occludes directional ambient diffuse lighting. It does not
add a separate specular-occlusion or indirect-light transport system. Geometric
normals come from depth rather than material normal maps. Off-screen geometry,
transparent surfaces and a missing or incomplete depth prepass are not rebuilt.
It is not global illumination and does not establish parity with another engine.

Native TAA is optional for SSAO. Disable Skyrim's own SSAO when comparing results
to avoid stacking two AO implementations. Compatibility with external graphics
engines, custom material replacements and reserved-slot users is not established.
The temporal-status log is written on state changes rather than every frame.
The log reports native shader counts, bypasses, creation failures, opaque passes
and binding conflicts. Unsupported shaders retain native lighting.

Tests cover native ambient attenuation, unchanged direct lighting and emission,
alpha, sky, secondary targets, inactive controls, context restoration, all three
quality modes, contacts, flat and sloping planes, silhouettes, world radius,
projection changes, invalid depth, resizes, ownership and corrupt bytecode.
DXBC checksum tests exercise both padding paths with 128 private-data sizes.
Native call fixtures verify delegated arguments across the SE/AE adapters.

## Technical references

- [Ground-truth ambient occlusion integration reference](https://github.com/GameTechDev/XeGTAO)
- [Native camera layout](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/N/NiCamera.h)
- [Shader instruction format](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/Support/d3d12TokenizedProgramFormat.hpp)
- [Opaque-pass boundaries](https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Deferred.h)
- [Native lighting reference](https://github.com/aers/Skyrim-SE-Shader-Tools/blob/master/old/shaders/Lighting/BSLightingShader.ps.hlsl)

CGE provides its own bytecode transformation, occlusion shaders and filtering
pipeline. No external AO shader implementation or game bytecode is bundled.
