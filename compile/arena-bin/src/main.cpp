#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include "query/engine.hpp"

int main(int argc, char **argv) {
    std::vector<std::filesystem::path> files;
    bool keep_alive = false;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--keep-alive") {
            keep_alive = true;
            continue;
        }
        if (arg == "--verbose") {
            verbose = true;
            continue;
        }

        std::filesystem::path file(argv[i]);
        files.push_back(file);
    }

    bool has_errors = false;
    arena::sema::QueryEngine engine;
    do {
        for (const auto &file : files) {
            if (verbose) {
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
            }

            const auto errors = engine.execute(arena::sema::RenderedErrorsQuery{file});

            if (errors.empty()) {
                std::cout << file << ": no errors. \n";
            } else {
                std::cout << errors << "\n";
                has_errors = true;
            }
        }

        if (keep_alive) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } while (keep_alive);

    return has_errors ? 1 : 0;
}