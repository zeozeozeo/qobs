#!/bin/bash

set -e

TEST_NAME="Transitive Dependencies Test"
QOBS_EXECUTABLE="${1:-../cmake-build-debug/qobs}"
TEST_DIR="test_workspace_transitive_deps"

echo "Running Test: $TEST_NAME"

cleanup() {
    echo "Cleaning up..."
    rm -rf "$TEST_DIR"
}
trap cleanup ERR EXIT

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# Create LibB
mkdir -p LibB/src LibB/include_b
cat > LibB/include_b/lib_b.hpp <<EOF
#pragma once
#include <string>
#ifdef LIBB_PUBLIC_DEFINE
std::string get_lib_b_message_with_define();
#endif
std::string get_lib_b_message();
EOF
cat > LibB/src/lib_b.cpp <<EOF
#include "lib_b.hpp"
#ifdef LIBB_PUBLIC_DEFINE
std::string get_lib_b_message_with_define() { return "LibB_Msg_With_Define"; }
#endif
std::string get_lib_b_message() { return "Hello from LibB"; }
EOF
cat > LibB/Qobs.toml <<EOF
[package]
name = "lib_b"
type = "lib"
public_include_dirs = ["include_b"]
[target]
sources = ["src/lib_b.cpp"]
public_cflags = "-DLIBB_PUBLIC_DEFINE"
EOF

# Create LibA (depends on LibB)
mkdir -p LibA/src LibA/include_a
cat > LibA/include_a/lib_a.hpp <<EOF
#pragma once
#include <string>
#include "lib_b.hpp" // Transitive include
#ifdef LIBA_PUBLIC_DEFINE
std::string get_lib_a_message_with_define();
#endif
std::string get_lib_a_message();
EOF
cat > LibA/src/lib_a.cpp <<EOF
#include "lib_a.hpp"
#ifdef LIBA_PUBLIC_DEFINE
std::string get_lib_a_message_with_define() { return "LibA_Msg_With_Define says: " + get_lib_b_message_with_define(); }
#endif
std::string get_lib_a_message() { return "Hello from LibA, which includes: " + get_lib_b_message(); }
EOF
cat > LibA/Qobs.toml <<EOF
[package]
name = "lib_a"
type = "lib"
public_include_dirs = ["include_a"]
[dependencies]
lib_b = { path = "../LibB" }
[target]
sources = ["src/lib_a.cpp"]
public_cflags = "-DLIBA_PUBLIC_DEFINE"
EOF

# Create App (depends on LibA)
mkdir -p App/src
cat > App/src/main.cpp <<EOF
#include <iostream>
#include "lib_a.hpp" // Direct include
// lib_b.hpp should be available transitively if LibA's public_include_dirs and DepGraph work correctly
// However, main.cpp here only directly includes lib_a.hpp which itself includes lib_b.hpp
// The test is that lib_a's compilation (which needs lib_b.hpp) and App's compilation (which needs lib_a.hpp) works,
// and App links against both lib_a and lib_b.

int main() {
    std::cout << "App says: " << get_lib_a_message() << std::endl;
#ifdef LIBA_PUBLIC_DEFINE
    std::cout << "App also says (LibA define): " << get_lib_a_message_with_define() << std::endl;
#else
    std::cout << "App notes: LIBA_PUBLIC_DEFINE is not defined for app." << std::endl;
#endif
#ifdef LIBB_PUBLIC_DEFINE // Check if LibB's public_cflags propagated to App via LibA
    std::cout << "App also says (LibB define check): " << get_lib_b_message_with_define() << std::endl;
#else
    std::cout << "App notes: LIBB_PUBLIC_DEFINE is not defined for app." << std::endl;
#endif
    return 0;
}
EOF
cat > App/Qobs.toml <<EOF
[package]
name = "my_transitive_app"
[dependencies]
lib_a = { path = "../LibA" }
EOF

# Build the App
cd App
echo "Building application with transitive dependencies..."
"$QOBS_EXECUTABLE" build --profile debug

echo "Verifying build..."
EXECUTABLE_NAME="my_transitive_app"
if [ ! -f "build/debug/$EXECUTABLE_NAME" ] && [ ! -f "build/debug/$EXECUTABLE_NAME.exe" ]; then
    echo "FAIL: Executable '$EXECUTABLE_NAME' not found."
    exit 1
fi
EXECUTABLE_PATH=$(find build/debug -maxdepth 1 -type f \( -name "$EXECUTABLE_NAME" -o -name "$EXECUTABLE_NAME.exe" \))

