# Frontend library - parsing and semantic analysis
add_library(arena-frontend
    src/parse/parse.cpp
    src/query/engine.cpp
    src/query/queries.cpp
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
