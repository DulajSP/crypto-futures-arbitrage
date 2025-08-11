#!/bin/bash

# Clean previous build artifacts
rm -rf build

# Create build directory
mkdir build
cd build

# Configure project with CMake and vcpkg toolchain
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build project using Ninja
ninja