#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release_tag="${1:?usage: scripts/package-mako-release.sh RELEASE_TAG [OUTPUT_PATH]}"
output_path="${2:-$repo_root/dist/vkBasalt-$release_tag-linux-x86.tar.xz}"

if [[ ! "$release_tag" =~ ^mako-v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+-[0-9]+$ ]]; then
    echo "Release tag must use mako-vX.Y.Z.UPSTREAM-REVISION: $release_tag" >&2
    exit 2
fi

for command in git meson ninja glslangValidator pkg-config readelf strip tar sha256sum xz; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Required command not found: $command" >&2
        exit 1
    fi
done

work_root="$(mktemp -d)"
cleanup() {
    rm -rf "$work_root"
}
trap cleanup EXIT

build64="$work_root/build64"
build32="$work_root/build32"
stage64="$work_root/stage64"
stage32="$work_root/stage32"
payload="$work_root/payload"

resolve_pkg_config32() {
    local candidate
    for candidate in /usr/lib32/pkgconfig /usr/lib/i386-linux-gnu/pkgconfig; do
        if [[ -f "$candidate/x11.pc" ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    if [[ -f /usr/lib32/libX11.so ]] && pkg-config --exists x11; then
        candidate="$work_root/pkgconfig32"
        mkdir -p "$candidate"
        pkg-config --variable=pcfiledir x11 >/dev/null
        sed -e 's|^libdir=.*|libdir=/usr/lib32|' \
            -e '/^Requires\.private:/d' \
            "$(pkg-config --variable=pcfiledir x11)/x11.pc" \
            > "$candidate/x11.pc"
        printf '%s\n' "$candidate"
        return
    fi

    echo "Could not locate 32-bit X11 pkg-config metadata" >&2
    exit 1
}

pkg_config32="$(resolve_pkg_config32)"
pkg_config32_libdir="$pkg_config32:/usr/share/pkgconfig"
resolved_x11_libdir="$(PKG_CONFIG_LIBDIR="$pkg_config32_libdir" pkg-config --variable=libdir x11)"
if [[ "$resolved_x11_libdir" != /usr/lib32 && "$resolved_x11_libdir" != /usr/lib/i386-linux-gnu ]]; then
    echo "32-bit X11 resolved to the wrong library directory: $resolved_x11_libdir" >&2
    exit 1
fi

meson setup "$build64" "$repo_root" \
    --buildtype=release \
    --prefix=/usr \
    --libdir=lib \
    -Dwith_json=false
meson compile -C "$build64"
DESTDIR="$stage64" meson install -C "$build64"

ASFLAGS=--32 CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 \
PKG_CONFIG_LIBDIR="$pkg_config32_libdir" \
meson setup "$build32" "$repo_root" \
    --buildtype=release \
    --prefix=/usr \
    --libdir=lib32 \
    -Dwith_json=false
meson compile -C "$build32"
DESTDIR="$stage32" meson install -C "$build32"

install -Dm755 "$stage64/usr/lib/libvkbasalt.so" \
    "$payload/lib/libvkbasalt.so"
install -Dm755 "$stage32/usr/lib32/libvkbasalt.so" \
    "$payload/lib32/libvkbasalt.so"
strip --strip-unneeded "$payload/lib/libvkbasalt.so"
strip --strip-unneeded "$payload/lib32/libvkbasalt.so"

install -Dm644 "$repo_root/packaging/vkBasalt.x86_64.json" \
    "$payload/share/vulkan/implicit_layer.d/vkBasalt.x86_64.json"
install -Dm644 "$repo_root/packaging/vkBasalt.x86.json" \
    "$payload/share/vulkan/implicit_layer.d/vkBasalt.x86.json"
install -Dm644 "$repo_root/LICENSE" \
    "$payload/share/doc/vkbasalt/LICENSE"
install -Dm644 "$repo_root/src/reshade/LICENSE.md" \
    "$payload/share/doc/vkbasalt/RESHade-LICENSE.md"

source_commit="$(git -C "$repo_root" rev-parse HEAD)"
upstream_commit="$(tr -d '[:space:]' < "$repo_root/packaging/UPSTREAM_BASE")"
if [[ ! "$upstream_commit" =~ ^[0-9a-f]{40}$ ]] || \
        ! git -C "$repo_root" merge-base --is-ancestor "$upstream_commit" HEAD; then
    echo "packaging/UPSTREAM_BASE must identify an ancestor of the release commit" >&2
    exit 1
fi
mkdir -p "$payload/share/doc/vkbasalt"
printf '%s\n' \
    "repository=https://github.com/eugeniosegala/vkBasalt" \
    "tag=$release_tag" \
    "commit=$source_commit" \
    "upstream_repository=https://github.com/DadSchoorse/vkBasalt" \
    "upstream_commit=$upstream_commit" \
    > "$payload/share/doc/vkbasalt/SOURCE"

verify_elf_class() {
    local path="$1"
    local expected="$2"
    local actual
    actual="$(od -An -t u1 -j 4 -N 1 "$path" | tr -d '[:space:]')"
    if [[ "$actual" != "$expected" ]]; then
        echo "$path has ELF class byte $actual; expected $expected" >&2
        exit 1
    fi
}

verify_layer() {
    local path="$1"
    if ! readelf -Ws "$path" | grep -F 'vkBasalt_GetInstanceProcAddr' >/dev/null; then
        echo "$path does not export vkBasalt_GetInstanceProcAddr" >&2
        exit 1
    fi
    if ! readelf -d "$path" | grep -F 'Shared library: [libX11.so.6]' >/dev/null; then
        echo "$path does not declare the expected X11 runtime dependency" >&2
        exit 1
    fi
}

verify_elf_class "$payload/lib/libvkbasalt.so" 2
verify_elf_class "$payload/lib32/libvkbasalt.so" 1
verify_layer "$payload/lib/libvkbasalt.so"
verify_layer "$payload/lib32/libvkbasalt.so"

(
    cd "$payload"
    find lib lib32 share -type f -print | LC_ALL=C sort | xargs sha256sum \
        > SHA256SUMS
    sha256sum --check --status SHA256SUMS
)

mkdir -p "$(dirname "$output_path")"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -C "$payload" -cJf "$output_path" .
sha256sum "$output_path" > "$output_path.sha256"
echo "Created and verified $output_path"
