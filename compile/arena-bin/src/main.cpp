#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <boost/program_options.hpp>
#include <sys/wait.h>

#include "query/engine.hpp"
#include "arena_backend.hpp"

namespace po = boost::program_options;

namespace arena::backend {
    std::istream &operator>>(std::istream &in, arena::backend::OptimizationLevel &level) {
        std::string token;
        in >> token;
        if (token == "none") {
            level = arena::backend::OptimizationLevel::None;
        } else if (token == "d" || token == "debug") {
            level = arena::backend::OptimizationLevel::Debug;
        } else if (token == "performance") {
            level = arena::backend::OptimizationLevel::Performance;
        } else if (token == "aggressive") {
            level = arena::backend::OptimizationLevel::Aggressive;
        } else if (token == "size") {
            level = arena::backend::OptimizationLevel::Size;
        } else if (token == "minify") {
            level = arena::backend::OptimizationLevel::Minify;
        } else {
            in.setstate(std::ios::failbit);
        }
        return in;
    }
}


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

    std::pair<std::string, std::string> optimization_level_parser(const std::string &opt) {
        if (opt.find("-O") == 0) {
            return {"optimization-level", opt.substr(2)};
        }
        return {"", ""};
    }

    po::options_description compile_options(arena::backend::BackendOptions &backend_options) {
        po::options_description options;
        // clang-format off
        options.add_options()
            ("help,h", "Show this help message")
            ("validate-ir",
             po::bool_switch(&backend_options.validate_ir),
             "Validate the generated IR before assembling")
            ("print-ir",
             po::bool_switch(&backend_options.print_ir),
             "Print the generated IR before assembling")
            ("optimization-level",
             po::value<arena::backend::OptimizationLevel>(&backend_options.optimization_level),
             "Set the optimization level for the backend");
        // clang-format on
        return options;
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
            ("output-path,o",
             po::value<std::filesystem::path>(&backend_options.output_path)->default_value("a.out"),
             "Output path for the generated backend output");
        // clang-format on

        desc.add(compile_options(backend_options));

        // Define positional arguments (source files)
        po::positional_options_description pos_desc;
        pos_desc.add("sources", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(pos_desc)
                      .extra_parser(optimization_level_parser)
                      .run(),
                  vm);
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

    int command_run(int argc, char **argv) {
        std::vector<std::filesystem::path> source_files;
        arena::backend::BackendOptions backend_options;
        po::options_description desc("Usage: arena run [options] <sources>");

        std::filesystem::path cache_dir = std::filesystem::current_path() / ".arena";
        bool use_os_cache = false;

        // clang-format off
        desc.add_options()
            ("help,h", "Show this help message")
            ("sources",
             po::value<std::vector<std::filesystem::path>>(&source_files),
             "Source files of the arena program")
            ("cache-dir",
             po::value<std::filesystem::path>(&cache_dir),
             "Directory to use for caching build objects. Defaults to `.arena/` in the current directory.")
            ("use-os-cache",
             po::bool_switch(&use_os_cache),
             "Use the OS cache for build objects");
        // clang-format on

        desc.add(compile_options(backend_options));

        // Define positional arguments (source files)
        po::positional_options_description pos_desc;
        pos_desc.add("sources", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(pos_desc)
                      .extra_parser(optimization_level_parser)
                      .run(),
                  vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        if (vm.count("use-os-cache") && vm.count("cache-dir")) {
            std::cerr << "Error: cannot use both --use-os-cache and --cache-dir options "
                         "simultaneously.\n";
            return 1;
        }

        std::cout << "Compiling..." << std::flush;
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

        if (has_errors) {
            return 1;
        }

        if (use_os_cache) {
            cache_dir = std::filesystem::temp_directory_path();
        } else if (!std::filesystem::exists(cache_dir)) {
            std::filesystem::create_directories(cache_dir);
        }

        if (!std::filesystem::is_directory(cache_dir)) {
            std::cerr << "Error: cache_dir is not a directory: " << cache_dir << "\n";
            return 2;
        }

        backend_options.output_path = cache_dir / "arena-bin.o";
        std::filesystem::path bin_path = cache_dir / "arena-bin";

        arena::backend::emit_impl(compilation_units, backend_options);
        std::system(
            ("cc " + backend_options.output_path.string() + " -o " + bin_path.string()).c_str());

        std::cout << "done, now running.\n" << std::flush;
        int status = std::system(bin_path.string().c_str());
        if (status == -1) {
            std::cerr << "Error: failed to execute the program.\n";
            return 1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 1;
        }
    }

    void list_valid_subcommands() {
        std::cout << "  arena compile ...           Compile arena source files\n";
        std::cout << "  arena run ...               Run an arena program from source\n";
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
    } else if (std::string(argv[1]) == "compile") {
        return command_compile(argc - 1, argv + 1);
    } else if (std::string(argv[1]) == "run") {
        return command_run(argc - 1, argv + 1);
    }

    std::cout << "Unknown subcommand: '" << argv[1] << "'\n";
    std::cout << "Use 'arena --help' to see available subcommands.\n";
    return 1;
}