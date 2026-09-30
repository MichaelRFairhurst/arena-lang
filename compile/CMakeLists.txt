# Compiler components
add_subdirectory(frontend)
add_subdirectory(codegen)

if(ARENA_ENABLE_LLVM)
    add_subdirectory(llvm)
endif()

add_subdirectory(arena-bin)

# Custom test runners
file(GLOB_RECURSE STATIC_TEST_FILES CONFIGURE_DEPENDS "static_lang_tests/**/*.rna")
file(GLOB_RECURSE EXECUTED_TEST_FILES CONFIGURE_DEPENDS "executed_lang_tests/**/*.rna")

foreach(TEST_FILE ${STATIC_TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)
    
    file(RELATIVE_PATH TEST_FILE_REL ${CMAKE_SOURCE_DIR} ${TEST_FILE})

    # Language tests that run the compiler and verify the output.
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_SOURCE_DIR}/compile/static_lang_tests/run_test.sh 
                ${TEST_FILE_REL} 
                $<TARGET_FILE:arena-bin>
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()

set(TEST_OPTIMIZATION_LEVELS "-Onone" "-Odebug" "-Operformance" "-Oaggressive" "-Osize" "-Ominify")

foreach(TEST_FILE ${EXECUTED_TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)
    
    file(RELATIVE_PATH TEST_FILE_REL ${CMAKE_SOURCE_DIR} ${TEST_FILE})

    # Language tests that execute a program and verify the output.
    foreach(OPT_LEVEL ${TEST_OPTIMIZATION_LEVELS})
        add_test(
            NAME ${TEST_NAME}${OPT_LEVEL}
            COMMAND ${CMAKE_SOURCE_DIR}/compile/executed_lang_tests/run_test.sh 
                    ${TEST_FILE_REL} 
                    $<TARGET_FILE:arena-bin>
                    ${OPT_LEVEL}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endforeach()
endforeach()