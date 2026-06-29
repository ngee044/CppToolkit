#!/bin/bash
#
# Build CppToolkit (Release, static) with the vcpkg toolchain + Ninja.
#
# Environment overrides (all optional):
#   VCPKG_TOOLCHAIN       path to vcpkg.cmake          (default: ../vcpkg/scripts/buildsystems/vcpkg.cmake)
#   VCPKG_INSTALLED_DIR   prebuilt vcpkg install tree  (default: ./vcpkg_installed if present)
#   FRESH_DEPS=1          ignore any prebuilt install and let vcpkg install from the manifest
#   SDKROOT               force a macOS SDK            (default: auto-detected if the compiler needs one)
#   CXX                   compiler used for the SDK probe / build (default: c++)
#
set -euo pipefail
export LC_ALL=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="$SCRIPT_DIR/build"
VCPKG_TOOLCHAIN="${VCPKG_TOOLCHAIN:-$SCRIPT_DIR/../vcpkg/scripts/buildsystems/vcpkg.cmake}"

CMAKE_ARGS=(
	-S "$SCRIPT_DIR" -B "$BUILD_DIR" -G Ninja
	-DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN"
	-DVCPKG_OVERLAY_TRIPLETS="$SCRIPT_DIR/custom-triplets"
	-DCMAKE_BUILD_TYPE=Release
	-DBUILD_SHARED_LIBS=OFF
)

# --- Reuse prebuilt vcpkg dependencies when available ----------------------
# A clean build wipes build/, which would otherwise force vcpkg to rebuild every
# dependency from source. Some ports (e.g. boost-context) can fail to build from
# source in a given toolchain. If a populated vcpkg_installed/ tree already
# exists, point vcpkg at it and skip the manifest install. Set FRESH_DEPS=1 to
# force a normal manifest install (do this after changing vcpkg.json).
INSTALLED_DIR="${VCPKG_INSTALLED_DIR:-$SCRIPT_DIR/vcpkg_installed}"
if [ "${FRESH_DEPS:-0}" != "1" ] && [ -d "$INSTALLED_DIR" ] && [ -n "$(ls -A "$INSTALLED_DIR" 2>/dev/null)" ]; then
	echo "[build.sh] Reusing prebuilt vcpkg dependencies: $INSTALLED_DIR"
	if [ "$SCRIPT_DIR/vcpkg.json" -nt "$INSTALLED_DIR" ]; then
		echo "[build.sh] WARNING: vcpkg.json is newer than the reused dependency tree;" >&2
		echo "[build.sh]          manifest changes will NOT be reflected. Run 'FRESH_DEPS=1 ./build.sh' to reinstall." >&2
	fi
	CMAKE_ARGS+=( -DVCPKG_INSTALLED_DIR="$INSTALLED_DIR" -DVCPKG_MANIFEST_INSTALL=OFF )
fi

# --- Pick a macOS SDK the active compiler can actually parse ---------------
# The default (newest) Command Line Tools SDK may ship libc++ headers that use
# builtins/macros the installed Apple clang does not support. Probe the default
# SDK; if it fails, search Xcode/CLT SDKs for one that compiles and use it.
if [ "$(uname -s)" = "Darwin" ] && [ -z "${SDKROOT:-}" ]; then
	CXX_PROBE="${CXX:-c++}"
	probe() { # $1 = sdk path ("" = compiler default)
		local sdk="$1"
		if [ -n "$sdk" ]; then
			printf '#include <string>\nint main(){}\n' | "$CXX_PROBE" -std=gnu++23 -isysroot "$sdk" -x c++ -c - -o /dev/null >/dev/null 2>&1
		else
			printf '#include <string>\nint main(){}\n' | "$CXX_PROBE" -std=gnu++23 -x c++ -c - -o /dev/null >/dev/null 2>&1
		fi
	}
	if ! probe ""; then
		SELECTED_SDK=""
		for sdk in \
			/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX*.sdk \
			/Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk; do
			[ -d "$sdk" ] || continue
			if probe "$sdk"; then
				SELECTED_SDK="$sdk"
				break
			fi
		done
		if [ -n "$SELECTED_SDK" ]; then
			echo "[build.sh] Default SDK incompatible with '$CXX_PROBE'; using SDK: $SELECTED_SDK"
			CMAKE_ARGS+=( -DCMAKE_OSX_SYSROOT="$SELECTED_SDK" )
		else
			echo "[build.sh] WARNING: no compatible macOS SDK found; building with the default SDK." >&2
		fi
	fi
fi

rm -rf "$BUILD_DIR"
cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j

echo "[build.sh] Build complete. Libraries in build/lib, samples in build/bin."
