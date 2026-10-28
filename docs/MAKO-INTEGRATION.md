# MAKO integration

[Back to README](../README.md) · [HDR processing](HDR.md) · [Building and releases](BUILDING.md)

MAKO installs this fork in a private user-owned directory from an exact release tag and SHA-256 checksum. It sets `VKBASALT_CONFIG_RELOAD=1` and an explicit `VKBASALT_CONFIG_FILE`; [configuration](CONFIGURATION.md#live-changes-in-this-fork) describes the user-visible reload behaviour.

## Effect replacement

MAKO's anti-aliasing, sharpening, and curated shader catalog support live selection and ordered combinations, including Clarity and Levels Plus. Custom ReShade chains use the same replacement path.

CAS/DLS strength and DLS denoise update through per-swapchain-image uniform buffers. Structural changes prepare a complete candidate before draining the layer's graphics queue once and retiring the old graph. Missing files, invalid textures, shader compilation errors, and Vulkan construction failures preserve the previous graph and roll back candidate resources. A later configuration save retries the change.

Retiring old graphs bounds effect-owned image and pipeline memory. Shorter chains release unused intermediates. File polling uses a bounded interval; edits to shader resources require an explicit refresh.

## Synchronization and resource lifetime

Multi-pass ReShade effects synchronize colour-attachment writes and layout transitions before later sampling, including ping-pong target reuse. The dependency applies to SDR and HDR without adding an image, draw, or CPU wait. Stencil attachments are allocated only when a selected pass enables stencil.

Colour-only graphs neither reference game depth images nor rebuild commands when those images change. They allocate no depth fences and add no per-frame fence operation.

Depth-sampling graphs track per-image submission fences and wait before replacing descriptors, commands, or views. Unrelated depth changes leave them alone. Image, format, and view are tracked together through out-of-order binding, failed creation/binding, and destruction before binding. Depth callbacks never wait on an application queue without owning its synchronization.

A failed depth-graph replacement stops further shader submissions instead of reusing invalid commands. Vulkan core/KHR `BindImageMemory2` images remain untracked for depth sampling. These lifetime rules apply to both SDR and HDR; [shader compatibility limits](CONFIGURATION.md#reshade-fx-shaders) still apply.

## Swapchain storage usage

MAKO Renderer can write its spatial reconstruction straight into the images this layer exposes, which needs storage usage on the swapchain. The application-facing and intermediate images inherit the requested usage, storage included, and stay mutable-format with an sRGB sibling. Effect input and output views therefore declare only sampled and colour-attachment usage through `VkImageViewUsageCreateInfo`, so sRGB views stay valid. MAKO detects this behaviour through:

```c
VkBool32 makoSwapchainStorageUsageSupportedV1(VkDevice);
```

It returns `VK_TRUE` for a valid device. Older builds lack the entry point, and MAKO keeps its copy-based path with them.

## Presentation cost

MAKO Renderer's Frame Generation layer sits above vkBasalt, so effects run on every real and generated output. GPU cost therefore scales with the Frame Generation multiplier.

The presentation path limits CPU overhead:

- Toggle-key polling runs at most once every 50 ms, avoiding a display-server round trip on every present.
- Each ReShade effect keeps its uniform buffer mapped for its lifetime.
- Up to four wait stages and four output semaphores use local storage. Larger wait lists and multi-swapchain presents retain dynamically sized storage without changing shader execution, semaphore ordering, or downstream results.
