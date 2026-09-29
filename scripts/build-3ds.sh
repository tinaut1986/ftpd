#!/bin/sh
# Build both 3DS variants (ftpd-EX and classic) and package 3dsx + CIA files.
#
# Usage: scripts/build-3ds.sh [version-label]
# Output: dist/ftpd-ex-<label>.{3dsx,cia} and dist/ftpd-classic-<label>.{3dsx,cia}
#
# Needs devkitARM + 3DS portlibs (3ds-curl 3ds-mbedtls 3ds-zlib 3ds-jansson) and
# the CMake toolchain at /opt/devkitpro/cmake/3DS.cmake. makerom/bannertool come
# from tools/bin.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LABEL=${1:-dev}
DIST=$ROOT/dist
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=${DEVKITARM:-$DEVKITPRO/devkitARM}
export PATH="$ROOT/tools/bin:$DEVKITPRO/tools/bin:$PATH"

mkdir -p "$DIST"
bannertool makebanner -i "$ROOT/meta/banner.png" -a "$ROOT/meta/audio.wav" -o "$DIST/ftpd.bnr" > /dev/null

# build_variant <build dir> <cmake target name> <output name> <rsf> [extra cmake args...]
build_variant() {
	dir=$ROOT/$1 target=$2 name=$3 rsf=$4
	shift 4
	cmake -S "$ROOT" -B "$dir" -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
		-DCMAKE_BUILD_TYPE=Release "$@"
	cmake --build "$dir" -j"$(nproc)"

	(
		cd "$dir"
		# The RSF paths are relative to the build directory
		sed 's#\.\./romfs#romfs#' "$rsf" > "$dir/$target-cia.rsf"
		makerom -f cia -o "$DIST/$name-$LABEL.cia" -elf "$target.elf" -rsf "$target-cia.rsf" \
			-icon "$target.smdh" -banner "$DIST/ftpd.bnr" -exefslogo -target t
	)
	cp "$dir/$target.3dsx" "$DIST/$name-$LABEL.3dsx"
}

build_variant build-3ds ftpd ftpd-ex "$ROOT/meta/ftpd-cia.rsf" -DFTPD_CLASSIC=OFF
build_variant build-3ds-classic ftpd-classic ftpd-classic "$ROOT/meta/ftpd-classic-cia.rsf" -DFTPD_CLASSIC=ON

rm -f "$DIST/ftpd.bnr"
ls -l "$DIST"
