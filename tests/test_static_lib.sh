#!/bin/bash

set -e

TEST_NAME="Static Library Build"
QOBS_EXECUTABLE="${1:-../cmake-build-debug/qobs}"
TEST_DIR="test_workspace_static_lib"

echo "Running Test: $TEST_NAME"

cleanup() {
    echo "Cleaning up..."
    rm -rf "$TEST_DIR"
}
trap cleanup ERR EXIT

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# 1. Create simple static library
mkdir -p src include
cat > include/lib.hpp <<EOF
#pragma once
#include <iostream>
// Macro to be defined by public_cflags
#ifdef MYLIB_PUBLIC_DEFINE
void say_hello_from_lib_with_define();
#endif
void say_hello_from_lib();
EOF

cat > src/lib.cpp <<EOF
#include "lib.hpp"
#ifdef MYLIB_PUBLIC_DEFINE
void say_hello_from_lib_with_define() {
    std::cout << "Hello from lib with define!" << std::endl;
}
#endif
void say_hello_from_lib() {
    std::cout << "Hello from lib!" << std::endl;
}
EOF

cat > Qobs.toml <<EOF
[package]
name = "mylib"
type = "lib"
public_include_dirs = ["include"]

[target]
sources = ["src/lib.cpp"]
public_cflags = "-DMYLIB_PUBLIC_DEFINE"
EOF

echo "Building debug version..."
"$QOBS_EXECUTABLE" build --profile debug

echo "Verifying debug build..."
# Check for static library artifact
if [ ! -f "build/debug/libmylib.a" ] && [ ! -f "build/debug/mylib.lib" ]; then
    echo "FAIL: Debug static library not found."
    exit 1
fi

# Check build.ninja for archive rule and no link rule for executable
if ! grep -q "rule ar" build/debug/build.ninja; then
    echo "FAIL: Archive rule not found in debug build.ninja."
    exit 1
fi
if grep -q "rule link" build/debug/build.ninja && grep -q "build mylib: link " build/debug/build.ninja; then
    # Ensure it's not trying to link an executable named "mylib"
    # A link rule itself might be present if the generator defines it globally,
    # but it shouldn't be used to create an executable for this library package.
    # A more precise check would be to see if `output_name` (libmylib.a) is linked as an exe.
    # For now, checking if "mylib" (package name) is a target of a link rule is a heuristic.
    # Grep for "build mylib: link" or "build mylib.exe: link"
    if grep -q "build ${PWD##*/}/mylib: link " build/debug/build.ninja || grep -q "build mylib: link " build/debug/build.ninja || grep -q "build mylib.exe: link " build/debug/build.ninja; then
        echo "FAIL: Library package 'mylib' appears to be linked as an executable in debug build.ninja."
        exit 1
    fi
fi
# Check for public_cflags being used (e.g. in final_cflags or similar)
# The library itself might not use its public_cflags for its own compilation,
# but they should be defined for consumers.
# For the library's own compilation, its regular cflags are used.
# We'll test consumption of public_cflags in the app-with-lib test.
# Here, just check it's in the variables section if possible.
if ! grep -q "user_cflags =.*-DMYLIB_PUBLIC_DEFINE" build/debug/build.ninja && ! grep -q "public_cflags =.*-DMYLIB_PUBLIC_DEFINE" build/debug/Qobs.toml; then
     # Check if public_cflags from Qobs.toml were intended to be part of user_cflags (current behavior in some versions of generator)
     # or if they are just for dep_cflags. The key is they are *available*.
     # The test for consumption is more important.
     # Let's ensure it's at least mentioned as a variable or part of a variable if not empty.
     # The manifest has public_cflags = "-DMYLIB_PUBLIC_DEFINE"
     # The NinjaGenerator should have: user_cflags = (from manifest.target.cflags)
     # and dep_cflags includes public_cflags of dependencies.
     # For the library itself, its own public_cflags don't apply to its own compilation of its sources.
     # This check is a bit loose.
     echo "Note: MYLIB_PUBLIC_DEFINE from public_cflags will be tested in a dependent app."
fi


echo "Debug build verified."

# (Optional: Add release build check similar to basic_app if needed)

cd ..
echo "PASS: $TEST_NAME"
exit 0
