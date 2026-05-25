#include "parse/parse.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

int main(int argc, char **argv) {
    /// auto ast = arena::parse::parse("a.*.*.b.& * 3.as(int)");
    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    } else if (argc > 2) {
        std::cerr << "Error: Only one file at a time is currently supported.\n";
        return 1;
    }

    // create a filesystem path
    std::filesystem::path file(argv[1]);
    if (!std::filesystem::exists(file)) {
        std::cerr << "Error: File does not exist: " << file << "\n";
        return 1;
    }

    // read the file into a string
    std::ifstream input_file(file);
    if (!input_file) {
        std::cerr << "Error: Could not open file: " << file << "\n";
        return 1;
    }

    // TODO: support larger files
    std::string input((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());

    auto ast = arena::parse::parse(input);
    std::cout << "Parsed AST: " << ast->to_string() << std::endl;

    arena::ast::Token *current = ast->begin();
    std::cout << "Tokens: ";
    while (current != nullptr) {
        std::cout << current->text;
        current = current->next;
    }
    std::cout << "\n";
}