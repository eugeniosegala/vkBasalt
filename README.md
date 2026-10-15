# vkBasalt

<p align="center">
  <img src="assets/mako-vkbasalt.webp" width="256" alt="MAKO shark splitting light through a prism into vkBasalt shader effects" />
</p>

A Vulkan post-processing layer for Linux, with sharpening, anti-aliasing, colour LUTs, and ReShade FX shaders. Works with native Vulkan games, DXVK, and vkd3d.

This is the [MAKO](https://github.com/eugeniosegala/MAKO) fork of [DadSchoorse/vkBasalt](https://github.com/DadSchoorse/vkBasalt), adding live effect changes, HDR processing, and verified 64-bit and 32-bit release bundles.

## Use with MAKO

MAKO installs and configures its own private copy. Choose effects in **MAKO Decky** or **MAKO Renderer Configuration**; no separate vkBasalt installation is needed.

## Standalone use

Install your distribution's vkBasalt package or [build this fork](docs/BUILDING.md). Distribution packages may not include this fork's MAKO features.

Enable it in Steam launch options:

```text
ENABLE_VKBASALT=1 %command%
```

Or launch a game from a terminal:

```bash
ENABLE_VKBASALT=1 yourgame
```

Copy the [example configuration](config/vkBasalt.conf) to `~/.config/vkBasalt/vkBasalt.conf` and choose your effects. **Home** toggles effects under X11. See [configuration](docs/CONFIGURATION.md) for per-game settings, Lutris, and custom shaders.

## Documentation

| Guide | Contents |
| --- | --- |
| [Configuration](docs/CONFIGURATION.md) | Effects, config paths, live reload, ReShade, and logging |
| [Building and releases](docs/BUILDING.md) | Dependencies, 32/64-bit builds, tests, and MAKO packaging |
| [MAKO integration](docs/MAKO-INTEGRATION.md) | Layer ordering, resource lifetime, and performance contracts |
| [HDR processing](docs/HDR.md) | Colour pipeline, private hooks, sharpening, and compact buffers |

## Credits and license

Original vkBasalt by **Georg Lehmann (DadSchoorse)** and contributors. The ReShade shader compiler is by [crosire and contributors](https://github.com/crosire/reshade).

See the [vkBasalt license](LICENSE) and [ReShade license](src/reshade/LICENSE.md).
