#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <boost/program_options.hpp>

#include "query/engine.hpp"
#include "arena_backend.hpp"

namespace po = boost::program_options;

namespace {
    int command_check(int argc, char **argv) {
        std::vector<std::filesystem::path> source_files;
        bool keep_alive = false;
        bool verbose = false;

        po::options_description desc("Usage: arena check [options] <sources>");
        // clang-format off
        desc.add_options()
            ("help,h", "Show this help message")
            ("keep-alive,k", po::bool_switch(&keep_alive), "Continuous execution mode")
            ("verbose,v", po::bool_switch(&verbose), "Enable verbose output")
            ("sources",
            po::value<std::vector<std::filesystem::path>>(&source_files),
            "Source files to load");
        // clang-format on

        // Define positional arguments (source files)
        po::positional_options_description pos_desc;
        pos_desc.add("sources", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        bool has_errors = false;
        arena::sema::QueryEngine engine;
        do {
            for (const auto &file : source_files) {
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

    int command_compile(int argc, char **argv) {
        std::vector<std::filesystem::path> source_files;
        arena::backend::BackendOptions backend_options;
        po::options_description desc("Usage: arena compile [options] <sources>");
        // clang-format off
        desc.add_options()
            ("help,h", "Show this help message")
            ("sources",
             po::value<std::vector<std::filesystem::path>>(&source_files),
             "Source files to load")
            ("validate-ir",
             po::bool_switch(&backend_options.validate_ir),
             "Validate the generated IR before assembling")
            ("print-ir",
             po::bool_switch(&backend_options.print_ir),
             "Print the generated IR before assembling")
            ("output-path,o",
             po::value<std::filesystem::path>(&backend_options.output_path)->default_value("a.out"),
             "Output path for the generated backend output");
        // clang-format on

        // Define positional arguments (source files)
        po::positional_options_description pos_desc;
        pos_desc.add("sources", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        bool has_errors = false;
        arena::sema::QueryEngine engine;
        std::vector<arena::backend::ResolvedCompilationUnit> compilation_units;

        for (const auto &file : source_files) {
            const auto errors = engine.execute(arena::sema::RenderedErrorsQuery{file});

            if (!errors.empty()) {
                std::cout << errors << "\n";
                has_errors = true;
            } else {
                const auto &typechecked = engine.execute(arena::sema::TypecheckedFileQuery{file});
                const auto &ftable =
                    engine.execute(arena::sema::AvailableFunctionsTableQuery{file});
                const auto &ttable = engine.execute(arena::sema::AvailableTypesTableQuery{file});

                compilation_units.push_back(arena::backend::ResolvedCompilationUnit{
                    .source_path = file,
                    .resolved = &typechecked,
                    .ftable = &ftable,
                    .ttable = &ttable,
                });
            }
        }

        if (!has_errors) {
            arena::backend::emit_impl(compilation_units, backend_options);
        }

        return has_errors ? 1 : 0;
    }

    void list_valid_subcommands() {
        std::cout << "  arena compile ...           Compile arena source files\n";
        std::cout << "  arena check ...             Typecheck arena source files\n";
        std::cout << "  arena version ...           Show version information\n";
        std::cout << "  arena help ...              Show help information\n";
    }
} // namespace

int main(int argc, char **argv) {
    if (argc == 1) {
        // clang-format off
        std::cout << " ,-''&''-.\n";
        std::cout << " | |`*`| | "  << "▀▌▛▘█▌▛▌▀▌\n";
        std::cout << " l  \\:/  j " << "█▌▌ ▙▖▌▌█▌v0.0.1\n";
        std::cout << "  \\  '  /\n";
        std::cout << "   '._.'   "  << "       arena cli\n";
        // clang-format on
        std::cout << "\n";
        std::cout << "Please provide a subcommand:\n";
        list_valid_subcommands();
        return 1;
    }

    if (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--version" ||
        std::string(argv[1]) == "version") {
        std::cout << "arena version 0.0.1\n";
        return 0;
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "help") {
        std::cout << "arena cli\n";
        std::cout << "Available subcommands:\n";
        list_valid_subcommands();
        return 0;
    }

    if (std::string(argv[1]) == "check") {
        return command_check(argc - 1, argv + 1);
    }
    if (std::string(argv[1]) == "compile") {
        return command_compile(argc - 1, argv + 1);
    }

    std::cout << "Unknown subcommand: '" << argv[1] << "'\n";
    std::cout << "Use 'arena --help' to see available subcommands.\n";
    return 1;
}