# Rendering review: CGE 0.7.1

## SSAO pipeline

SSAO computes occlusion after the native depth prepass and applies it to
verified directional ambient calculations during opaque material rendering.
It no longer copies or multiplies the already-lit HDR image.

The horizon search reads a dedicated R32 linear-depth target instead of the
16-byte geometry target. Depth and normal precision, quality resolutions,
direction counts and sample budgets are unchanged. Repeated sample coordinates
and samples outside the world-space depth radius are skipped.

Balanced writes its nearest-surface guide alongside raw occlusion. Filtering
and upsampling reuse the guide instead of searching each 2x2 footprint again
for every tap. High and Ultra combine the vertical bilateral filter with final
occlusion conversion, reducing their pipeline from five passes to four.
Explicit half-float rounding preserves the former intermediate result.

Targets are reused until dimensions or quality change. The additional linear
depth and reduced guide consume more memory in exchange for fewer texture reads.
No per-material replay, CPU readback or shader-binding interception is used.
Shader variants are transformed once at creation. An inactive control buffer
skips their occlusion lookup.

## Measurements

A synthetic 1920x1080 scene on an AMD Radeon RX 9070 XT produced the following
median time per frame from GPU-completed batches:

| Quality | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Balanced | 1.209 ms | 0.369 ms | 69.5% |
| High | 3.101 ms | 1.836 ms | 40.8% |
| Ultra | 5.237 ms | 3.057 ms | 41.6% |

Each of three paired runs takes the median of three batches of 32 SSAO frames.
Timing includes CPU submission and completion waiting; isolated GPU timestamps
were unavailable. These figures are not in-game FPS predictions. Scene content,
GPU load and hardware affect the result.

The before/after 1920x1080 and 1280x720 occlusion buffers were bit-identical in
all three quality modes. This checks the comparison scenes; it does not prove
identical output for every possible input or hardware implementation.

## Other rendering paths

Color, motion blur and lens distortion reuse intermediate textures. Lens
Distortion follows the other SDR processing without a redundant full-screen
copy. Output views are cached while their resource and format remain unchanged
and released before swap-chain resizing.

Context isolation restores native state after each effect. LUT status strings
are cached. Motion blur depth rejection accounts for sloping surfaces while
retaining foreground/background rejection. Presets preserve its global settings.

## Verification

63 automated tests pass, covering GPU effects, native ambient/direct/emissive
separation, settings, profiles, preset color retention, the FPS HUD, framework
controls, hook conflicts, resizing, resource ownership and repeated captures.
The 23 SE/AE runtime fixtures do not establish real-game compatibility for each
executable. The reported 1.6.1170 failures are not claimed resolved. VR is excluded.
