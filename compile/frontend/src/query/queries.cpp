#include "query/queries.hpp"
#include "query/engine.hpp"
#include "resolve/imports.hpp"
#include <fstream>

using namespace arena::sema;

std::string arena::sema::compute_query_result(const QueryEngineContext &ctx, SourceContentsQuery query) {
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

arena::parse::ParseResult arena::sema::compute_query_result(const QueryEngineContext &ctx, ParseQuery query) {
    const auto &path = query.input;
    auto &source_contents = ctx.run_query(SourceContentsQuery{path});
    auto result = arena::parse::parse(source_contents);
    return result;
}

std::vector<FunctionId> arena::sema::compute_query_result(const QueryEngineContext &ctx, FunctionIdsQuery query) {
    const auto &path = query.input;
    auto &ast = ctx.run_query(ParseQuery{path});
    FunctionTableBuilder builder(ctx.get_function_registry(), ctx.get_type_registry());
    auto ftable = builder.build(ast.declarations);
    return ftable.get_ids();
};

std::vector<std::filesystem::path> arena::sema::compute_query_result(const QueryEngineContext &ctx, ImportedPathsQuery query) {
    const auto &path = query.input;
    auto &ast = ctx.run_query(ParseQuery{path});
    ImportResolver resolver;
    return resolver.resolve_imports(ast.declarations, path);
};

arena::sema::ResolvedExpressionsResult arena::sema::compute_query_result(const QueryEngineContext &ctx, ResolvedCallsQuery query) {
    const auto &path = query.input;
    auto &imports = ctx.run_query(ImportedPathsQuery{path});

    std::vector<FunctionId> all_function_ids;
    for (const auto &import : imports) {
        auto &function_ids = ctx.run_query(FunctionIdsQuery{import});
        all_function_ids.insert(all_function_ids.end(), function_ids.begin(), function_ids.end());
    }
    auto &my_function_ids = ctx.run_query(FunctionIdsQuery{path});
    all_function_ids.insert(all_function_ids.end(), my_function_ids.begin(), my_function_ids.end());

    FunctionTable ftable(ctx.get_function_registry());
    for (const auto &func : all_function_ids) {
        auto symbol = ctx.get_function_registry().get_function_symbol(func);
        ftable.add_function(symbol, {func, symbol, {}, std::nullopt}, nullptr);
    }

    auto &ast = ctx.run_query(ParseQuery{path});
    ExpressionResolver resolver;
    return resolver.resolve(ast.declarations, ftable);
};