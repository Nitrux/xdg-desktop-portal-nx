#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
PACKAGE_VERSION="${PACKAGE_VERSION:-0.0.1}"
TARGET_ARCH="${TARGET_ARCH:-$(dpkg --print-architecture)}"

build_dir="$repo_root/build"
meson_dir="$build_dir/meson"
stage_dir="$build_dir/package"
rm -rf "$meson_dir" "$stage_dir"
mkdir -p "$build_dir"

meson_args=(--prefix=/usr --buildtype=release)
if [ "$(dpkg-architecture -qDEB_HOST_ARCH)" = "amd64" ]; then
    meson_args+=("-Dcpp_args=-march=x86-64-v3")
fi

meson setup "$meson_dir" "$repo_root" "${meson_args[@]}"
meson compile -C "$meson_dir" -j "$(nproc)"
DESTDIR="$stage_dir" meson install -C "$meson_dir"

mkdir -p "$stage_dir/DEBIAN"
cat > "$stage_dir/DEBIAN/control" <<CONTROL
Package: xdg-desktop-portal-nx
Version: ${PACKAGE_VERSION}
Section: utils
Priority: optional
Architecture: ${TARGET_ARCH}
Maintainer: uri_herrera@nxos.org
Provides: xdg-desktop-portal-nx
Depends: xdg-desktop-portal,xdg-desktop-portal-hyprland,mauikit (>= 4.0.5),mauikit-filebrowsing (>= 4.0.5),qml6-module-qtquick-controls,qml6-module-qtquick-layouts,qt6-wayland
Description: Nitrux XDG desktop portal.
 Nitrux XDG desktop portal.
CONTROL

dpkg-deb --build --root-owner-group "$stage_dir" "$build_dir/xdg-desktop-portal-nx_${PACKAGE_VERSION}_${TARGET_ARCH}.deb"
