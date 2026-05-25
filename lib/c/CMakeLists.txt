# Arena C runtime library
add_library(arena-rt STATIC
    src/arena.c
)

target_include_directories(arena-rt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install targets
install(TARGETS arena-rt
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
