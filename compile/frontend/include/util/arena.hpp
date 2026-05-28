#ifndef ARENA_INCLUDE_UTIL_ARENA_HPP
#define ARENA_INCLUDE_UTIL_ARENA_HPP
extern "C" {
#include "arena.h"
}

namespace arena::util {
    class Arena {
    public:
        Arena() { rena_arena_init(&arena, 0, 0); }

        Arena(size_t initial_page_size, size_t max_grow_size) {
            rena_arena_init(&arena, initial_page_size, max_grow_size);
        }

        Arena(const Arena &) = delete;
        Arena &operator=(const Arena &) = delete;
        Arena(Arena &&other) {
            arena = other.arena;
            other.arena.head = nullptr;
        }
        Arena &operator=(Arena &&other) {
            if (this != &other) {
                rena_arena_free(&arena);
                arena = other.arena;
                other.arena.head = nullptr;
            }
            return *this;
        }

        ~Arena() { rena_arena_free(&arena); }

        template <typename T, typename... Args>
        T *alloc(Args &&...args) {
            void *ptr;
            if (rena_arena_alloc(&arena, sizeof(T), alignof(T), &ptr) != RENA_ARENA_SUCCESS) {
                return nullptr;
            }
            return new (ptr) T(std::forward<Args>(args)...);
        }

        template <typename T>
        T *alloc_array(size_t count) {
            void *ptr;
            if (rena_arena_alloc(&arena, sizeof(T) * count, alignof(T), &ptr) != RENA_ARENA_SUCCESS) {
                return nullptr;
            }
            T* typed_ptr = static_cast<T *>(ptr);
            for (size_t i = 0; i < count; i++) {
                new (typed_ptr + i) T();
            }
            return typed_ptr;  
        }

    private:
        rena_arena arena;
    };
} // namespace arena::util

#endif