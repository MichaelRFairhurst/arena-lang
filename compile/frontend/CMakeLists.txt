# Frontend library - parsing and semantic analysis
add_library(arena-frontend
    src/ast/token.cpp
    src/errors/line_endings.cpp
    src/errors/renderer.cpp
    src/parse/parse.cpp
    src/query/engine.cpp
    src/query/queries.cpp
    src/resolve/functions.cpp
    src/resolve/symbols.cpp
    src/resolve/expressions.cpp
    src/types/typecheck.cpp
    src/types/inference.cpp
    src/types/lifetimes.cpp
    src/types/substitute.cpp
    src/signatures/functions.cpp
    src/signatures/lifetimes.cpp
    src/signatures/types.cpp
)

target_include_directories(arena-frontend
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(arena-frontend
    PUBLIC
        arena-rt
)

target_compile_features(arena-frontend PUBLIC cxx_std_17)
