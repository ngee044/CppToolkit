#!/bin/bash

# Determine OS platform
if [ "$(uname)" == "Darwin" ]; then
    # MacOS specific commands
    brew update
    brew upgrade
    brew install pkg-config autoconf cmake boost python rust

    # VCPKG installation steps
    cd ..
    if [ ! -d "./vcpkg/" ]; then
        git clone https://github.com/microsoft/vcpkg.git
    fi
    cd vcpkg
    git pull
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg upgrade --no-dry-run
    ./vcpkg install 'lz4' 'fmt' 'cryptopp' 'redis-plus-plus' 'gtest' 'libpq' 'efsw' 'boost-algorithm' 'boost-dll' 'boost-asio' 'boost-json' 'boost-system' 'boost-container' 'boost-filesystem' 'sndfile' 'libsamplerate' 'curl' 'cpp-httplib[openssl]' 'aws-sdk-cpp[ssm]' 'librabbitmq' --recurse

    cd ..

    exit
fi

if [ "$(uname)" == "Linux" ]; then
    # Linux specific commands
    apt update
    apt upgrade -y
    apt install cmake build-essential gdb rustc -y
    apt-get update
    apt-get upgrade -y
    apt-get install curl zip unzip tar ninja-build -y
    apt-get install swig pkg-config autoconf -y
    apt-get install python3-pip -y

    # Conditional Python package installation based on OS version
    if [ $(egrep "^(VERSION_ID)=" /etc/os-release) != "VERSION_ID=\"22.04\"" ]; then
        pip3 install cmake
    fi

    # Conditional environment variable setting based on architecture
    if [ $(uname -m) == "aarch64" ]; then
        export VCPKG_FORCE_SYSTEM_BINARIES=arm
    fi

    # VCPKG installation steps
    cd ..
    if [ ! -d "./vcpkg/" ]; then
        git clone https://github.com/microsoft/vcpkg.git
    fi
    cd vcpkg
    git pull
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg upgrade --no-dry-run
    ./vcpkg install 'lz4' 'fmt' 'cryptopp' 'redis-plus-plus' 'gtest' 'libpq' 'efsw' 'boost-algorithm' 'boost-dll' 'boost-asio' 'boost-json' 'boost-system' 'boost-container' 'boost-filesystem' 'sndfile' 'libsamplerate' 'curl' 'cpp-httplib[openssl]' 'aws-sdk-cpp[ssm]' 'librabbitmq' --recurse

    cd ..

    exit
fi