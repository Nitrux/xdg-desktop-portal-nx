#!/usr/bin/env bash
set -e
if [ "$EUID" -ne 0 ]; then APT_COMMAND="sudo apt"; else APT_COMMAND="apt"; fi
$APT_COMMAND update -q
$APT_COMMAND install -y --no-install-recommends ca-certificates curl gnupg

mkdir -p /etc/apt/keyrings
curl -fsSL https://packagecloud.io/nitrux/mauikit/gpgkey | gpg --dearmor -o /etc/apt/keyrings/nitrux-mauikit.gpg

cat <<EOF > /etc/apt/sources.list.d/nitrux-mauikit.sources
Types: deb
Description: Nitrux MauiKit Repo
URIs: https://packagecloud.io/nitrux/mauikit/debian/
Suites: duke
Components: main
Signed-By: /etc/apt/keyrings/nitrux-mauikit.gpg
Enabled: yes
EOF
$APT_COMMAND update -q
$APT_COMMAND install -y --no-install-recommends build-essential cmake curl dpkg-dev extra-cmake-modules gnupg pkg-config qt6-base-dev qt6-declarative-dev qt6-wayland-dev mauikit mauikit-filebrowsing
