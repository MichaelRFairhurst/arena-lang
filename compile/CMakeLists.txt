# Compiler components
add_subdirectory(frontend)
add_subdirectory(codegen)

if(ARENA_ENABLE_LLVM)
    add_subdirectory(llvm)
endif()

add_subdirectory(arena-bin)
