# vkBasalt

This fork publishes immutable `mako-v*` releases containing verified 64-bit and 32-bit Linux Vulkan-layer libraries for MAKO. MAKO consumes those archives by exact release tag and SHA-256 checksum and installs them only in its private user-owned directory. Upstream development remains credited to DadSchoorse and the vkBasalt contributors.

MAKO launches this fork with `VKBASALT_CONFIG_RELOAD=1` and an explicit `VKBASALT_CONFIG_FILE`. The layer checks that file at a bounded interval and updates CAS/DLS strength and DLS denoise through per-swapchain-image uniform buffers. MAKO's managed anti-aliasing, sharpening, and curated shader catalog can switch live, including ordered combinations of the colour and finishing presets bundled by MAKO Decky. The catalog also recognizes MAKO's Clarity and Levels Plus effects. Selection changes drain the layer's graphics queue once before retiring the previous graph, which bounds effect-owned image and pipeline memory instead of retaining every visited combination for the lifetime of the swapchain. FXAA uses its default medium-dither preset and SMAA defaults to its documented medium threshold and search limits; explicit configuration-file values still override the SMAA defaults. Layer activation remains restart-bound. Custom ReShade effect selections, removal, order, paths, includes, and configuration-file options now use the same live graph replacement. A complete candidate is prepared before the previous graph is retired; missing files, invalid textures, compilation failures, and Vulkan construction failures retain the previous graph and are retried after a later configuration save. FX source/include/texture file edits alone are not watched: MAKO’s Refresh action advances `makoReloadGeneration` to rebuild the active chain, including an unchanged selection. Manual clients can advance that configuration option. Chain changes can briefly hitch during compilation and queue drain. Intermediate chain images are released when a shorter chain replaces a longer one.

The ReShade path allocates a stencil attachment only when a selected pass enables stencil. Effects that do not use it avoid that allocation; effects that require stencil retain the existing path. This is an allocation optimization, not a promise that every ReShade shader or depth-based effect works.

MAKO's Frame Generation layer sits above vkBasalt, so every generated and real output passes through this layer's present hook. The toggle key is therefore read at most once every 50 ms rather than through a display-server round trip on every present, and each ReShade effect keeps its uniform buffer mapped for its lifetime instead of mapping and unmapping it on every present. Effects still run on every output, so their GPU cost scales with the Frame Generation multiplier.

vkBasalt is a Vulkan post processing layer to enhance the visual graphics of games.

The built-in effects are:

- Contrast Adaptive Sharpening
- Denoised Luma Sharpening
- Fast Approximate Anti-Aliasing
- Enhanced Subpixel Morphological Anti-Aliasing
- 3D color LookUp Table

It is also possible to use Reshade Fx shaders.

## Disclaimer

This is one of my first projects ever, so expect it to have bugs. Use it at your own risk.

## Building from Source

### Dependencies

Before building, you will need:

- GCC >= 9
- X11 development files
- glslang
- SPIR-V Headers
- Vulkan Headers

### Building

**The examples below install system-wide for manual vkBasalt use; MAKO's packaged fork instead lives in MAKO's private directory. Installing it system-wide is unnecessary for MAKO. `--prefix=/usr` writes into a directory normally owned by the package manager. Omit that prefix for `/usr/local`, and make sure the dynamic linker can find the installed library.**

For a separate system-wide vkBasalt installation, prefer your distribution's package when available.

```
git clone https://github.com/eugeniosegala/vkBasalt.git
cd vkBasalt
```

#### 64bit

```
meson setup --buildtype=release --prefix=/usr builddir
ninja -C builddir install
```

#### 32bit

Make sure that `PKG_CONFIG_PATH=/usr/lib32/pkgconfig` and `--libdir=lib32` are correct for your distro and change them if needed. On Debian based distros you need to replace `lib32` with `lib/i386-linux-gnu`, for example.

```
ASFLAGS=--32 CFLAGS=-m32 CXXFLAGS=-m32 PKG_CONFIG_PATH=/usr/lib32/pkgconfig meson setup --prefix=/usr --buildtype=release --libdir=lib32 -Dwith_json=false builddir.32
ninja -C builddir.32 install
```

## MAKO Vulkan baseline and release validation

`vulkan-headers-revision.txt` pins Vulkan Headers to `v1.4.365`, aligned with MAKO Renderer's `engine/vulkan-headers-revision.txt`. The release packager fetches that immutable tag and passes its include directory explicitly to both architectures. Direct Meson builds require headers 1.4.365 or newer and can select them with `-Dvulkan_headers=/path/to/Vulkan-Headers/include`. The reviewed API declaration lives in `meson.build`; Meson generates all manifests and the runtime declaration from it. Header upgrades require a separate review of API support before changing that declaration.

Run `scripts/package-mako-release.sh mako-v0.3.2.10-N /tmp/vkBasalt-candidate.tar.xz` to compile and test both architectures without installing or publishing. The archive records the exact Vulkan header tag, resolved commit, and declared API in `share/doc/vkbasalt/SOURCE`. Tests cover Vulkan 1.4 forwarding and failed instance creation as well as live effects and resource rollback. A new `mako-v*` tag triggers `.github/workflows/release-mako.yml`, which runs that same tested packager and publishes an immutable attested archive. MAKO must verify the downloaded public asset before updating its dependency pin and generated Flatpak module.

