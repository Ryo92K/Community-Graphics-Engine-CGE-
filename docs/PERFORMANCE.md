# Rendering review: CGE 0.7.0

## Optimizations with unchanged output

- Cache SDR and HDR output render-target views while their texture and format
  remain unchanged, rather than creating a view in every active pass each frame.
  Invalidate on resource changes and release cached swap-chain views before
  ResizeBuffers/ResizeBuffers1. Disabled passes release their cached output.
- When Lens Distortion follows color grading and/or motion blur, route the
  preceding pass to an existing intermediate texture. This removes the second
  full-screen CopyResource without changing pass order or UNORM quantization.
  The combination uses one initial copy rather than two. A single 1080p RGBA8
  copy carries 7.91 MiB; at 4K it carries 31.64 MiB, excluding read/write overhead.
- Upload the SSAO constant buffer only when its filtering axis changes. The
  same five passes need three updates instead of five. Samples, resolution,
  formats, radius and bilateral weights are unchanged by this optimization.
- Reuse cached LUT diagnostic text rather than rebuilding the active filename
  string each frame. Restore the correct status after disabling/re-enabling.

There is no reduction of sample counts, effect quality or render resolution.
The changes above were validated separately from the visual corrections below.
All 32 deterministic captures were byte-identical before and after optimization,
covering all eight SDR pass combinations and eight HDR chains, twice each.
They use WARP at 96x72. This validates those output paths; it is not an exhaustive
proof for all hardware and scenes or an in-game FPS benchmark.

## Motion Blur corrections

The available MO2 log contained earlier valid native vectors but the latest
sessions started with Motion Blur off. Visual preset application previously
reset that global option. Built-in presets now preserve its toggle, intensity,
radius and sample count. Manual INI replacement still replaces the entire INI.

The depth rejection now predicts the local planar depth gradient, so a sloping
surface is not treated as a silhouette. Opposite depth gradients at isolated
foreground pixels keep a zero slope, retaining foreground/background rejection.
Intensity extends from 2 to 4; new settings and Natural use 1 instead of 0.5.
Strong uses 2.5 instead of 1.5. Existing saved values are not overwritten.
Native TAA, native resolution and valid motion/depth buffers remain required.
No native vector decoding change was needed after reviewing the formats.

## SSAO corrections

Reject shallow world-space self-occlusion, suppress the weakest visibility
changes and cap darkening before strength. Light Protection uses HDR luminance
to preserve bright surfaces. It defaults to 1 and saves per profile. This is
an approximation because the current native hook runs after scene lighting;
CGE still cannot separate the ambient and direct lighting contributions.
These corrections deliberately change SSAO output and are not counted as
lossless optimizations. In-game screenshots show broad darkening beyond contact regions. The feature
remains beta; this issue is not claimed fixed.

## Verification

59 automated tests pass, including all 23 SE/AE runtime fixtures, sloping motion,
foreground rejection, stronger motion, preset persistence, SSAO highlights,
settings, HUD preservation, resize/reattachment, cache invalidation and repeated
frame captures. CommonLibSSE-NG remains linked and no new native hook is added.

The reported real 1.6.1170 startup crash remains unconfirmed. Skyrim VR is still
excluded. New in-game motion/SSAO behavior and FPS gains are not yet measured.
