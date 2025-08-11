#!/bin/bash

# Determine OS platform
if [ "$(uname)" == "Darwin" ]; then
    # MacOS specific commands
    brew update
    brew upgrade
    brew install pkg-config autoconf cmake boost python rust clang-uml plantuml ninja

    # VCPKG installation steps
    cd ..
    if [ ! -d "./vcpkg/" ]; then
        git clone https://github.com/microsoft/vcpkg.git
    fi
    cd vcpkg
    git pull
    ./bootstrap-vcpkg.sh
    ./vcpkg upgrade --no-dry-run

    cd ..

    exit
fi

if [ "$(uname)" == "Linux" ]; then
    # Linux specific commands
    add-apt-repository ppa:bkryza/clang-uml
    
    apt update
    apt upgrade -y
    apt install cmake build-essential gdb rustc linux-libc-dev -y
    apt install clang-uml plantuml -y

    apt-get update
    apt-get upgrade -y
    apt-get install curl zip unzip tar ninja-build -y
    apt-get install bison flex pkg-config autoconf -y

    # Conditional Python package installation based on OS version
    if [ $(egrep "^(VERSION_ID)=" /etc/os-release) != "VERSION_ID=\"22.04\"" ]; then
        apt-get install python3-pip -y
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
    ./vcpkg upgrade --no-dry-run

    cd ..

    exit
fi
