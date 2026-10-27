# Rendering review: CGE 0.8.0

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

These SSAO measurements were taken for 0.7.1 and cover the spatial path.
The 0.8.0 temporal resolver adds a pass and cached history, so the figures below
are not timings for the complete temporal pipeline. Spatial quality budgets
remain unchanged.

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

## Temporal path optimization

Balanced and High reuse neighborhood samples through a shared-memory compute
pass. Groups load a 16x8 output tile with its border once. The pixel resolver
retains the original reprojection, rejection thresholds and final R16 render
target. The existing R32 linear-depth scratch texture holds packed neighborhood
bounds after depth search finishes; no additional full-screen image is allocated.
Ultra keeps the original temporal pixel path because the extra cache pass did
not show a repeatable benefit at 1080p. Missing UAV support retains the pixel path.
Quality resolutions, directions, sample counts, radius and temporal weights are
unchanged by these optimizations.

Temporal status is no longer re-reported on every Present. Frame cleanup now
preserves the reported state and previous game hour. This removes repeated
file opens/writes/closes and restores time-jump detection. A native test holds
SSAO steady for 16 frames and requires no additional temporal-status messages.

The fine-contact correction is a separate visual change: near-tangent height
rejection now follows depth precision instead of 2 percent of the chosen radius.
At radius 250, the former test rejected all contacts of five units or less.
Contact tests verify small occluders at large radius while flat and distant
sloping planes remain unoccluded. Performance comparisons use the same contact
correction in both reference and optimized paths.

A paired synthetic check on the Radeon RX 9070 XT at 1920x1080 measured
0.470 to 0.457 ms for Balanced and 1.364 to 1.258 ms for High. Ultra's original
path measured 1.880 versus 1.875 ms, effectively unchanged. These are medians
from five interleaved batches of 48 GPU-completed frames, including CPU
submission. They are not isolated GPU timestamps or in-game FPS predictions.

The final comparison checked 54 complete AO buffers per configuration on WARP
at 129x97 and hardware at 1280x720 and 1920x1080, including camera movement,
resets, changing occluders and settings. All 54 WARP buffers matched bit for bit.
The hardware runs had a maximum absolute AO difference of 0.000122071, within
R16 rounding precision; hardware output is not claimed bit-identical. A separate
reference/reference control matched exactly. These checks cover the comparison
scenes, not every possible GPU or image. The contact correction was enabled in
both paths so its intentional visual change is excluded from that comparison.

## Other rendering paths

Color, motion blur and lens distortion reuse intermediate textures. Lens
Distortion follows the other SDR processing without a redundant full-screen
copy. Output views are cached while their resource and format remain unchanged
and released before swap-chain resizing.

Context isolation restores native state after each effect. LUT status strings
are cached. Motion blur depth rejection accounts for sloping surfaces while
retaining foreground/background rejection. Presets preserve its global settings.

## Verification

111 automated tests pass, covering GPU effects, native ambient/direct/emissive
separation, settings, profiles, preset color retention, the FPS HUD, framework
controls, hook conflicts, resizing, resource ownership and repeated captures.
The 23 SE/AE runtime fixtures do not establish real-game compatibility for each
executable. Native motion resources and opaque SSAO hooks have been observed in a local
1.6.1170 session with native TAA enabled. Other configurations still need game
testing. VR is excluded.

## Weather profiles

Weather adjustments are computed on the CPU and reuse the existing color,
bloom and fog passes. No new texture, shader or draw call is introduced.
Sky weather is sampled at most ten times per second while the option is enabled
outdoors. Disabled weather profiles perform no native Sky sampling.
