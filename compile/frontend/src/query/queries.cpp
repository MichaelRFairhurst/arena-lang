#include "query/queries.hpp"
#include "query/engine.hpp"
#include "resolve/imports.hpp"
#include "types/typecheck.hpp"
#include <fstream>

using namespace arena::sema;

std::string arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                              SourceContentsQuery query) {
    const auto &path = query.input;
    // For simplicity, we will just read the file contents directly. In a real
    // implementation, we would want to use a file system listener or timestamp to
    // determine when source contents have changed.
    std::ifstream input_file(path);
    if (!input_file) {
        throw std::runtime_error("Error: Could not open file: " + path.string());
    }
    return std::string((std::istreambuf_iterator<char>(input_file)),
                       std::istreambuf_iterator<char>());
}

arena::parse::ParseResult arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                            ParseQuery query) {
    const auto &path = query.input;
    auto &source_contents = ctx.run_query(SourceContentsQuery{path});
    auto result = arena::parse::parse(source_contents);
    return result;
}

std::vector<std::filesystem::path> arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                                     ImportedPathsQuery query) {
    const auto &path = query.input;
    auto &ast = ctx.run_query(ParseQuery{path});
    ImportResolver resolver;
    return resolver.resolve_imports(ast.declarations, path);
};

FunctionTable arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                          FunctionTableQuery query) {
    const auto &path = query.input;
    auto &ast = ctx.run_query(ParseQuery{path});
    TypeTable ttable = TypeTable::builtin_type_table(ctx.get_type_registry());
    FunctionTableBuilder builder(ctx.get_function_registry(), ttable);
    return builder.build(ast.declarations);
};

FunctionSymbolSet arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                          FunctionIdsQuery query) {
    const auto &path = query.input;
    auto &ftable = ctx.run_query(FunctionTableQuery{path});
    return FunctionSymbolSet(&ftable);
};

FunctionSymbolSet arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                          AvailableFunctionIdsQuery query) {
    const auto &path = query.input;
    auto &imports = ctx.run_query(ImportedPathsQuery{path});

    FunctionSymbolSet all_functions;
    for (const auto &import : imports) {
        auto &functions = ctx.run_query(FunctionIdsQuery{import});
        all_functions.import(functions);
    }
    auto &my_function_ids = ctx.run_query(FunctionIdsQuery{path});
    all_functions.import(my_function_ids);

    return all_functions;
};

FunctionTable arena::sema::compute_query_result(
    const QueryEngineContext &ctx, AvailableFunctionsTableQuery query) {
    const auto &path = query.input;
    auto &imports = ctx.run_query(ImportedPathsQuery{path});

    FunctionTable ftable(ctx.get_function_registry());

    for (const auto &import : imports) {
        auto &imported_ftable = ctx.run_query(FunctionTableQuery{import});
        ftable.import(imported_ftable);
    }
    auto &my_ftable = ctx.run_query(FunctionTableQuery{path});
    ftable.import(my_ftable);

    return ftable;
}

arena::sema::ResolvedExpressionsResult arena::sema::compute_query_result(
    const QueryEngineContext &ctx, ResolvedCallsQuery query) {
    const auto &path = query.input;
    auto all_symbols = ctx.run_query(AvailableFunctionIdsQuery{path});

    auto &ast = ctx.run_query(ParseQuery{path});
    ExpressionResolver resolver;
    TypeTable ttable = TypeTable::builtin_type_table(ctx.get_type_registry());
    TypeSymbolSet ttable_symbols(&ttable);
    return resolver.resolve(ast.declarations, &all_symbols, &ttable_symbols);
}

arena::sema::ResolvedExpressionsResult arena::sema::compute_query_result(
    const QueryEngineContext &ctx, TypecheckedFileQuery query) {
    const auto &path = query.input;
    const FunctionTable &ftable = ctx.run_query(AvailableFunctionsTableQuery{path});
    auto &resolved_calls = ctx.run_query(ResolvedCallsQuery{path});

    auto &ast = ctx.run_query(ParseQuery{path});
    const TypeTable ttable = TypeTable::builtin_type_table(ctx.get_type_registry());
    TypeChecker typechecker(ftable, ttable);

    return typechecker.type_check(resolved_calls.get_resolved_decls());
}