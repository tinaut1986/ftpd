#!/bin/sh
# Build both Switch variants (ftpd-EX and classic) and package the NRO files.
#
# Usage: scripts/build-switch.sh [version-label]
# Output: dist/ftpd-ex-<label>.nro and dist/ftpd-classic-<label>.nro
#
# Needs devkitA64 + libnx and the Switch portlibs (switch-curl switch-libzstd
# switch-jansson switch-zlib switch-mbedtls switch-glm), plus ImageMagick and zstd
# for the texture assets.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LABEL=${1:-dev}
DIST=$ROOT/dist
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITA64=${DEVKITA64:-$DEVKITPRO/devkitA64}
export PATH="$DEVKITPRO/tools/bin:$PATH"

mkdir -p "$DIST"

# build_variant <build dir> <cmake target name> <output name> [extra cmake args...]
build_variant() {
	dir=$ROOT/$1 target=$2 name=$3
	shift 3
	cmake -S "$ROOT" -B "$dir" -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
		-DCMAKE_BUILD_TYPE=Release "$@"
	cmake --build "$dir" -j"$(nproc)"
	cp "$dir/$target.nro" "$DIST/$name-$LABEL.nro"
}

build_variant build-switch ftpd ftpd-ex -DFTPD_CLASSIC=OFF
build_variant build-switch-classic ftpd-classic ftpd-classic -DFTPD_CLASSIC=ON

ls -l "$DIST"
