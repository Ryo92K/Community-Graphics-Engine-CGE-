# SSAO (beta)

This feature is experimental and disabled by default. In-game testing has shown
broad darkening beyond contact areas. Light Protection reduces the effect but
does not solve the underlying lighting integration limitation. Disable SSAO if
it produces a dark or gray veil. This is not the final native lighting backend.

CGE provides an independent horizon-based screen-space ambient occlusion pass.
It operates on the native HDR scene before fog, depth of field, bloom and tone
mapping, using the current world camera and native depth. It adds contact
shadows around visible intersections without changing the HUD.

## Controls

Enable SSAO on Effects / SSAO (BÊTA), choose the active profile and press Save.
Existing settings and presets leave it disabled until explicitly enabled.

| Control | Range | Default | Purpose |
| --- | --- | --- | --- |
| Quality | Balanced, High, Ultra | High | Resolution and horizon samples |
| Light Protection | 0 to 1 | 1 | Reduce occlusion in bright HDR lighting |
| Strength | 0 to 1 | 0.65 | Blend between the original scene and occlusion |
| Radius (units) | 1 to 250 | 60 | World-space reach around a surface |
| Bias | 0.01 to 0.3 | 0.05 | Reject shallow angles that can cause self-shadowing |
| Power | 1 to 4 | 1.5 | Contrast of the occlusion response |
| Max Distance (units) | 500 to 50000 | 8000 | Fade out distant occlusion |

Balanced uses half-resolution occlusion, eight directions and four steps.
High uses full resolution, twelve directions and six steps. Ultra uses full
resolution, sixteen directions and eight steps. The world radius is projected
through the current frustum and capped at 128 screen pixels for bounded work.
Strength zero and either enable switch bypass the passes and release targets.

## Filtering

A full-resolution geometry pass reconstructs view-space depth and normals.
One-sided derivatives select the closest depth continuation to avoid normals
crossing silhouettes. Each direction tracks the highest surface elevation with
world-distance falloff. Samples less than two percent of the radius above the
receiver tangent plane are rejected to reduce small self-occlusion. Sampling uses a fixed spatial pattern, without animated
noise or a frame-history dependency.

Two separable bilateral passes use depth and normal agreement to smooth the
visibility field. Balanced selects the nearest valid surface in each 2x2 depth
footprint and uses depth/normal-guided upsampling at composite time. Invalid
samples and sky remain unchanged. Distance fades over the last 20 percent of
the range; screen borders fade rather than inventing off-screen occluders.

All resources stay on the GPU. CGE uses an isolated D3D11 context state and
restores the native state after rendering. Resources are recreated after a
resolution, quality or HDR-format change. No new executable hook is introduced.

## Scope

This pass reconstructs geometric normals from depth, without material normal
maps or off-screen geometry. Transparent surfaces without their own depth use
the depth behind them. It shades the already-lit HDR scene. Light Protection reduces the result using
HDR luminance, fully protecting luminance 2 and above at its default strength.
A three-percent visibility dead zone rejects weak uniform occlusion; maximum
darkening is capped at 40 percent before Strength and Light Protection. These
controls reduce a global dark veil but do not isolate the ambient lighting
contribution. Dark materials under direct light can still receive occlusion.
It is not global illumination, a temporal GTAO implementation or a substitute
for ray tracing. Visual parity with any other graphics engine is not established.

Native TAA is optional. CGE adds no temporal accumulation or reprojection here.
Disable Skyrim's native SSAO when comparing to avoid stacking two AO passes.
Compatibility with external graphics engines or depth-altering injectors is
not established. Start with Balanced if GPU frame time is a concern; actual
GPU cost must be measured in game on the target hardware.

## Validation

The WARP test renders flat and sloping planes, a foreground step and a
perpendicular wall-floor corner. It checks contact darkening, silhouette
preservation, sky, HDR hue and alpha, world-radius cutoff, near-plane invariance,
quality levels, deterministic frames, odd and one-pixel sizes, camera validation,
resource ownership, supported HDR formats, switches and context restoration.
Settings tests check every profile, bounds and non-finite inputs. Native plugin
fixtures exercise the menu, saves and actual rendering delegate across the
23 runtime adapters, including operation without TAA, fog or depth of field.
Synthetic tests do not establish visual quality, frame-time cost or successful
startup in every real game executable. In-game SSAO validation is pending.

## Technical references

- [Native camera and frustum layout](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/N/NiCamera.h)
- [Practical Real-Time Strategies for Accurate Indirect Occlusion](https://research.activision.com/publications/archives/atvi-tr-16-01practical-realtime-strategies-for-accurate-indirect-occlusion)

The shader and filtering implementation are CGE code. No external AO shader
or runtime library is bundled.
