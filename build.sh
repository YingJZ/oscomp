#!/bin/bash

# Create build directory if it doesn't exist
rm -rf build
mkdir -p build
cd build

# Configure with CMake
cmake ..
# Build all targets
cmake --build  . # --verbose
cd ..

echo "Build complete. Binaries are in build/bin and build/hostfs_root/bin"
echo "Run with: cd build && cmake --build . --target run"