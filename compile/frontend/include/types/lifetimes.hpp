#ifndef ARENA_INCLUDE_TYPES_LIFETIMES_HPP
#define ARENA_INCLUDE_TYPES_LIFETIMES_HPP

#include <stack>
#include "ast/declarations.hpp"
#include "errors/errors.hpp"
#include "signatures/lifetimes.hpp"
#include "resolve/variables.hpp"

namespace arena::sema {

    class LifetimeSolver {
    public:
        LifetimeSolver() = default;
        LifetimeSolver(error::Reporter *errors) : errors(errors) {}

        void solve(const LifetimeGroup &group);

    private:
        error::Reporter *errors;
    };
} // namespace arena::sema

#endif // ARENA_INCLUDE_TYPES_LIFETIMES_HPP