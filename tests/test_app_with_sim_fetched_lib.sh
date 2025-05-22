#!/bin/bash

set -e

TEST_NAME="Application with Simulated Fetched Library Dependency"
QOBS_EXECUTABLE="${1:-../cmake-build-debug/qobs}"
TEST_DIR="test_workspace_app_sim_fetched_lib"

echo "Running Test: $TEST_NAME"

cleanup() {
    echo "Cleaning up..."
    rm -rf "$TEST_DIR"
}
trap cleanup ERR EXIT

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# 1. Manually create the "fetched" library in a layout qobs would expect
# (e.g., if it were fetched into the app's _deps directory)
# For this test, we will have the app point to a dependency as if it was already fetched.
# The app's Qobs.toml will use a path dependency to this "pre-fetched" location.

# Create the library my_sim_lib
mkdir -p my_sim_lib/src my_sim_lib/include
cat > my_sim_lib/include/my_sim_lib.hpp <<EOF
#pragma once
#include <string>
std::string get_sim_lib_message();
EOF

cat > my_sim_lib/src/my_sim_lib.cpp <<EOF
#include "my_sim_lib.hpp"
std::string get_sim_lib_message() {
    return "Hello from SimLib!";
}
EOF

cat > my_sim_lib/Qobs.toml <<EOF
[package]
name = "my_sim_lib"
type = "lib"
public_include_dirs = ["include"]

[target]
sources = ["src/my_sim_lib.cpp"]
EOF

# 2. Create application app
mkdir -p app/src
# The app will have a build/_deps directory where DepGraph would normally place fetched sources.
# For this test, we're effectively saying the "fetching" step has already placed "my_sim_lib"
# at a location, and the app will reference it via a path.
# To make it more like a "fetched" scenario for DepGraph, the app would depend on "my_sim_lib"
# and DepGraph's fetch_and_get_path would return the path to my_sim_lib.
# Here, we'll simulate that the "my_sim_lib" is already inside the app's conceptual "_deps"
# or a known relative location.

# Let's place my_sim_lib adjacent to the app, and the app refers to it.
# app/Qobs.toml will have: my_sim_lib = { path = "../my_sim_lib" }
# This is similar to the local lib test, but named to reflect the "simulated fetch" intent.

cat > app/src/main.cpp <<EOF
#include <iostream>
#include "my_sim_lib.hpp" // From the dependency

int main() {
    std::cout << "App says: " << get_sim_lib_message() << std::endl;
    return 0;
}
EOF

cat > app/Qobs.toml <<EOF
[package]
name = "my_app_uses_sim_lib"

[dependencies]
# This path simulates that my_sim_lib is available as if fetched/placed by a user/script
my_sim_lib = { path = "../my_sim_lib" } 
EOF

# 3. Build the application
cd app
echo "Building application with simulated fetched library dependency..."
"$QOBS_EXECUTABLE" build --profile debug

echo "Verifying build..."
EXECUTABLE_NAME="my_app_uses_sim_lib"
if [ ! -f "build/debug/$EXECUTABLE_NAME" ] && [ ! -f "build/debug/$EXECUTABLE_NAME.exe" ]; then
    echo "FAIL: Executable '$EXECUTABLE_NAME' not found."
    exit 1
fi
EXECUTABLE_PATH=$(find build/debug -maxdepth 1 -type f \( -name "$EXECUTABLE_NAME" -o -name "$EXECUTABLE_NAME.exe" \))

OUTPUT=$($EXECUTABLE_PATH)
EXPECTED_OUTPUT="App says: Hello from SimLib!"
if [ "$OUTPUT" != "$EXPECTED_OUTPUT" ]; then
    echo "FAIL: Executable output mismatch. Got: '$OUTPUT', Expected: '$EXPECTED_OUTPUT'"
    exit 1
fi

APP_NINJA="build/debug/build.ninja"
if ! grep -q -- "-I.*my_sim_lib/include" "$APP_NINJA"; then
    echo "FAIL: Include path for my_sim_lib not found in $APP_NINJA."
    exit 1
fi

if ! grep -q "build/_deps/my_sim_lib-build/[^ ]*my_sim_lib\.\(a\|lib\)" "$APP_NINJA"; then
    echo "FAIL: Link command for my_sim_lib artifact not found or path incorrect in $APP_NINJA."
    exit 1
fi

LIB_ARTIFACT_A="build/debug/_deps/my_sim_lib-build/libmy_sim_lib.a"
LIB_ARTIFACT_LIB="build/debug/_deps/my_sim_lib-build/my_sim_lib.lib"
if [ ! -f "$LIB_ARTIFACT_A" ] && [ ! -f "$LIB_ARTIFACT_LIB" ]; then
    echo "FAIL: Static library artifact for my_sim_lib not found in app's _deps build directory."
    ls -R build/debug/_deps
    exit 1
fi

cd ../.. # Back to TEST_DIR's parent
echo "PASS: $TEST_NAME"
exit 0
