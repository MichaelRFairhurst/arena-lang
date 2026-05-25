#ifndef ARENA_LIB_ARENA_H
#define ARENA_LIB_ARENA_H

#include <stdlib.h>

typedef struct rena_arena_page {
    struct rena_arena_page *next;
    void *begin;
    void *end;
    void *current;
} rena_arena_page;

typedef struct rena_arena {
    rena_arena_page *head;
    rena_arena_page *tail;
    size_t max_grow_size;
} rena_arena;

enum rena_arena_error {
    RENA_ARENA_SUCCESS = 0,
    RENA_ARENA_OUT_OF_MEMORY = 1,
    RENA_ARENA_INVALID_ARGUMENT = 2,
    RENA_ARENA_INTERNAL_ERROR = 3,
};

#define RENA_ARENA_DEFAULT_PAGE_SIZE (4096) // 4KB default page size
#define RENA_ARENA_SMALL_PAGE_SIZE (256) // 256B small page size for small allocations
#define RENA_ARENA_LARGE_PAGE_SIZE (65536) // 64KB large page size for large allocations

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#ifndef RENA_ARENA_PAGE_ALIGN
#define RENA_ARENA_PAGE_ALIGN 64 // 64-byte alignment for better cache performance
#endif
#endif


/**
 * Initialize a default arena for bump allocation.
 * 
 * If initial_page_size is greater than zero, that number of bytes will be allocated immediately
 * for the first page. If the initial page size is zero, the first page will be allocated on the
 * first allocation request, and use DEFAULT_PAGE_SIZE. Note that header bytes will reduce the
 * usable space on each page, including the first.
 * 
 * Pages will double on each grow, up to the max_grow_size limit. If max_grow_size is zero, it will
 * default to LARGE_PAGE_SIZE. Allocations larger than MAX_GROW_SIZE will allocate a single page of
 * the requested size.
 */
int rena_arena_init(rena_arena *arena, size_t initial_page_size, size_t max_grow_size);

/**
 * Add a new page to the arena with the given size. The page will be aligned to
 * RENA_ARENA_PAGE_ALIGN if defined.
 */
int rena_arena_add_page(rena_arena *arena, size_t page_size);

/**
 * Add a new page to the arena that can at least fit one object of the given size and alignment.
 * 
 * Note that the pointer into the page will not be aligned to the requested alignment. Alignment is
 * only used to calculate the page allocation size.
 */
int rena_arena_grow(rena_arena *arena, size_t size, size_t align);

/**
 * Allocate memory from the arena with the given size and alignment.
 * The allocated memory pointer will be stored in out_ptr.
 */
int rena_arena_alloc(rena_arena *arena, size_t size, size_t align, void **out_ptr);

/**
 * Free all pages in the arena.
 */
int rena_arena_free(rena_arena *arena);
#endif // ARENA_LIB_ARENA_H