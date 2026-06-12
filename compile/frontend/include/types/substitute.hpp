#ifndef ARENA_INCLUDE_TYPES_SUBSTITUTE_HPP
#define ARENA_INCLUDE_TYPES_SUBSTITUTE_HPP

#include "signatures/types.hpp"

namespace arena::sema {

    TypeId substitute_lifetimes(TypeId type_id,
                                const LifetimeGroup &type_lifetimes,
                                const TypeTable *ttable,
                                const std::unordered_map<LifetimeId, LifetimeId> &substitutions);

} // namespace arena::sema

#endif // ARENA_INCLUDE_TYPES_SUBSTITUTE_HPP