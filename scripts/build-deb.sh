#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build
cd build
PACKAGE_VERSION="${PACKAGE_VERSION:-0.0.1}"
TARGET_ARCH="${TARGET_ARCH:-$(dpkg --print-architecture)}"
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc -DCMAKE_BUILD_TYPE=Release -DCPACK_PACKAGE_VERSION="${PACKAGE_VERSION}" -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="${TARGET_ARCH}" ..
cmake --build . --parallel "$(nproc)"
cpack -G DEB -C Release
