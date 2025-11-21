#!/bin/bash

# Default to release if no argument is provided
BUILD_TYPE="Release"
DEVICE_PATH="/dev/input/event5"

# Convert input to lowercase
INPUT_TYPE=$(echo "$1" | tr '[:upper:]' '[:lower:]')

if [[ "$INPUT_TYPE" == "debug" ]]; then
    BUILD_TYPE="Debug"
fi

echo "Building in $BUILD_TYPE mode..."

# Create build directory if not present
mkdir -p build
cd build || exit 1

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build the project
make -j$(nproc)

# Run the binary
echo "Running gesture_daemon on $DEVICE_PATH"
./gesture_daemon "$DEVICE_PATH"
