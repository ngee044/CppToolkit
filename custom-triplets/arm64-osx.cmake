set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_CXX_FLAGS "-std=c++23")
set(VCPKG_C_FLAGS "")

# build.sh probes for an SDK the active compiler can actually parse and exports it
# as SDKROOT. Honor it here so vcpkg port *source builds* use the same SDK — the
# default (newest CommandLineTools) SDK ships libc++ headers that use builtins the
# installed clang lacks (e.g. __builtin_ctzg), which breaks ports like boost-context.
if(DEFINED ENV{SDKROOT} AND NOT "$ENV{SDKROOT}" STREQUAL "")
	set(VCPKG_OSX_SYSROOT "$ENV{SDKROOT}")
endif()
