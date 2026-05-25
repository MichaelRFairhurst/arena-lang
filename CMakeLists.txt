cmake_minimum_required(VERSION 3.20)
project(Arena VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ARENA_BUILD_COMPILER "Build Arena compiler" ON)
option(ARENA_BUILD_RUNTIME "Build runtime libraries" ON)
option(ARENA_ENABLE_LLVM "Enable LLVM backend" ON)

# vcpkg toolchain file is set via CMAKE_TOOLCHAIN_FILE
# or automatically by vcpkg integration

# Find dependencies
if(ARENA_ENABLE_LLVM)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
endif()

# Add subdirectories
if(ARENA_BUILD_RUNTIME)
    add_subdirectory(lib)
endif()

if(ARENA_BUILD_COMPILER)
    add_subdirectory(compile)
endif()
