#include "arena.h"

int rena_arena_init(rena_arena *arena, size_t initial_page_size, size_t max_grow_size) {
    if (arena == NULL) {
        return RENA_ARENA_INVALID_ARGUMENT;
    }

    if (max_grow_size == 0) {
        max_grow_size = RENA_ARENA_LARGE_PAGE_SIZE;
    }

    arena->head = (rena_arena_page *)NULL;
    arena->tail = (rena_arena_page *)NULL;
    arena->max_grow_size = max_grow_size;

    if (initial_page_size > 0) {
        return rena_arena_add_page(arena, initial_page_size);
    }

    return RENA_ARENA_SUCCESS;
}

// Round up to next power of 2
static inline size_t rena_next_pow2(size_t n) {
    if (n == 0)
        return 1;
    if ((n & (n - 1)) == 0)
        return n; // Already power of 2

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    n |= n >> 32;
#endif
    return n + 1;
}

static inline size_t rena_get_aligned(size_t offset, size_t align) {
    return (offset + align - 1) & ~(align - 1);
}

static inline size_t rena_get_padding(size_t offset, size_t align) {
    size_t aligned = rena_get_aligned(offset, align);
    return aligned - offset;
}

static inline size_t rena_get_page_size(rena_arena_page *page) {
    return (size_t)((char *)page->end - (char *)page);
}

int rena_arena_add_page(rena_arena *arena, size_t page_size) {
    if (arena == NULL || page_size == 0) {
        return RENA_ARENA_INVALID_ARGUMENT;
    }

    char *alloc;
#ifdef RENA_ARENA_PAGE_ALIGN
    // Use aligned_alloc if available (C11)
    page_size = rena_get_aligned(page_size, RENA_ARENA_PAGE_ALIGN);
    alloc = (char *)aligned_alloc(RENA_ARENA_PAGE_ALIGN, page_size);
#else
    alloc = (char *)malloc(page_size);
#endif

    if (alloc == NULL) {
        return RENA_ARENA_OUT_OF_MEMORY;
    }

    void *buf_begin = (void *)(alloc + sizeof(rena_arena_page));
    void *buf_end = (void *)(alloc + page_size);
    rena_arena_page *new_page = (rena_arena_page *)alloc;
    new_page->next = (rena_arena_page *)NULL;
    new_page->begin = buf_begin;
    new_page->end = buf_end;
    new_page->current = new_page->begin;

    if (arena->head == NULL) {
        arena->head = new_page;
        arena->tail = new_page;
    } else {
        arena->tail->next = new_page;
        arena->tail = new_page;
    }


    return RENA_ARENA_SUCCESS;
}

int rena_arena_grow(rena_arena *arena, size_t size, size_t align) {
    if (arena == NULL || size == 0) {
        return RENA_ARENA_INVALID_ARGUMENT;
    }

    // Ensure we have enough room for header and padding to first object.
    size_t padding = rena_get_padding(sizeof(rena_arena_page), align);
    size_t min_page_size = size + padding + sizeof(rena_arena_page);

    size_t new_size;
    if (arena->tail == NULL) {
        new_size = RENA_ARENA_DEFAULT_PAGE_SIZE;
    } else {
        size_t prev_size = rena_get_page_size(arena->tail);
        new_size = rena_next_pow2(prev_size * 2);
    }

    // Respect max_grow_size limit
    if (new_size > arena->max_grow_size) {
        new_size = arena->max_grow_size;
    }

    // Ensure we can at least fit the requested allocation
    if (new_size < min_page_size) {
        new_size = min_page_size;
    }

#ifdef RENA_ARENA_PAGE_ALIGN
    new_size = rena_get_aligned(new_size, RENA_ARENA_PAGE_ALIGN);
#endif

    return rena_arena_add_page(arena, new_size);
}

int rena_arena_should_grow(rena_arena_page *page, size_t size, size_t align) {
    if (page == NULL || size == 0) {
        return 1; // Invalid argument. Treat as grow, which will return error code.
    }

    uintptr_t aligned_addr = rena_get_aligned((uintptr_t)page->current, align);
    return aligned_addr + size > (uintptr_t)page->end;
}

int rena_arena_alloc(rena_arena *arena, size_t size, size_t align, void **out_ptr) {
    if (arena == NULL || out_ptr == NULL || size == 0) {
        return RENA_ARENA_INVALID_ARGUMENT;
    }

    if (rena_arena_should_grow(arena->tail, size, align)) {
        int grow_result = rena_arena_grow(arena, size, align);
        if (grow_result != RENA_ARENA_SUCCESS) {
            return grow_result;
        }
    }

    rena_arena_page *page = arena->tail;

    // Align the current pointer to the requested alignment
    uintptr_t aligned_addr = rena_get_aligned((uintptr_t)page->current, align);
    page->current = (void *)(aligned_addr + size);

    if (page->current > page->end) {
        return RENA_ARENA_INTERNAL_ERROR; // This should never happen if grow logic is correct
    }

    *out_ptr = (void *)aligned_addr;

    return RENA_ARENA_SUCCESS;
}

int rena_arena_free(rena_arena *arena) {
    if (arena == NULL) {
        return RENA_ARENA_INVALID_ARGUMENT;
    }

    rena_arena_page *page = arena->head;
    while (page != NULL) {
        rena_arena_page *next = page->next;
        free(page);
        page = next;
    }
    arena->head = (rena_arena_page *)NULL;
    arena->tail = (rena_arena_page *)NULL;
    return RENA_ARENA_SUCCESS;
}