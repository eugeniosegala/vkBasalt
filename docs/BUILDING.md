# Building and releases

[Back to README](../README.md)

MAKO installs a private release bundle. The standalone installation commands here are unnecessary for MAKO users.

## Distribution packages

For a separate system installation, these packages provide vkBasalt but may not include this fork's MAKO changes:

| Distribution | Command |
| --- | --- |
| [Debian](https://tracker.debian.org/pkg/vkbasalt) | `sudo apt install vkbasalt` |
| [Fedora](https://src.fedoraproject.org/rpms/vkBasalt) | `sudo dnf install vkBasalt` |
| [Void Linux](https://github.com/void-linux/void-packages/blob/master/srcpkgs/vkBasalt/template) | `sudo xbps-install vkBasalt` |

## Dependencies

- GCC 9 or newer, Meson, Ninja, and pkg-config.
- X11 development files; both architectures for a dual-architecture build.
- glslang (`glslangValidator`), SPIR-V Headers, and Vulkan Headers.
- Python 3 for validation; Git, binutils, tar, SHA-256 tools, and xz for release packaging.
- A multilib compiler and 32-bit development libraries for 32-bit builds.

[vulkan-headers-revision.txt](../vulkan-headers-revision.txt) owns the minimum Vulkan Headers version. Direct builds can select an external checkout with `-Dvulkan_headers=/path/to/Vulkan-Headers/include`. The release packager fetches the exact pinned tag for both architectures, aligned with MAKO Renderer's header pin.

The reviewed Vulkan API declaration is separate: [meson.build](../meson.build) generates the manifests and runtime declaration from it. Review API support before changing that declaration during a header upgrade.

## Build and test

```bash
git clone https://github.com/eugeniosegala/vkBasalt.git
cd vkBasalt
meson setup --buildtype=release --prefix=/usr builddir
meson compile -C builddir
meson test -C builddir --print-errorlogs
```

For 32-bit builds, adjust the library and pkg-config paths for your distribution. Debian-based systems use `lib/i386-linux-gnu` instead of `lib32`:

```bash
ASFLAGS=--32 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 \
PKG_CONFIG_LIBDIR=/usr/lib32/pkgconfig:/usr/share/pkgconfig \
meson setup --buildtype=release --prefix=/usr --libdir=lib32 \
  -Dwith_json=false builddir.32
meson compile -C builddir.32
meson test -C builddir.32 --print-errorlogs
```

The tests cover configuration reload, effect replacement, shader compilation, texture validation, Vulkan construction rollback, API forwarding, depth lifetime, presentation allocation, and HDR policies. They do not establish game compatibility or GPU image quality.

## Optional system installation

After building, install with `meson install -C builddir` and, if needed, `meson install -C builddir.32`. This may require elevated permissions. `--prefix=/usr` writes into package-manager-owned directories; omit it at setup time to use `/usr/local`, and ensure the dynamic linker can find the library.

## MAKO release bundle

[scripts/package-mako-release.sh](../scripts/package-mako-release.sh) builds and tests both architectures, verifies the libraries, and creates an archive with private manifests, checksums, source identity, and licenses. It neither installs nor publishes. Tags must match `mako-vX.Y.Z.UPSTREAM-REVISION`; for example:

```bash
scripts/package-mako-release.sh mako-v0.3.2.10-1 /tmp/vkBasalt-candidate.tar.xz
```

The archive's `share/doc/vkbasalt/SOURCE` records the source commit, Vulkan header tag and resolved commit, and declared API version.

Pushing a new `mako-v*` tag triggers the [release workflow](../.github/workflows/release-mako.yml), which runs the same packager and publishes an attested archive. Published releases are immutable. MAKO verifies the public asset before updating its exact tag/checksum pin and generated Flatpak module.
