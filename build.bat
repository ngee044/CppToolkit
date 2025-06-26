@echo off
cd /d %~dp0

if not exist build (
	echo Creating build directory...
	mkdir build
	cd build
	cmake .. -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
) else (
	echo Using existing build directory...
	cd build
)

cmake --build . --config Release -- /m:16
