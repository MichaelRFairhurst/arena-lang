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

        const auto &resolved_calls = engine.execute(arena::sema::ResolvedCallsQuery{file});
        //std::cout << "Resolved Exprs:\n";
        //auto resolved_exprs = resolved_calls.get_resolved_decls();
        //for (const auto &expr : resolved_exprs) {
        //    std::cout << expr->to_string() << "\n";
        //}
        auto rerrors = resolved_calls.get_errors();
        std::cout << "Resolve Errors:\n";
        for (const auto &error : rerrors) {
            std::cout << "Error: " << error.message << " at node: " << error.node->begin()->text << "\n";
        }

        const auto &typechecked = engine.execute(arena::sema::TypecheckedFileQuery{file});
        //std::cout << "Resolved Exprs:\n";
        //auto resolved_exprs = resolved_calls.get_resolved_decls();
        //for (const auto &expr : resolved_exprs) {
        //    std::cout << expr->to_string() << "\n";
        //}
        auto terrors = typechecked.get_errors();
        std::cout << "Type Errors:\n";
        for (const auto &error : terrors) {
            std::cout << "Error: " << error.message << " at node: " << error.node->begin()->text << "\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}