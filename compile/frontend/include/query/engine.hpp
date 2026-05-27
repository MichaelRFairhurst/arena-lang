#ifndef ARENA_INCLUDE_QUERY_ENGINE_HPP
#define ARENA_INCLUDE_QUERY_ENGINE_HPP

#include <variant>
#include <cassert>
#include <unordered_map>
#include <optional>
#include <fstream>
#include <algorithm>
#include "ast/declarations.hpp"
#include "parse/parse.hpp"
#include "query/queries.hpp"
#include "resolve/functions.hpp"

namespace arena::sema {
    struct RevisionUpdate {
        size_t changed;
        size_t verified;
    };

    class GlobalContext {
    public:
        GlobalContext() = default;

        const FunctionSymbolRegistry &get_function_registry() const { return function_registry; }

        const TypeSymbolRegistry &get_type_registry() const { return type_registry; }

    private:
        FunctionSymbolRegistry function_registry;
        TypeSymbolRegistry type_registry;
    };

    class QueryEngine {
    public:
        QueryEngine() : revision() {}

        template <typename T>
        const typename T::ResultType &execute(const T &query) {
            // TODO: more intelligently update revisions.
            revision++;
            return this->execute(query, query);
        }

    private:
        std::pair<size_t, RevisionUpdate *> get_query_metadata(const Query &query) {
            auto it = query_id.find(query);
            if (it == query_id.end()) {
                // First time executing this query
                it = query_id.emplace(query, query_id.size()).first;
                query_updates.emplace_back(RevisionUpdate{0, 0});
                query_dependencies.emplace_back();
                queries.push_back(query);
            }
            size_t id = it->second;
            return std::make_pair(id, &query_updates[id]);
        }

        bool is_query_out_of_date(const Query &query) {
            auto [id, update] = get_query_metadata(query);
            if (update->verified == revision) {
                return false; // Query is up to date
            }

            if (update->verified == 0) {
                return true; // Query has never been verified, so it is out of date
            }

            QueryRefreshType refresh_type =
                std::visit([this,
                            query](auto &&q) { return std::decay_t<decltype(q)>::refresh_type; },
                           query);

            if (refresh_type == QueryRefreshType::AlwaysRefresh) {
                return true; // Query is always out of date
            } else if (refresh_type == QueryRefreshType::RefreshOnDemand) {
                // For simplicity, we will assume on-demand queries are always up to date unless
                // marked as changed.
                return update->changed > update->verified;
            }

            assert(refresh_type == QueryRefreshType::RefreshOnDependentChange);

            // This is where much of the magic happens.
            //
            // After evaluating a query, we mark it either verified or changed at that revision. If
            // we can skip an evaluation, or produce an equal result, we mark it verified. If a
            // query's dependencies are all verified, the query can be verified.
            //
            // We can only trust the 'changed' revision is accurate for a verified query.
            //
            // Verifying a query for a new revision involves recursively verifying all dependencies.
            // Eventually, the recursion bottoms out at queries with no dependencies. These queries
            // are either always out of date and refreshed on demand, or they are invalidated by
            // external event triggers, and always up to date unless marked as changed.
            //
            // From the bottom up, queries will either update "verified" to the current revision, or
            // update "changed" and "verified" to the current revision.
            for (size_t dep : query_dependencies[id]) {
                auto &dep_update = query_updates[dep];
                // If we know a dependency has changed since our last verification, we must rerun.
                if (dep_update.changed > update->verified) {
                    return true;
                }

                // If a dependency has been verified to be unchanged, it is up to date.
                if (dep_update.verified == revision) {
                    continue;
                }

                // The dependency does not appear to be changed, but is not verified, so we must
                // recursively verify it, by re-executing. It will either verify and skip, or it
                // will rerun and check for output equality.
                if (is_query_out_of_date(queries[dep])) {
                    // Note: do not pass the parent query, or we'll add new nodes to the dependency
                    // graph. We don't want to update the dependency graph unless we rerun the
                    // parent.
                    execute(queries[dep], std::nullopt);
                }

                // After reverifying and/or refreshing the dependency, we can trust the "changed"
                // revision is accurate.
                if (dep_update.changed > update->verified) {
                    return true;
                }
            }

            update->verified = revision; // All dependencies are verified and up to date, so we are
                                         // verified and up to date.
            return false;
        }

        struct GetCachedVisitor {
            QueryEngine *engine;

            template <typename T>
            std::optional<std::reference_wrapper<std::add_const_t<typename T::ResultType>>>
            cache_lookup(const T &query,
                         const std::unordered_map<T, typename T::ResultType> &cache) {
                auto it = cache.find(query);
                if (it != cache.end()) {
                    return std::optional(std::cref(it->second));
                }
                return std::nullopt;
            }

