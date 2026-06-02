#ifndef ARENA_INCLUDE_QUERY_QUERIES_HPP
#define ARENA_INCLUDE_QUERY_QUERIES_HPP

#include <filesystem>
#include <variant>
#include <unordered_map>
#include <vector>
#include "parse/parse.hpp"
#include "resolve/symbols.hpp"
#include "resolve/expressions.hpp"

namespace arena::sema {

    class QueryCache;
    class QueryEngineContext;

    enum class QueryRefreshType {
        // A query with no dependencies may have this refresh type, and it will always re-run when
        // requested.
        AlwaysRefresh,
        // A query with no dependencies may have this refresh type, and will only re-run after
        // invalidated by an external event.
        RefreshOnDemand,
        // A query with dependencies will have this refresh type, and will only re-run when one of
        // its dependencies has changed.
        RefreshOnDependentChange,
    };

    template <typename Tag, typename Input, typename Result, QueryRefreshType refresh_type_v>
    struct QueryBase {
        using TagType = Tag;
        using ResultType = Result;
        using InputType = Input;
        using CacheType = std::unordered_map<Input, ResultType>;
        static constexpr QueryRefreshType refresh_type = refresh_type_v;

        QueryBase(Input input) : input(input) {}
        Input input;

        template <typename Self>
        bool operator==(const Self &other) const {
            return input == other.input;
        }
    };

    using SourceContentsQuery = QueryBase<struct SourceContentsQueryTag,
                                          std::filesystem::path,
                                          std::string,
                                          // TODO: use file system listener or timestamp to
                                          // determine when source contents have changed.
                                          QueryRefreshType::AlwaysRefresh>;

    using ParseQuery = QueryBase<struct ParseQueryTag,
                                 std::filesystem::path,
                                 arena::parse::ParseResult,
                                 QueryRefreshType::RefreshOnDependentChange>;

    using ImportedPathsQuery = QueryBase<struct ImportedPathsQueryTag,
                                         std::filesystem::path,
                                         std::vector<std::filesystem::path>,
                                         QueryRefreshType::RefreshOnDependentChange>;

    using FunctionTableQuery = QueryBase<struct FunctionTableQueryTag,
                                         std::filesystem::path,
                                         FunctionTable,
                                         QueryRefreshType::RefreshOnDependentChange>;

    using TypeTableQuery = QueryBase<struct TypeTableQueryTag,
                                     std::filesystem::path,
                                     TypeTable,
                                     QueryRefreshType::RefreshOnDependentChange>;

    using FunctionIdsQuery = QueryBase<struct FunctionIdsQueryTag,
                                       std::filesystem::path,
                                       FunctionSymbolSet,
                                       QueryRefreshType::RefreshOnDependentChange>;

    using AvailableFunctionIdsQuery = QueryBase<struct AvailableFunctionIdsQueryTag,
                                                std::filesystem::path,
                                                FunctionSymbolSet,
                                                QueryRefreshType::RefreshOnDependentChange>;

    using TypeIdsQuery = QueryBase<struct TypeIdsQueryTag,
                                       std::filesystem::path,
                                       TypeSymbolSet,
                                       QueryRefreshType::RefreshOnDependentChange>;

    using AvailableTypeIdsQuery = QueryBase<struct AvailableTypeIdsQueryTag,
                                       std::filesystem::path,
                                       TypeSymbolSet,
                                       QueryRefreshType::RefreshOnDependentChange>;

    using AvailableFunctionsTableQuery = QueryBase<struct AvailableFunctionsTableQueryTag,
                                                   std::filesystem::path,
                                                   FunctionTable,
                                                   QueryRefreshType::RefreshOnDependentChange>;

    using AvailableTypesTableQuery = QueryBase<struct AvailableTypesTableQueryTag,
                                                   std::filesystem::path,
                                                   TypeTable,
                                                   QueryRefreshType::RefreshOnDependentChange>;

