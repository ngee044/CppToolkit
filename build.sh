#!/bin/bash

rm -rf build

mkdir build
cd build
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_OVERLAY_TRIPLETS="../custom-triplets" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
ninja
export LC_ALL=C