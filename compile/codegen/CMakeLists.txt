# Codegen - backend-agnostic IR and backend API
add_library(arena-codegen INTERFACE)

target_include_directories(arena-codegen
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# This will be header-only for now (backend API)
# Add source files when you create the IR implementation