OUTPUT=$($EXECUTABLE_PATH)
EXPECTED_LINE1="App says: Hello from LibA, which includes: Hello from LibB"
EXPECTED_LINE2="App also says (LibA define): LibA_Msg_With_Define says: LibB_Msg_With_Define" # LibA uses LibB's define
EXPECTED_LINE3="App also says (LibB define check): LibB_Msg_With_Define" # App should get LibB's define too

if ! echo "$OUTPUT" | grep -Fxq "$EXPECTED_LINE1"; then
    echo "FAIL: Output line 1 mismatch."
    echo "Expected: $EXPECTED_LINE1"
    echo "Got: $OUTPUT"
    exit 1
fi
if ! echo "$OUTPUT" | grep -Fxq "$EXPECTED_LINE2"; then
    echo "FAIL: Output line 2 (LIBA_PUBLIC_DEFINE) mismatch."
    echo "Expected: $EXPECTED_LINE2"
    echo "Got: $OUTPUT"
    exit 1
fi
if ! echo "$OUTPUT" | grep -Fxq "$EXPECTED_LINE3"; then
    echo "FAIL: Output line 3 (LIBB_PUBLIC_DEFINE for App) mismatch."
    echo "Expected: $EXPECTED_LINE3"
    echo "Got: $OUTPUT"
    exit 1
fi


APP_NINJA="build/debug/build.ninja"
# Check for include paths for LibA and LibB
if ! grep -q -- "-I.*LibA/include_a" "$APP_NINJA"; then
    echo "FAIL: Include path for LibA not found in $APP_NINJA."
    exit 1
fi
if ! grep -q -- "-I.*LibB/include_b" "$APP_NINJA"; then
    echo "FAIL: Include path for LibB not found in $APP_NINJA."
    exit 1
fi

# Check for linking against LibA and LibB artifacts
if ! grep -q "build/_deps/lib_a-build/[^ ]*lib_a\.\(a\|lib\)" "$APP_NINJA"; then
    echo "FAIL: Link command for lib_a artifact not found in $APP_NINJA."
    exit 1
fi
# LibB is a dependency of LibA, so it should be linked with the app too.
# Its path in the app's ninja file would be relative to app's build dir, under _deps.
# e.g. build/_deps/lib_b-build/liblib_b.a (if lib_a's _deps dir is not reused, which it shouldn't be for global DepGraph)
# The DepGraph makes all dependencies "flat" from the perspective of the app's build/_deps.
if ! grep -q "build/_deps/lib_b-build/[^ ]*lib_b\.\(a\|lib\)" "$APP_NINJA"; then
    echo "FAIL: Link command for lib_b artifact not found in $APP_NINJA."
    exit 1
fi

# Check that public_cflags from LibA (-DLIBA_PUBLIC_DEFINE) and LibB (-DLIBB_PUBLIC_DEFINE)
# are part of dep_cflags in App's build.ninja
if ! grep -q "dep_cflags =.*-DLIBA_PUBLIC_DEFINE" "$APP_NINJA"; then
    echo "FAIL: LIBA_PUBLIC_DEFINE from lib_a's public_cflags not found in dep_cflags of $APP_NINJA."
    exit 1
fi
if ! grep -q "dep_cflags =.*-DLIBB_PUBLIC_DEFINE" "$APP_NINJA"; then
    echo "FAIL: LIBB_PUBLIC_DEFINE from lib_b's public_cflags not found in dep_cflags of $APP_NINJA."
    exit 1
fi


# Verify artifacts exist
# App executable already checked
# LibA artifact
LIB_A_ARTIFACT_A="build/debug/_deps/lib_a-build/liblib_a.a"
LIB_A_ARTIFACT_LIB="build/debug/_deps/lib_a-build/lib_a.lib"
if [ ! -f "$LIB_A_ARTIFACT_A" ] && [ ! -f "$LIB_A_ARTIFACT_LIB" ]; then
    echo "FAIL: Static library artifact for lib_a not found."
    ls -R build/debug/_deps
    exit 1
fi
# LibB artifact
LIB_B_ARTIFACT_A="build/debug/_deps/lib_b-build/liblib_b.a"
LIB_B_ARTIFACT_LIB="build/debug/_deps/lib_b-build/lib_b.lib"
if [ ! -f "$LIB_B_ARTIFACT_A" ] && [ ! -f "$LIB_B_ARTIFACT_LIB" ]; then
    echo "FAIL: Static library artifact for lib_b not found."
    ls -R build/debug/_deps
    exit 1
fi

# TODO: Check build order if possible (e.g. timestamps or specific ninja output parsing)
# For now, correct linking and execution implies correct order.

cd ../.. # Back to TEST_DIR's parent
echo "PASS: $TEST_NAME"
exit 0
