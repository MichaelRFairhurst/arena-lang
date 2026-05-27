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

    class QueryEngine;
    class QueryEngineContext {
    public:
        QueryEngineContext(GlobalContext *global_context, QueryEngine *engine, size_t our_query_id)
            : our_query_id(our_query_id), global_context(global_context), engine(engine) {}

        template <typename T>
        const typename T::ResultType &run_query(const T &child_query) const;

        const FunctionSymbolRegistry &get_function_registry() const { return global_context->get_function_registry(); }
        const TypeSymbolRegistry &get_type_registry() const { return global_context->get_type_registry(); }

    private:
        GlobalContext *global_context;
        QueryEngine *engine;
        size_t our_query_id;
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
        std::pair<size_t, RevisionUpdate *> get_query_metadata(const Query &query);

        bool is_query_out_of_date(const Query &query);

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
                auto optional = query_cache.get_cached(query);
                if (!optional.has_value()) {
                    throw std::runtime_error("Cache miss for up to date query");
                }
                update->verified = revision;
                return optional.value().get();
            }

            query_dependencies[id].clear(); // Clear dependencies before execution, they will be
                                            // repopulated during execution.

            auto previous = query_cache.get_cached(query);
            auto context = QueryEngineContext{&global_context, this, id};
            auto result = compute_query_result(context, query);

            if (previous.has_value() && previous.value().get() == result) {
                update->verified =
                    revision; // Output is the same, so we are verified and up to date.
                return previous.value().get();
            } else {
                update->changed = revision; // Output has changed, so we are changed and up to date.
                update->verified = revision;
                query_cache.update_cache(query, std::move(result));
                auto &value = query_cache.get_cached(query).value().get();
                return value;
            }
        }

        std::deque<Query> queries;
        std::deque<RevisionUpdate> query_updates;
        std::deque<std::deque<size_t>> query_dependencies;
        QueryCache query_cache;
        std::unordered_map<Query, size_t> query_id;
        size_t revision = 1;
        GlobalContext global_context;

        friend class QueryEngineContext;
    };

    template <typename T>
    const typename T::ResultType &QueryEngineContext::run_query(const T &child_query) const {
        return engine->execute(child_query, our_query_id);
    };

} // namespace arena::sema

#endif // ARENA_INCLUDE_QUERY_ENGINE_HPP