#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include "query/engine.hpp"

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    } else if (argc > 2) {
        std::cerr << "Error: Only one file at a time is currently supported.\n";
        return 1;
    }

    // create a filesystem path
    std::filesystem::path file(argv[1]);

    arena::sema::QueryEngine engine;
    while (true) {
        const auto &ast = engine.execute(arena::sema::ParseQuery{file});
        auto decl = ast.declarations.at(0);
        std::cout << "Parsed AST:\n";
        for (const auto &decl : ast.declarations) {
            std::cout << decl->to_string() << "\n";
        }

        arena::ast::Token *current = decl->begin();
        std::cout << "Tokens: ";
        while (current != nullptr) {
            std::cout << current->text;
            current = current->next;
        }
        std::cout << "\n";

        const auto ids = engine.execute(arena::sema::FunctionIdsQuery{file});
        std::cout << "Function IDs: ";
        for (const auto &id : ids) {
            std::cout << id.f_id << " ";
        }
        std::cout << "\n";

        const auto imports = engine.execute(arena::sema::ImportedPathsQuery{file});
        std::cout << "Imports: ";
        for (const auto &import : imports) {
            std::cout << import << " ";
        }
        std::cout << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}