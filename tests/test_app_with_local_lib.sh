#!/bin/bash

set -e

TEST_NAME="Application with Local Static Library Dependency"
QOBS_EXECUTABLE="${1:-../cmake-build-debug/qobs}"
TEST_DIR="test_workspace_app_local_lib"

echo "Running Test: $TEST_NAME"

cleanup() {
    echo "Cleaning up..."
    rm -rf "$TEST_DIR"
}
trap cleanup ERR EXIT

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# 1. Create library lib_dep
mkdir -p lib_dep/src lib_dep/include
cat > lib_dep/include/lib_dep.hpp <<EOF
#pragma once
#include <string>
#ifdef LIB_DEP_PUBLIC_DEFINE
std::string get_lib_dep_message_with_define();
#endif
std::string get_lib_dep_message();
EOF

cat > lib_dep/src/lib_dep.cpp <<EOF
#include "lib_dep.hpp"
#ifdef LIB_DEP_PUBLIC_DEFINE
std::string get_lib_dep_message_with_define() {
    return "LibDepMsgWithDefine";
}
#endif
std::string get_lib_dep_message() {
    return "Hello from LibDep!";
}
EOF

cat > lib_dep/Qobs.toml <<EOF
[package]
name = "my_lib_dep"
type = "lib"
public_include_dirs = ["include"]

[target]
sources = ["src/lib_dep.cpp"]
public_cflags = "-DLIB_DEP_PUBLIC_DEFINE"
EOF

# 2. Create application app
mkdir -p app/src
cat > app/src/main.cpp <<EOF
#include <iostream>
#include "lib_dep.hpp" // From the dependency

int main() {
    std::cout << "App says: " << get_lib_dep_message() << std::endl;
#ifdef LIB_DEP_PUBLIC_DEFINE
    std::cout << "App also says: " << get_lib_dep_message_with_define() << std::endl;
#else
    std::cout << "App notes: LIB_DEP_PUBLIC_DEFINE is not defined for app." << std::endl;
#endif
    return 0;
}
EOF

cat > app/Qobs.toml <<EOF
[package]
name = "my_app_with_dep"

[dependencies]
my_lib_dep = { path = "../lib_dep" }
EOF

# 3. Build the application (which should build the library first)
cd app
echo "Building application with local library dependency..."
"$QOBS_EXECUTABLE" build --profile debug

echo "Verifying build..."
# Check for executable
EXECUTABLE_NAME="my_app_with_dep"
if [ ! -f "build/debug/$EXECUTABLE_NAME" ] && [ ! -f "build/debug/$EXECUTABLE_NAME.exe" ]; then
    echo "FAIL: Executable '$EXECUTABLE_NAME' not found."
    exit 1
fi
EXECUTABLE_PATH=$(find build/debug -maxdepth 1 -type f \( -name "$EXECUTABLE_NAME" -o -name "$EXECUTABLE_NAME.exe" \))

# Check executable output
OUTPUT=$($EXECUTABLE_PATH)
EXPECTED_OUTPUT_LINE1="App says: Hello from LibDep!"
EXPECTED_OUTPUT_LINE2="App also says: LibDepMsgWithDefine"

if ! echo "$OUTPUT" | grep -q "$EXPECTED_OUTPUT_LINE1"; then
    echo "FAIL: Executable output line 1 mismatch. Got: '$OUTPUT'"
    exit 1
fi
if ! echo "$OUTPUT" | grep -q "$EXPECTED_OUTPUT_LINE2"; then
    echo "FAIL: Executable output line 2 mismatch (LIB_DEP_PUBLIC_DEFINE not propagated). Got: '$OUTPUT'"
    exit 1
fi

# Check app's build.ninja for include paths and linking
APP_NINJA="build/debug/build.ninja"
if ! grep -q -- "-I.*lib_dep/include" "$APP_NINJA"; then
    echo "FAIL: Include path for lib_dep not found in $APP_NINJA."
    exit 1
fi

# Check for linking against the library artifact.
# The path to the library artifact will be relative to the app's build directory,
# typically in _deps/my_lib_dep-build/libmy_lib_dep.a or _deps/my_lib_dep-build/my_lib_dep.lib
# For Ninja files, paths might be escaped or use variables.
# We expect something like: build/_deps/my_lib_dep-build/(lib)my_lib_dep.(a|lib)
if ! grep -q "build/_deps/my_lib_dep-build/[^ ]*my_lib_dep\.\(a\|lib\)" "$APP_NINJA"; then
    echo "FAIL: Link command for my_lib_dep artifact not found or path incorrect in $APP_NINJA."
    grep "link" "$APP_NINJA" # Print link commands for debugging
    exit 1
fi


# Check that LIB_DEP_PUBLIC_DEFINE was used for compiling app/src/main.cpp
# This means it should be part of the 'dep_cflags' or 'final_cflags' in app's build.ninja
# and used in the compile line for main.cpp.
# The 'dep_cflags' variable should contain -DLIB_DEP_PUBLIC_DEFINE
if ! grep -q "dep_cflags =.*-DLIB_DEP_PUBLIC_DEFINE" "$APP_NINJA"; then
    echo "FAIL: LIB_DEP_PUBLIC_DEFINE from lib_dep's public_cflags not found in dep_cflags of $APP_NINJA."
    exit 1
fi

# Check that the library itself was built
LIB_ARTIFACT_A="build/debug/_deps/my_lib_dep-build/libmy_lib_dep.a"
LIB_ARTIFACT_LIB="build/debug/_deps/my_lib_dep-build/my_lib_dep.lib"
if [ ! -f "$LIB_ARTIFACT_A" ] && [ ! -f "$LIB_ARTIFACT_LIB" ]; then
    echo "FAIL: Static library artifact for my_lib_dep not found in app's _deps build directory."
    ls -R build/debug/_deps # List contents for debugging
    exit 1
fi

cd ../.. # Back to TEST_DIR's parent
echo "PASS: $TEST_NAME"
exit 0
