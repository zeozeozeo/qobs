cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(qobs LANGUAGES C CXX VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)

# add executable
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})


# cpm
include(cmake/CPM.cmake)

# configure libraries
add_definitions(-DGIT_DEPRECATE_HARD) # libgit2: ensure that we do not use deprecated functions internally
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # libgit2
# silence stupid libgit2 warnings on Windows
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wno-language-extension-token -Wno-long-long -Wno-documentation")

# add packages
CPMAddPackage("gh:p-ranav/argparse#1b27c6f3da8204a2d130d9b0308c485e46b762a9")
CPMAddPackage("gh:fmtlib/fmt#ffdc3fdbd90385429dd2ea6a774848e39d4f957a")
CPMAddPackage("gh:marzer/tomlplusplus#e2bae9d559b4956a831fcef10ac8f01c88cb0d13")
CPMAddPackage("gh:gabime/spdlog#5ebfc927306fd7ce551fa22244be801cf2b9fdd9")
CPMAddPackage("gh:p-ranav/glob#d025092c0e1eb1a8b226d3a799fd32680d2fd13f")
CPMAddPackage(
    NAME LIBGIT2
    GITHUB_REPOSITORY libgit2/libgit2
    VERSION 1.8.1
)

target_include_directories(${PROJECT_NAME} PRIVATE thirdparty ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} ${LIBGIT2_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

# link dependencies
target_link_libraries(${PROJECT_NAME}
                      fmt::fmt
                      spdlog::spdlog
                      argparse
                      tomlplusplus::tomlplusplus
                      Glob
                      libgit2package)
