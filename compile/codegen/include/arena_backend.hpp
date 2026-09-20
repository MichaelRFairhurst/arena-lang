#ifndef ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP
#define ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP

#include <string>
#include <filesystem>
#include "resolve/expressions.hpp"

namespace arena::backend {
    /**
     * The actual backend implementation for the Arena compiler, provided at link time.
     *
     * This can be used to generate LLVM IR, or other backends in the future. At the moment, exactly
     * one backend is required, and only an LLVM backend is implemented.
     */
    std::string emit_impl(const arena::sema::ResolvedExpressionsResult &resolved,
                          const arena::sema::FunctionTable &ftable,
                          const arena::sema::TypeTable &ttable,
                          const std::filesystem::path &source_path);
} // namespace arena::backend

#endif // ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP