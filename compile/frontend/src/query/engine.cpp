#include "query/engine.hpp"

using namespace arena::sema;

std::pair<size_t, RevisionUpdate *> QueryEngine::get_query_metadata(const Query &query) {
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

bool QueryEngine::is_query_out_of_date(const Query &query) {
    auto [id, update] = get_query_metadata(query);
    if (update->verified == revision) {
        return false; // Query is up to date
    }

    if (update->verified == 0) {
        return true; // Query has never been verified, so it is out of date
    }

    QueryRefreshType refresh_type =
        std::visit([this, query](auto &&q) { return std::decay_t<decltype(q)>::refresh_type; },
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