    using ResolvedCallsQuery = QueryBase<struct ResolvedCallsQueryTag,
                                         std::filesystem::path,
                                         ResolvedExpressionsResult,
                                         QueryRefreshType::RefreshOnDependentChange>;

    using TypecheckedFileQuery = QueryBase<struct TypecheckedFileQueryTag,
                                           std::filesystem::path,
                                           ResolvedExpressionsResult,
                                           QueryRefreshType::RefreshOnDependentChange>;


    std::string compute_query_result(const QueryEngineContext &ctx, SourceContentsQuery query);
    arena::parse::ParseResult compute_query_result(const QueryEngineContext &ctx, ParseQuery query);
    std::vector<std::filesystem::path> compute_query_result(const QueryEngineContext &ctx,
                                                            ImportedPathsQuery query);
    FunctionTable compute_query_result(const QueryEngineContext &ctx, FunctionTableQuery query);
    TypeTable compute_query_result(const QueryEngineContext &ctx, TypeTableQuery query);
    FunctionSymbolSet compute_query_result(const QueryEngineContext &ctx, FunctionIdsQuery query);
    FunctionSymbolSet compute_query_result(const QueryEngineContext &ctx,
                                           AvailableFunctionIdsQuery query);
    TypeSymbolSet compute_query_result(const QueryEngineContext &ctx, TypeIdsQuery query);
    TypeSymbolSet compute_query_result(const QueryEngineContext &ctx, AvailableTypeIdsQuery query);
    FunctionTable compute_query_result(const QueryEngineContext &ctx,
                                       AvailableFunctionsTableQuery query);
    TypeTable compute_query_result(const QueryEngineContext &ctx,
                                       AvailableTypesTableQuery query);
    arena::sema::ResolvedExpressionsResult compute_query_result(const QueryEngineContext &ctx,
                                                                ResolvedCallsQuery query);
    arena::sema::ResolvedExpressionsResult compute_query_result(const QueryEngineContext &ctx,
                                                                TypecheckedFileQuery query);

    struct QueryCache {
        SourceContentsQuery::CacheType source_contents_cache;
        ParseQuery::CacheType parse_cache;
        ImportedPathsQuery::CacheType imported_paths_cache;
        FunctionTableQuery::CacheType function_table_cache;
        TypeTableQuery::CacheType type_table_cache;
        FunctionIdsQuery::CacheType function_ids_cache;
        AvailableFunctionIdsQuery::CacheType available_function_ids_cache;
        TypeIdsQuery::CacheType type_ids_cache;
        AvailableTypeIdsQuery::CacheType available_type_ids_cache;
        AvailableFunctionsTableQuery::CacheType available_functions_table_cache;
        AvailableTypesTableQuery::CacheType available_types_table_cache;
        ResolvedCallsQuery::CacheType resolved_calls_cache;
        TypecheckedFileQuery::CacheType typechecked_file_cache;

        template <typename QueryType>
        typename QueryType::CacheType &get_cache() {
            if constexpr (std::is_same_v<QueryType, SourceContentsQuery>) {
                return source_contents_cache;
            } else if constexpr (std::is_same_v<QueryType, ParseQuery>) {
                return parse_cache;
            } else if constexpr (std::is_same_v<QueryType, ImportedPathsQuery>) {
                return imported_paths_cache;
            } else if constexpr (std::is_same_v<QueryType, FunctionTableQuery>) {
                return function_table_cache;
            } else if constexpr (std::is_same_v<QueryType, TypeTableQuery>) {
                return type_table_cache;
            } else if constexpr (std::is_same_v<QueryType, FunctionIdsQuery>) {
                return function_ids_cache;
            } else if constexpr (std::is_same_v<QueryType, AvailableFunctionIdsQuery>) {
                return available_function_ids_cache;
            } else if constexpr (std::is_same_v<QueryType, TypeIdsQuery>) {
                return type_ids_cache;
            } else if constexpr (std::is_same_v<QueryType, AvailableTypeIdsQuery>) {
                return available_type_ids_cache;
            } else if constexpr (std::is_same_v<QueryType, AvailableFunctionsTableQuery>) {
                return available_functions_table_cache;
            } else if constexpr (std::is_same_v<QueryType, AvailableTypesTableQuery>) {
                return available_types_table_cache;
            } else if constexpr (std::is_same_v<QueryType, ResolvedCallsQuery>) {
                return resolved_calls_cache;
            } else if constexpr (std::is_same_v<QueryType, TypecheckedFileQuery>) {
                return typechecked_file_cache;
            } else {
                static_assert(false && sizeof(QueryType), "Unsupported query type");
            }
        }

