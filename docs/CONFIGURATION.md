# Configuration

[Back to README](../README.md)

MAKO users configure effects through MAKO Decky or MAKO Renderer Configuration. The settings below are for standalone use; MAKO manages its own configuration file.

## Launching

Use `ENABLE_VKBASALT=1 yourgame` in a terminal or `ENABLE_VKBASALT=1 %command%` in Steam launch options. In Lutris, open the game's **Configure > System options > Environment variables** and add `ENABLE_VKBASALT` with value `1`.

## Configuration file

vkBasalt uses the first readable file in this order:

1. The path set by `VKBASALT_CONFIG_FILE`.
2. `vkBasalt.conf` in the game's working directory.
3. `$XDG_CONFIG_HOME/vkBasalt/vkBasalt.conf`, or `~/.config/vkBasalt/vkBasalt.conf` when unset.
4. `$XDG_DATA_HOME/vkBasalt/vkBasalt.conf`, or `~/.local/share/vkBasalt/vkBasalt.conf` when unset.
5. `<sysconfdir>/vkBasalt.conf`.
6. `<sysconfdir>/vkBasalt/vkBasalt.conf`.
7. `<datadir>/vkBasalt/vkBasalt.conf`.

System paths follow the build's installation directories, commonly `/etc` and `/usr/share`. For one game, place a config in its working directory or set `VKBASALT_CONFIG_FILE=/absolute/path/to/vkBasalt.conf`.

Start from the [example configuration](../config/vkBasalt.conf), which documents effect options and defaults. Effects run left to right:

```ini
effects = fxaa:cas
casSharpness = 0.4
```

Built-in choices include CAS and DLS sharpening, FXAA and SMAA anti-aliasing, and colour LUTs. FXAA uses its medium-dither preset; SMAA defaults to medium thresholds and search limits. Explicit config values override the configurable defaults.

## Live changes in this fork

Set `VKBASALT_CONFIG_RELOAD=1` before launching to watch the selected configuration file. MAKO enables this automatically.

CAS/DLS strength and DLS denoise update live. Effect selection, removal, ordering, shader paths, includes, and shader options rebuild the effect chain. Failed builds keep the previous chain and retry after a later configuration save. Compilation and replacement can briefly hitch. Enabling the layer itself requires restarting the game.

Shader, include, and texture file edits are not watched directly. After editing those files, use MAKO's **Refresh** action or increment `makoReloadGeneration` in the config to rebuild an unchanged selection.

## ReShade FX shaders

Point `reshadeTexturePath` and `reshadeIncludePath` at your [ReShade shader collection](https://github.com/crosire/reshade-shaders). Give each shader an alias and add it to `effects`:

```ini
effects = colorfulness:denoise
colorfulness = /home/user/reshade-shaders/Shaders/Colourfulness.fx
denoise = /home/user/reshade-shaders/Shaders/Denoise.fx
reshadeTexturePath = /home/user/reshade-shaders/Textures
reshadeIncludePath = /home/user/reshade-shaders/Shaders
```

Shader settings have [partial configuration support](https://github.com/DadSchoorse/vkBasalt/pull/46); editing the shader source is another option. Refresh after source edits when using live reload.

Shaders requiring multiple techniques are unsupported. Some stencil, blending, or depth-dependent effects may fail. Experimental `depthCapture = on` can select the wrong depth buffer and is not reliable across games or drivers. Vulkan core/KHR `BindImageMemory2` depth capture is unsupported. Anti-cheat compatibility is not guaranteed.

DDS textures support legacy DXT1–5 and 8/16/24/32-bit raw formats. Complete mipmap and cubemap payloads are required; DX10 headers are unsupported. Decoded storage is limited to 256 MiB and dimensions to 16384 per face.

## Toggle and logging

**Home** toggles effects; change `toggleKey` in the config to choose another key. Keyboard input requires X11 and does not work in a pure Wayland session.

Set `VKBASALT_LOG_LEVEL` to `trace`, `debug`, `info`, `warn`, `error`, or `none`. Logs go to stderr unless `VKBASALT_LOG_FILE` names an output file:

```bash
ENABLE_VKBASALT=1 VKBASALT_LOG_LEVEL=debug VKBASALT_LOG_FILE=vkBasalt.log yourgame
```


## Shader parameter editors

MAKO Decky and the optional Qt UI discover controls with the bundled `mako-shader-tool CONFIG`. The helper uses the same ReShade compiler and preprocessor as the layer and emits schema-1 JSON for selected effects, without a Vulkan device. Labels, descriptions, categories, numeric bounds, choices, and defaults come from uniform annotations. Each vector/matrix/array component has an independent control. Runtime source uniforms and hidden values are excluded; numeric controls without explicit initializers default to zero. The native Deband effect shares a parameter declaration with its rendering implementation. ReShade compatibility limits still apply, and compile-time macros are not uniform controls.

The config key is `makoShader_<hex-effect-alias>_<hex-uniform-name>_<component-index>`. Indices follow compiler order within each uniform, starting at zero. Effect-local keys take precedence over legacy bare-name values, which remain readable. Reset writes the declared default rather than deleting an override and accidentally exposing a legacy value. Saves use the existing live graph-replacement path. No additional GPU pass or per-frame parameter handling is introduced; changing compile-time values can briefly interrupt rendering while a replacement graph is prepared. Shader quality settings can request more GPU work.

An effect-local value of `default` selects the shader’s compiled default in the current runtime context and overrides any legacy bare-name value. This preserves resolution-dependent defaults across resets and later resolution changes.
