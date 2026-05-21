cmake_minimum_required(VERSION 3.20)
project(arenalang)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain file is set via CMAKE_TOOLCHAIN_FILE
# or automatically by vcpkg integration

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

# Add the executable
add_executable(arena-bin src/main.cpp)

# Configure LLVM for the target
target_include_directories(arena-bin PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(arena-bin PRIVATE ${LLVM_DEFINITIONS})

# Link LLVM libraries (must use llvm_map_components_to_libnames)
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(arena-bin PRIVATE ${llvm_libs})
