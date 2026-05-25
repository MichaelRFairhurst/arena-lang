# Frontend library - parsing and semantic analysis
add_library(arena-frontend
    src/parse/parse.cpp
    # Add other source files as you create them:
    # src/ast/expressions.cpp
    # src/ast/statements.cpp
    # src/sema/type_checker.cpp
)

target_include_directories(arena-frontend
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(arena-frontend PUBLIC cxx_std_17)