        template <typename T>
        std::optional<std::reference_wrapper<const typename T::ResultType>> get_cached(
            const T &query) {
            auto &cache = get_cache<T>();
            auto it = cache.find(query.input);
            if (it != cache.end()) {
                return std::optional(std::cref(it->second));
            }
            return std::nullopt;
        }

        template <typename T>
        void update_cache(const T &query, typename T::ResultType result) {
            auto &cache = get_cache<T>();
            cache[query.input] = std::move(result);
        }
    };

    using Query = std::variant<SourceContentsQuery,
                               ParseQuery,
                               ImportedPathsQuery,
                               FunctionTableQuery,
                               TypeTableQuery,
                               FunctionIdsQuery,
                               AvailableFunctionIdsQuery,
                               AvailableFunctionsTableQuery,
                               AvailableTypesTableQuery,
                               TypeIdsQuery,
                               AvailableTypeIdsQuery,
                               ResolvedCallsQuery,
                               TypecheckedFileQuery>;

    template <typename QueryType>
    struct QueryHashBase {
        size_t operator()(const QueryType &query) const {
            return std::hash<typename QueryType::InputType>()(query.input);
        }
    };
} // namespace arena::sema

template <>
struct std::hash<arena::sema::SourceContentsQuery>
    : arena::sema::QueryHashBase<arena::sema::SourceContentsQuery> {};

template <>
struct std::hash<arena::sema::ParseQuery> : arena::sema::QueryHashBase<arena::sema::ParseQuery> {};

template <>
struct std::hash<arena::sema::ImportedPathsQuery>
    : arena::sema::QueryHashBase<arena::sema::ImportedPathsQuery> {};

template <>
struct std::hash<arena::sema::FunctionTableQuery>
    : arena::sema::QueryHashBase<arena::sema::FunctionTableQuery> {};

template <>
struct std::hash<arena::sema::TypeTableQuery>
    : arena::sema::QueryHashBase<arena::sema::TypeTableQuery> {};

template <>
struct std::hash<arena::sema::FunctionIdsQuery>
    : arena::sema::QueryHashBase<arena::sema::FunctionIdsQuery> {};

template <>
struct std::hash<arena::sema::AvailableFunctionIdsQuery>
    : arena::sema::QueryHashBase<arena::sema::AvailableFunctionIdsQuery> {};

template <>
struct std::hash<arena::sema::TypeIdsQuery>
    : arena::sema::QueryHashBase<arena::sema::TypeIdsQuery> {};

template <>
struct std::hash<arena::sema::AvailableTypeIdsQuery>
    : arena::sema::QueryHashBase<arena::sema::AvailableTypeIdsQuery> {};

template <>
struct std::hash<arena::sema::AvailableFunctionsTableQuery>
    : arena::sema::QueryHashBase<arena::sema::AvailableFunctionsTableQuery> {};

template <>
struct std::hash<arena::sema::AvailableTypesTableQuery>
    : arena::sema::QueryHashBase<arena::sema::AvailableTypesTableQuery> {};

template <>
struct std::hash<arena::sema::ResolvedCallsQuery>
    : arena::sema::QueryHashBase<arena::sema::ResolvedCallsQuery> {};

template <>
struct std::hash<arena::sema::TypecheckedFileQuery>
    : arena::sema::QueryHashBase<arena::sema::TypecheckedFileQuery> {};

#endif // ARENA_INCLUDE_QUERY_QUERIES_HPP