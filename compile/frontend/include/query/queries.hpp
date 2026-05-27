#ifndef ARENA_INCLUDE_QUERY_QUERIES_HPP
#define ARENA_INCLUDE_QUERY_QUERIES_HPP

#include <filesystem>
#include <variant>
#include <unordered_map>
#include <vector>
#include "parse/parse.hpp"
#include "resolve/symbols.hpp"

namespace arena::sema {

    class QueryEngine;

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

    struct SourceContentsQuery {
        using ResultType = std::string;
        // TODO: use file system listener or timestamp to determine when source contents have
        // changed.
        static constexpr QueryRefreshType refresh_type = QueryRefreshType::AlwaysRefresh;
        std::filesystem::path path;

        bool operator==(const SourceContentsQuery &other) const { return path == other.path; }
    };

    struct ParseQuery {
        using ResultType = ::arena::parse::ParseResult;
        static constexpr QueryRefreshType refresh_type = QueryRefreshType::RefreshOnDependentChange;
        std::filesystem::path path;

        bool operator==(const ParseQuery &other) const { return path == other.path; }
    };

    struct FunctionIdsQuery {
        using ResultType = std::vector<FunctionId>;
        static constexpr QueryRefreshType refresh_type = QueryRefreshType::RefreshOnDependentChange;
        std::filesystem::path path;

        bool operator==(const FunctionIdsQuery &other) const {
            auto [ours, theirs] =
                std::mismatch(path.begin(),
                              path.end(),
                              other.path.begin(),
                              other.path.end(),
                              [](const auto &a, const auto &b) { return a == b; });

            return ours == path.end() && theirs == other.path.end();
        }
    };


    using Query = std::variant<SourceContentsQuery, ParseQuery, FunctionIdsQuery>;
} // namespace arena::sema

template <>
struct std::hash<arena::sema::SourceContentsQuery> {
    size_t operator()(const arena::sema::SourceContentsQuery &query) const {
        return std::hash<std::filesystem::path>()(query.path);
    }
};

template <>
struct std::hash<arena::sema::ParseQuery> {
    size_t operator()(const arena::sema::ParseQuery &query) const {
        return std::hash<std::filesystem::path>()(query.path);
    }
};

template <>
struct std::hash<arena::sema::FunctionIdsQuery> {
    size_t operator()(const arena::sema::FunctionIdsQuery &query) const {
        return std::hash<std::filesystem::path>()(query.path);
    }
};

#endif // ARENA_INCLUDE_QUERY_QUERIES_HPP