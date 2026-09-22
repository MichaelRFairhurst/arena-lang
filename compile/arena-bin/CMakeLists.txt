find_package(boost_program_options CONFIG REQUIRED)

# Arena compiler executable
add_executable(arena-bin
    src/main.cpp
)

target_include_directories(arena-bin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arena-bin
    PRIVATE
        arena-frontend
        arena-codegen
        Boost::program_options
)

# Link LLVM backend if enabled
if(ARENA_ENABLE_LLVM)
    target_link_libraries(arena-bin PRIVATE arena-backend-llvm)
    target_compile_definitions(arena-bin PRIVATE ARENA_HAS_LLVM_BACKEND)
endif()

target_compile_features(arena-bin PUBLIC cxx_std_17)

# Place executable in build/arena-bin instead of build/compile/arena-bin/arena-bin
set_target_properties(arena-bin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Install the compiler
install(TARGETS arena-bin DESTINATION bin)
