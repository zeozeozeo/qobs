cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(qobs LANGUAGES CXX VERSION 0.1.0)

# add executable
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})

# cpm
include(cmake/CPM.cmake)

# add packages
CPMAddPackage("gh:p-ranav/argparse#1b27c6f3da8204a2d130d9b0308c485e46b762a9")
CPMAddPackage("gh:fmtlib/fmt#ffdc3fdbd90385429dd2ea6a774848e39d4f957a")
CPMAddPackage("gh:marzer/tomlplusplus#e2bae9d559b4956a831fcef10ac8f01c88cb0d13")
CPMAddPackage("gh:gabime/spdlog#5ebfc927306fd7ce551fa22244be801cf2b9fdd9")
CPMAddPackage("gh:p-ranav/glob#d025092c0e1eb1a8b226d3a799fd32680d2fd13f")

# link dependencies
target_link_libraries(${PROJECT_NAME}
                      fmt::fmt
                      spdlog::spdlog
                      argparse
                      tomlplusplus::tomlplusplus
                      Glob)