            std::optional<std::reference_wrapper<const SourceContentsQuery::ResultType>> operator()(
                const SourceContentsQuery &query) {
                return cache_lookup(query, engine->source_contents_cache);
            }

            std::optional<std::reference_wrapper<const ParseQuery::ResultType>> operator()(
                const ParseQuery &query) {
                return cache_lookup(query, engine->parse_cache);
            }

            std::optional<std::reference_wrapper<const FunctionIdsQuery::ResultType>> operator()(
                const FunctionIdsQuery &query) {
                return cache_lookup(query, engine->function_ids_cache);
            }
        };

        struct UpdateCacheVisitor {
            QueryEngine *engine;

            void operator()(const SourceContentsQuery &query,
                            const SourceContentsQuery::ResultType &result) {
                engine->source_contents_cache[query] = result;
            }

            void operator()(const ParseQuery &query, ParseQuery::ResultType &result) {
                engine->parse_cache[query] = std::move(result);
            }

            void operator()(const FunctionIdsQuery &query, FunctionIdsQuery::ResultType &result) {
                engine->function_ids_cache[query] = std::move(result);
            }
        };

        struct ExecuteQueryVisitor {
            QueryEngine *engine;
            size_t query_id;

            SourceContentsQuery::ResultType operator()(const SourceContentsQuery &query) {
                // For simplicity, we will just read the file contents directly. In a real
                // implementation, we would want to use a file system listener or timestamp to
                // determine when source contents have changed.
                std::ifstream input_file(query.path);
                if (!input_file) {
                    throw std::runtime_error("Error: Could not open file: " + query.path.string());
                }
                return std::string((std::istreambuf_iterator<char>(input_file)),
                                   std::istreambuf_iterator<char>());
            }

            ParseQuery::ResultType operator()(const ParseQuery &query) {
                auto &source_contents = engine->execute(SourceContentsQuery{query.path}, query_id);
                auto result = arena::parse::parse(source_contents);
                return result;
            }

            FunctionIdsQuery::ResultType operator()(const FunctionIdsQuery &query) {
                auto &ast = engine->execute(ParseQuery{query.path}, query_id);
                FunctionTableBuilder builder(engine->global_context.get_function_registry(),
                                             engine->global_context.get_type_registry());
                auto ftable = builder.build(ast.declarations);
                return ftable.get_ids();
            }
        };

        void execute(const Query &key, std::optional<size_t> id) {
            std::visit([this, &key, id](auto &&q) { this->execute(q, key, id); }, key);
        }

        template <typename T>
        const typename T::ResultType &execute(const T &query, size_t id) {
            return this->execute(query, query, id);
        }

        template <typename T>
        const typename T::ResultType &execute(const T &query,
                                              const Query key,
                                              std::optional<size_t> parent = std::nullopt) {
            assert(std::holds_alternative<T>(key));
            assert(std::get<T>(key) == query);

            auto [id, update] = get_query_metadata(key);

            if (parent.has_value()) {
                // "Always-refresh" dependencies will execute with their own ID as parent. Ignore.
                query_dependencies[parent.value()].push_back(id);
            }

            if (!is_query_out_of_date(key)) {
                auto optional = GetCachedVisitor{this}(query);
                if (!optional.has_value()) {
                    throw std::runtime_error("Cache miss for up to date query");
                }
                update->verified = revision;
                return optional.value().get();
            }

            query_dependencies[id].clear(); // Clear dependencies before execution, they will be
                                            // repopulated during execution.

            auto previous = GetCachedVisitor{this}(query);
            auto result = ExecuteQueryVisitor{this, id}(query);

            if (previous.has_value() && previous.value().get() == result) {
                update->verified =
                    revision; // Output is the same, so we are verified and up to date.
                return previous.value().get();
            } else {
                update->changed = revision; // Output has changed, so we are changed and up to date.
                update->verified = revision;
                UpdateCacheVisitor{this}(query, result);
                auto &value = GetCachedVisitor{this}(query).value().get();
                return value;
            }
        }

        std::deque<Query> queries;
        std::deque<RevisionUpdate> query_updates;
        std::deque<std::deque<size_t>> query_dependencies;
        std::unordered_map<SourceContentsQuery, SourceContentsQuery::ResultType>
            source_contents_cache;
        std::unordered_map<ParseQuery, ParseQuery::ResultType> parse_cache;
        std::unordered_map<FunctionIdsQuery, FunctionIdsQuery::ResultType> function_ids_cache;
        std::unordered_map<Query, size_t> query_id;
        size_t revision = 1;
        GlobalContext global_context;
    };

} // namespace arena::sema

#endif // ARENA_INCLUDE_QUERY_ENGINE_HPP