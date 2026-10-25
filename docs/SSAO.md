# SSAO

CGE 0.7.1 calculates horizon-based screen-space occlusion from the current
native world camera and the depth prepass. Validated material shaders apply the
result to directional ambient lighting while drawing opaque geometry. The old
post-lighting HDR multiplier has been removed.

## Controls

Open Effects / SSAO, enable SSAO for the selected profile and save.
Strength blends from no occlusion at 0 to full computed occlusion at 1.
Radius sets the search distance in game units. Bias rejects near-tangent
samples. Power shapes the visibility response. Max Distance fades the effect
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

View-space depth and geometric normals are reconstructed on the GPU. One-sided
normal derivatives select the closest depth continuation at silhouettes.
Directional horizon searches use a world-space radius, tangent rejection and
distance falloff. Two bilateral passes preserve depth and normal boundaries.
Balanced uses nearest-surface downsampling and a guided full-resolution resolve.
Sky and invalid depth produce zero occlusion; screen-edge fading avoids inventing
off-screen occluders. The pattern is deterministic, without temporal history.

All intermediate resources remain on the GPU. The calculation uses an isolated
D3D11 context state and restores the native state. It does not copy or modify the
HDR scene. Buffers are reused and resized when depth dimensions or quality change.
The search reads a separate 32-bit linear-depth target. Balanced caches its
nearest-surface guides; High and Ultra fuse the vertical filter and final
conversion while retaining half-float rounding. These changes keep the existing
sample budgets and quality resolutions. See PERFORMANCE.md for measurements.

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
The log reports native shader counts, bypasses, creation failures, opaque passes
and binding conflicts. Unsupported shaders retain native lighting.

Tests cover native ambient attenuation, unchanged direct lighting and emission,
alpha, sky, secondary targets, inactive controls, context restoration, all three
quality modes, contacts, flat and sloping planes, silhouettes, world radius,
projection changes, invalid depth, resizes, ownership and corrupt bytecode.
DXBC checksum tests exercise both padding paths with 128 private-data sizes.
Native call fixtures verify delegated arguments across the SE/AE adapters.

## Technical references

- [Native camera layout](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/N/NiCamera.h)
- [Shader instruction format](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/Support/d3d12TokenizedProgramFormat.hpp)
- [Opaque-pass boundaries](https://github.com/community-shaders/skyrim-community-shaders/blob/main/src/Deferred.h)
- [Native lighting reference](https://github.com/aers/Skyrim-SE-Shader-Tools/blob/master/old/shaders/Lighting/BSLightingShader.ps.hlsl)

CGE provides its own bytecode transformation, occlusion shaders and filtering
pipeline. No external AO shader implementation or game bytecode is bundled.
