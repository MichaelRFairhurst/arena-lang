#ifndef ARENA_INCLUDE_ERRORS_RENDERER_HPP
#define ARENA_INCLUDE_ERRORS_RENDERER_HPP

#include "errors/errors.hpp"
#include <filesystem>

namespace arena::sema {
    class QueryEngineContext;
}

namespace arena::error {

    class CliRenderer {
    public:
        std::string render(std::filesystem::path,
                           const std::vector<Error> &errors,
                           const sema::QueryEngineContext &context);
    };
} // namespace arena::error


#endif // ARENA_INCLUDE_ERRORS_RENDERER_HPP