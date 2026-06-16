# Compiler components
add_subdirectory(frontend)
add_subdirectory(codegen)

if(ARENA_ENABLE_LLVM)
    add_subdirectory(llvm)
endif()

add_subdirectory(arena-bin)

# Custom test runner
file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS "lang_tests/**/*.rna")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)
    
    file(RELATIVE_PATH TEST_FILE_REL ${CMAKE_SOURCE_DIR} ${TEST_FILE})

    # Language tests that run the compiler and verify the output.
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_SOURCE_DIR}/compile/lang_tests/run_test.sh 
                ${TEST_FILE_REL} 
                $<TARGET_FILE:arena-bin>
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()