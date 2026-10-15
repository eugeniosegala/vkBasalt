# HDR processing

[Back to README](../README.md) · [MAKO integration](MAKO-INTEGRATION.md)

## Colour-space contract

HDR processing requires an explicit colour space; bit depth alone never enables it. Ordinary Vulkan callers retain the colour space from their swapchain creation structure.

MAKO's isolated Gamescope bridge supplies the original application's colour space through this private device-dispatch hook, after lower swapchain creation and before image enumeration:

```cpp
VkBool32 makoSetSwapchainColorSpaceV1(VkDevice, VkSwapchainKHR, VkColorSpaceKHR);
```

The hook rejects unknown swapchains, device mismatches, invalid HDR format pairs, and changes after private images exist.

## Effect working space

Active HDR10/PQ and linear scRGB chains normally use RGBA16F intermediates containing PQ BT.2020 code values. scRGB converts from 80-nit BT.709 units before the chain and back afterwards, requiring two conversion passes. HDR10 retains its transfer function.

HDR10 skips the input copy when CAS, DLS, or ReShade is first, and the output copy when CAS or DLS is last. Either sharpener alone runs in one pass between the original packed 10-bit images; multi-effect intermediates remain RGBA16F. Skipping the input copy also removes one image per swapchain slot and its half-float rounding. Pixels can change slightly when that rounding step disappears.

ReShade working, ping-pong, and output images retain the graph format; sampling uses the original input format. An empty chain copies the original images without colour conversion. Live replacement retains the colour-space contract and retires unused intermediates.

Managed and custom effects operate in this bounded display-code domain. Custom shaders may clamp, remap colours, or use lower-precision textures and require independent visual qualification.

## CAS and DLS sharpening

HDR uses a transfer correction within the existing sharpening pass. Contrast and highlight limits are evaluated in an sRGB-like range referenced to 203 nits, retaining BT.2020 primaries. A shared reversible RGB scale moves bright neighbourhoods to the filters' SDR midrange, with a local maximum of 0.5, and restores HDR brightness afterwards. Darker neighbourhoods retain their range; contrast and denoise guards remain active.

CAS converts shared neighbourhood extrema once and bypasses transfer arithmetic for constant HDR neighbourhoods. The approximation uses a cubic rational input curve, a quartic rational output curve, and an analytic dark output segment. Validation checks separate and contracted float32 arithmetic, monotonicity, adjacent values at both joins, a maximum 0.1 HDR10-code-value round-trip error, and a maximum 0.01-code-value inverse error:

```bash
python3 scripts/generate-hdr-sharpen-curve.py --check
```

This also checks generated-source freshness. The correction adds arithmetic without an extra pass, image, descriptor, or wider storage format. SDR selects its original shader path at pipeline creation; HDR10 and scRGB use the correction only after explicit HDR classification.

## Sharpening after display resize

Before image exposure, MAKO's internal HDR display resize can supply:

```cpp
VkBool32 makoSetSwapchainSharpenSourceExtentV1(VkDevice, VkSwapchainKHR, uint32_t width, uint32_t height);
```

CAS/DLS then sample one source pixel away instead of one output pixel away, preserving the detail footprint after enlargement. The hook requires the correct device, explicit HDR, matching aspect ratio, and source dimensions at least half the output dimensions. Invalid or late calls preserve prior state.

Graph replacement retains the swapchain-owned extent; replacement swapchains start with native sampling. SDR, native-size HDR, explicit MAKO Scaling, anti-aliasing, and custom shaders keep their existing sampling. Specialization constants and the same texture fetches implement the correction without an extra pass, image, uniform update, or synchronization.

## Experimental compact buffers

MAKO can request packed RGB10A2 PQ intermediates through:

```cpp
VkBool32 makoSetSwapchainHdrPrecisionV1(VkDevice, VkSwapchainKHR, VkBool32);
```

The hook validates swapchain/device ownership and Boolean input, and accepts requests before image exposure or later. Explicit HDR is required; SDR ignores the request. RGBA16F remains the default and the fallback when sampled, filtered, blendable colour-attachment support is unavailable. Format checks run outside steady rendering. scRGB retains its boundary conversions and original output format.

When the validated PQ working format exactly matches the HDR10 output, the final effect writes directly to it, removing one copy pass and one full-resolution intermediate image per swapchain slot. Multi-pass ReShade effects can use this path because working, ping-pong, and output formats match. Fallback formats, different packed channel order, and scRGB retain their required boundary pass.

Precision changes use the ordinary live graph transaction. Each graph owns its intermediates, allowing a complete candidate without reinterpreting images still used by the previous graph. Failed construction frees candidate resources; an uncertain final drain retains them until safe retirement or teardown. Identical requests neither allocate nor drain.

Compact storage halves intermediate bytes per pixel but reduces RGB and alpha precision and clamps overshoot. Blending and effect chains may change, and FPS gains require measurement. The constructed graph log's `working_format` records actual activation.
