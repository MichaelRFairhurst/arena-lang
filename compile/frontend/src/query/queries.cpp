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
    FunctionTableBuilder builder(ctx.get_function_registry(), ctx.get_type_registry(), ttable);
    return builder.build(ast.declarations);
};

TypeTable arena::sema::compute_query_result(const QueryEngineContext &ctx, TypeTableQuery query) {
    const auto &path = query.input;
    auto &ast = ctx.run_query(ParseQuery{path});
    TypeTable ttable = TypeTable::builtin_type_table(ctx.get_type_registry());
    TypeTableBuilder builder(&ctx.get_type_registry());
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

TypeSymbolSet arena::sema::compute_query_result(const QueryEngineContext &ctx, TypeIdsQuery query) {
    const auto &path = query.input;
    auto &ttable = ctx.run_query(TypeTableQuery{path});
    return TypeSymbolSet(&ttable, ctx.get_type_registry());
};

TypeSymbolSet arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                AvailableTypeIdsQuery query) {
    const auto &path = query.input;
    auto &imports = ctx.run_query(ImportedPathsQuery{path});

    TypeSymbolSet all_types{ctx.get_type_registry()};
    for (const auto &import : imports) {
        auto &types = ctx.run_query(TypeIdsQuery{import});
        all_types.import(types);
    }
    auto &my_types = ctx.run_query(TypeIdsQuery{path});
    all_types.import(my_types);

    return all_types;
};

FunctionTable arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                                AvailableFunctionsTableQuery query) {
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

TypeTable arena::sema::compute_query_result(const QueryEngineContext &ctx,
                                            AvailableTypesTableQuery query) {
    const auto &path = query.input;
    auto &imports = ctx.run_query(ImportedPathsQuery{path});

    TypeTable ttable(ctx.get_type_registry());

    for (const auto &import : imports) {
        auto &imported_ttable = ctx.run_query(TypeTableQuery{import});
        ttable.import(imported_ttable);
    }
    auto &my_ttable = ctx.run_query(TypeTableQuery{path});
    ttable.import(my_ttable);

    return ttable;
}

arena::sema::ResolvedExpressionsResult arena::sema::compute_query_result(
    const QueryEngineContext &ctx, ResolvedCallsQuery query) {
    const auto &path = query.input;
    auto my_ftable = ctx.run_query(FunctionTableQuery{path}); // For lifetime information
    auto all_fsymbols = ctx.run_query(AvailableFunctionIdsQuery{path});
    auto all_tsymbols = ctx.run_query(AvailableTypeIdsQuery{path});

    auto &ast = ctx.run_query(ParseQuery{path});
    ExpressionResolver resolver;
    return resolver.resolve(ast.declarations,
                            &all_fsymbols,
                            &my_ftable,
                            &all_tsymbols,
                            &ctx.get_type_registry());
}

arena::sema::ResolvedExpressionsResult arena::sema::compute_query_result(
    const QueryEngineContext &ctx, TypecheckedFileQuery query) {
    const auto &path = query.input;
    const FunctionTable &ftable = ctx.run_query(AvailableFunctionsTableQuery{path});
    const TypeTable &ttable = ctx.run_query(AvailableTypesTableQuery{path});
    auto &resolved_calls = ctx.run_query(ResolvedCallsQuery{path});

    auto &ast = ctx.run_query(ParseQuery{path});
    TypeChecker typechecker(ftable, ttable);

    return typechecker.type_check(resolved_calls.get_resolved_decls(),
                                  resolved_calls.get_resolved_variables());
}