## Packaging status

[Debian](https://tracker.debian.org/pkg/vkbasalt) `sudo apt install vkbasalt`

[Fedora](https://src.fedoraproject.org/rpms/vkBasalt) `sudo dnf install vkBasalt`

[Void Linux](https://github.com/void-linux/void-packages/blob/master/srcpkgs/vkBasalt/template) `sudo xbps-install vkBasalt`

## Usage

Enable the layer with the environment variable.

### Standard

When using the terminal or an application (.desktop) file, execute:

```ini
ENABLE_VKBASALT=1 yourgame
```

### Lutris

With Lutris, follow these steps below:

1. Right click on a game, and press `configure`.
2. Go to the `System options` tab and scroll down to `Environment variables`.
3. Press on `Add`, and add `ENABLE_VKBASALT` under `Key`, and add `1` under `Value`.

### Steam

With Steam, edit your launch options and add:

```ini
ENABLE_VKBASALT=1 %command%
```

## Configure

Settings like the CAS sharpening strength can be changed in the config file. The config file will be searched for in the following locations:

- a file set with the environment variable `VKBASALT_CONFIG_FILE=/path/to/vkBasalt.conf`
- `vkBasalt.conf` in the working directory of the game
- `$XDG_CONFIG_HOME/vkBasalt/vkBasalt.conf` or `~/.config/vkBasalt/vkBasalt.conf` if `XDG_CONFIG_HOME` is not set
- `$XDG_DATA_HOME/vkBasalt/vkBasalt.conf` or `~/.local/share/vkBasalt/vkBasalt.conf` if `XDG_DATA_HOME` is not set
- `/etc/vkBasalt.conf`
- `/etc/vkBasalt/vkBasalt.conf`
- `/usr/share/vkBasalt/vkBasalt.conf`

If you want to make changes for one game only, you can create a file named `vkBasalt.conf` in the working directory of the game and change the values there.

#### Reshade Fx shaders

To run reshade fx shaders e.g. shaders from the [reshade repo](https://github.com/crosire/reshade-shaders), you have to set `reshadeTexturePath` and `reshadeIncludePath` to the matching dirctories from the repo. To then use a specific shader you need to set a custom effect name to the shader path and then add that effect name to `effects` like every other effect.

```ini
effects = colorfulness:denoise

colorfulness = /home/user/reshade-shaders/Shaders/Colourfulness.fx
denoise = /home/user/reshade-shaders/Shaders/Denoise.fx
reshadeTexturePath = /home/user/reshade-shaders/Textures
reshadeIncludePath = /home/user/reshade-shaders/Shaders
```

#### Ingame Input

The [HOME key](https://en.wikipedia.org/wiki/Home_key) can be used to disable and re-enable the applied effects, the key can also be changed in the config file. This is based on X11 so it won't work on pure wayland. It **should** however at least not crash without X11.

#### Debug Output

The amount of debug output can be set with the `VKBASALT_LOG_LEVEL` env var, e.g. `VKBASALT_LOG_LEVEL=debug`. Possible values are: `trace, debug, info, warn, error, none`.

By default the logger outputs to stderr, a file as output location can be set with the `VKBASALT_LOG_FILE` env var, e.g. `VKBASALT_LOG_FILE="vkBasalt.log"`.

## FAQ

#### Why is it called vkBasalt?

It's a joke: vulkan post processing &#8594; after vulcan &#8594; basalt

#### Does vkBasalt work with dxvk and vkd3d?

Yes.

#### Will vkBasalt get me banned?

Maybe. To my knowledge this hasn't happened yet but don't blame me if your frog dies.

#### Will there be a openGl version?

No. I don't know anything about openGl and I don't want to either. Also openGl has no layer system like vulkan.

#### Is there a GUI?

The Vulkan layer has no built-in GUI. MAKO Decky and MAKO Renderer Configuration provide controls for MAKO's managed fork.

#### So is vkBasalt just a reshade port for linux?

Not really, most of the code was written from scratch. vkBasalt directly uses reshade source code for the shader compiler (thanks [@crosire](https://github.com/crosire)), but that's about it.

#### Does every reshade shader work?

No. Shaders that need multiple techniques do not work, and some stencil, blending, or depth-dependent effects may still fail. Stencil resources are now allocated only for passes that request them; that change does not remove stencil support or make unsupported ReShade features work.

#### You said that "depth buffer access isn't ready yet", what does this mean?

There is a wip version that you can enable with `depthCapture = on`. It will lead to many problems especially on non nvidia hardware. Also the selected depth buffer isn't always the one you would want.

#### Is there a way to change settings for reshade shaders?

There is some support for it [#46](https://github.com/DadSchoorse/vkBasalt/pull/46). One easy way so to simply edit the shader file.

DDS decoding accepts bounded legacy DXT1–5 and 8/16/24/32-bit raw textures, checks complete payloads including mipmaps/cubemap faces, and rejects unsupported DX10 headers. Decoded DDS storage is limited to 256 MiB, with dimensions no larger than 16384 per face. Candidate Vulkan construction checks failures and rolls back partial resources before returning to the existing graph.
