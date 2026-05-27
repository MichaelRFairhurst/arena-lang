#ifndef ARENA_INCLUDE_RESOLVE_IMPORTS_HPP
#define ARENA_INCLUDE_RESOLVE_IMPORTS_HPP

#include <vector>
#include <filesystem>
#include <unordered_map>
#include "ast/declarations.hpp"

namespace arena::sema {
    class ImportResolver {
    public:
        std::vector<std::filesystem::path> resolve_imports(
            const std::vector<ast::Declaration *> &declarations,
            std::filesystem::path file_path) const {
            std::vector<std::filesystem::path> imports;
            std::filesystem::path file_dir = file_path.parent_path();
            std::string ext = file_path.extension().string();
            for (const auto &decl : declarations) {
                if (auto importDecl = dynamic_cast<ast::ImportDeclaration *>(decl)) {
                    imports.push_back(file_dir / (std::string(importDecl->get_path()) + ext));
                }
            }
            return imports;
        }
    };
}; // namespace arena::sema

#endif // ARENA_INCLUDE_RESOLVE_IMPORTS_HPP