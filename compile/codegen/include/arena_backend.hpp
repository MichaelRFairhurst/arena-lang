#ifndef ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP
#define ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP

#include <string>
#include <filesystem>
#include "resolve/expressions.hpp"

namespace arena::backend {
    enum class OptimizationLevel { None, Debug, Performance, Aggressive };

    struct BackendOptions {
        bool validate_ir = false;
        bool print_ir = false;
        OptimizationLevel optimization_level_enum = OptimizationLevel::None;
        std::filesystem::path output_path;
    };

    struct ResolvedCompilationUnit {
        std::filesystem::path source_path;
        const arena::sema::ResolvedExpressionsResult *resolved;
        const arena::sema::FunctionTable *ftable;
        const arena::sema::TypeTable *ttable;
    };

    /**
     * The actual backend implementation for the Arena compiler, provided at link time.
     *
     * This can be used to generate LLVM IR, or other backends in the future. At the moment, exactly
     * one backend is required, and only an LLVM backend is implemented.
     */
    void emit_impl(const std::vector<ResolvedCompilationUnit> &compilation_units,
                          const BackendOptions &options);
} // namespace arena::backend

#endif // ARENA_INCLUDE_CODEGEN_ARENA_BACKEND_HPP