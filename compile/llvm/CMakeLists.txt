# LLVM backend implementation
add_library(arena-backend-llvm
    # Add source files when you implement the LLVM backend:
    # src/llvm_backend.cpp
    # src/llvm_codegen.cpp
    src/main.cpp
)

target_include_directories(arena-backend-llvm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(arena-backend-llvm
    PUBLIC
        arena-codegen
    PRIVATE
        ${LLVM_LIBRARIES}
)

target_compile_definitions(arena-backend-llvm
    PRIVATE
        ${LLVM_DEFINITIONS}
)

target_compile_features(arena-backend-llvm PUBLIC cxx_std_17)

# Map LLVM components to libraries
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(arena-backend-llvm PRIVATE ${llvm_libs})
