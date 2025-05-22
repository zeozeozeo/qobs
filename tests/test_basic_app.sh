#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

TEST_NAME="Basic Application Build"
QOBS_EXECUTABLE="${1:-../cmake-build-debug/qobs}" # Allow overriding qobs executable path
TEST_DIR="test_workspace_basic_app"

echo "Running Test: $TEST_NAME"

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    rm -rf "$TEST_DIR"
}

# Trap ERR and EXIT signals to ensure cleanup happens
trap cleanup ERR EXIT

# Create a clean test directory
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# 1. Create simple "hello world" application
mkdir -p src
cat > src/main.cpp <<EOF
#include <iostream>
int main() {
    std::cout << "Hello, world!" << std::endl;
    return 0;
}
EOF

cat > Qobs.toml <<EOF
[package]
name = "hello_app"
# type defaults to "app"
EOF

echo "Building debug version..."
"$QOBS_EXECUTABLE" build --profile debug

echo "Verifying debug build..."
# Check for executable
if [ ! -f "build/debug/hello_app" ] && [ ! -f "build/debug/hello_app.exe" ]; then
    echo "FAIL: Debug executable not found."
    exit 1
fi
EXECUTABLE_PATH_DEBUG=$(find build/debug -maxdepth 1 -type f \( -name "hello_app" -o -name "hello_app.exe" \))

# Check executable output
OUTPUT_DEBUG=$($EXECUTABLE_PATH_DEBUG)
if [ "$OUTPUT_DEBUG" != "Hello, world!" ]; then
    echo "FAIL: Debug executable output mismatch. Got: '$OUTPUT_DEBUG'"
    exit 1
fi

# Check build.ninja for debug flags
if ! grep -q -- "-g" build/debug/build.ninja; then
    echo "FAIL: Debug flags not found in debug build.ninja."
    exit 1
fi
echo "Debug build verified."

echo "Building release version..."
"$QOBS_EXECUTABLE" build --profile release

echo "Verifying release build..."
# Check for executable
if [ ! -f "build/release/hello_app" ] && [ ! -f "build/release/hello_app.exe" ]; then
    echo "FAIL: Release executable not found."
    exit 1
fi
EXECUTABLE_PATH_RELEASE=$(find build/release -maxdepth 1 -type f \( -name "hello_app" -o -name "hello_app.exe" \))

# Check executable output
OUTPUT_RELEASE=$($EXECUTABLE_PATH_RELEASE)
if [ "$OUTPUT_RELEASE" != "Hello, world!" ]; then
    echo "FAIL: Release executable output mismatch. Got: '$OUTPUT_RELEASE'"
    exit 1
fi

# Check build.ninja for release flags
if ! grep -q -- "-O2" build/release/build.ninja || ! grep -q -- "-DNDEBUG" build/release/build.ninja; then
    echo "FAIL: Release flags not found in release build.ninja."
    exit 1
fi
echo "Release build verified."

cd ..
echo "PASS: $TEST_NAME"
exit 0
