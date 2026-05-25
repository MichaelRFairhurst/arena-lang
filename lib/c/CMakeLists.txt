# Arena C runtime library
add_library(arena-rt STATIC
    src/arena.c
)

target_include_directories(arena-rt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Tests
find_package(unofficial-cunit CONFIG)
add_executable(test_arena test/test_arena.c)
target_link_libraries(test_arena PRIVATE arena-rt unofficial::cunit::cunit)

# Add test to CTest
add_test(NAME ArenaTests COMMAND test_arena)

# Install targets
install(TARGETS arena-rt
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
