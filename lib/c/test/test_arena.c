#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "arena.h"
#include <string.h>
#include <stdint.h>

// ============================================================================
// Initialization Tests
// ============================================================================

static inline size_t rena_get_page_size(rena_arena_page *page) {
    return (size_t)((char *)page->end - (char *)page);
}

void test_init_with_initial_page(void) {
    rena_arena arena;
    int result = rena_arena_init(&arena, 4096, 0);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(arena.head);
    CU_ASSERT_PTR_NOT_NULL(arena.tail);
    CU_ASSERT_PTR_EQUAL(arena.head, arena.tail);
    CU_ASSERT_EQUAL(arena.max_grow_size, RENA_ARENA_LARGE_PAGE_SIZE);
    CU_ASSERT_PTR_EQUAL(arena.head->begin, arena.head->current);
    CU_ASSERT_PTR_EQUAL(arena.head->end, arena.head->current + 4096 - sizeof(rena_arena_page));
    
    rena_arena_free(&arena);
}

void test_init_deferred(void) {
    rena_arena arena;
    int result = rena_arena_init(&arena, 0, 8192);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NULL(arena.head);
    CU_ASSERT_PTR_NULL(arena.tail);
    CU_ASSERT_EQUAL(arena.max_grow_size, 8192);
}

void test_init_null_arena(void) {
    int result = rena_arena_init(NULL, 4096, 0);
    CU_ASSERT_EQUAL(result, RENA_ARENA_INVALID_ARGUMENT);
}

// ============================================================================
// Basic Allocation Tests
// ============================================================================

void test_alloc_basic(void) {
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    void *ptr;
    int result = rena_arena_alloc(&arena, 64, 8, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 7, 0);
    
    rena_arena_free(&arena);
}

void test_alloc_aligned_64(void) {
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    void *ptr;
    rena_arena_alloc(&arena, 100, 64, &ptr);
    
    CU_ASSERT_EQUAL((uintptr_t)ptr & 63, 0);
    
    rena_arena_free(&arena);
}

void test_alloc_various_alignments(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    
    void *ptr1, *ptr2, *ptr3;
    
    rena_arena_alloc(&arena, 10, 1, &ptr1);
    CU_ASSERT_EQUAL((uintptr_t)ptr1 & 0, 0);
    
    rena_arena_alloc(&arena, 10, 16, &ptr2);
    CU_ASSERT_EQUAL((uintptr_t)ptr2 & 15, 0);
    
    rena_arena_alloc(&arena, 10, 128, &ptr3);
    CU_ASSERT_EQUAL((uintptr_t)ptr3 & 127, 0);
    
    rena_arena_free(&arena);
}

void test_alloc_no_overlap(void) {
    rena_arena arena;
    rena_arena_init(&arena, 1024, 0);
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        rena_arena_alloc(&arena, 20, 4, &ptrs[i]);
        memset(ptrs[i], i, 20);
    }
    
    // Verify data integrity (no overlap)
    for (int i = 0; i < 10; i++) {
        unsigned char *p = ptrs[i];
        for (int j = 0; j < 20; j++) {
            CU_ASSERT_EQUAL(p[j], (unsigned char)i);
        }
    }
    
    rena_arena_free(&arena);
}

// ============================================================================
// Growth Tests
// ============================================================================

void test_alloc_triggers_growth(void) {
    rena_arena arena;
    rena_arena_init(&arena, 128, 0);
    
    // Count initial pages
    size_t initial_pages = 0;
    for (rena_arena_page *p = arena.head; p; p = p->next) {
        initial_pages++;
    }
    
    // Allocate more than fits
    void *ptr;
    int result = rena_arena_alloc(&arena, 200, 8, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    
    // Count pages after growth
    size_t after_pages = 0;
    for (rena_arena_page *p = arena.head; p; p = p->next) {
        after_pages++;
    }
    
    CU_ASSERT_TRUE(after_pages > initial_pages);
    
    rena_arena_free(&arena);
}

void test_alloc_huge_exceeds_max(void) {
    rena_arena arena;
    rena_arena_init(&arena, 0, 4096);
    
    void *ptr;
    int result = rena_arena_alloc(&arena, 10000, 8, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_TRUE(rena_get_page_size(arena.head) >= 10000 + sizeof(rena_arena_page));
    CU_ASSERT_TRUE(ptr >= arena.head->begin && (char *)ptr + 10000 <= (char *)arena.head->end);
    
    // Should be able to write to it
    memset(ptr, 0xFF, 10000);
    
    rena_arena_free(&arena);
}

void test_alloc_with_deferred_init(void) {
    rena_arena arena;
    rena_arena_init(&arena, 0, 0);
    
    CU_ASSERT_PTR_NULL(arena.head);
    
    void *ptr;
    int result = rena_arena_alloc(&arena, 64, 8, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(arena.head);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    
    rena_arena_free(&arena);
}

void test_growth_power_of_2(void) {
    rena_arena arena;
    rena_arena_init(&arena, 256, 0);
    
    // Force multiple growths
    void *ptr;
    rena_arena_alloc(&arena, 200, 8, &ptr);
    rena_arena_alloc(&arena, 400, 8, &ptr);
    rena_arena_alloc(&arena, 800, 8, &ptr);
    
    rena_arena_page *page = arena.head;
    CU_ASSERT_PTR_NOT_NULL_FATAL(page);
    rena_arena_page *second = page->next;
    CU_ASSERT_PTR_NOT_NULL_FATAL(second);
    rena_arena_page *third = second->next;
    CU_ASSERT_PTR_NOT_NULL_FATAL(third);

    CU_ASSERT_PTR_NULL(third->next);
    CU_ASSERT_TRUE(rena_get_page_size(page) == 256);
    CU_ASSERT_TRUE(rena_get_page_size(second) == 512);
    CU_ASSERT_TRUE(rena_get_page_size(third) == 1024);
    
    rena_arena_free(&arena);
}

void test_growth_exceed_power_of_2(void) {
    rena_arena arena;
    rena_arena_init(&arena, 256, 0);
    void *ptr;
    // trigger an allocation
    rena_arena_alloc(&arena, 10, 8, &ptr);

    // trigger an allocation larger than next power of 2
    rena_arena_alloc(&arena, 1000, 8, &ptr);
    rena_arena_page *page = arena.head;
    CU_ASSERT_PTR_NOT_NULL_FATAL(page);
    rena_arena_page *second = page->next;
    CU_ASSERT_PTR_NOT_NULL_FATAL(second);
    
    CU_ASSERT_PTR_NULL(second->next);
    CU_ASSERT_TRUE(rena_get_page_size(page) == 256);
    CU_ASSERT_TRUE(rena_get_page_size(second) == 1024);

    // exactly double power of 2 of next size
    rena_arena_alloc(&arena, 2048, 8, &ptr);
    rena_arena_page *third = second->next;
    CU_ASSERT_PTR_NOT_NULL_FATAL(third);
    CU_ASSERT_PTR_NULL(third->next);
    CU_ASSERT_TRUE(rena_get_page_size(third) == 4096);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

void test_alloc_null_arguments(void) {
    rena_arena arena;
    void *ptr;
    
    rena_arena_init(&arena, 1024, 0);
    
    // NULL arena
    int result = rena_arena_alloc(NULL, 64, 8, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_INVALID_ARGUMENT);
    
    // NULL out_ptr
    result = rena_arena_alloc(&arena, 64, 8, NULL);
    CU_ASSERT_EQUAL(result, RENA_ARENA_INVALID_ARGUMENT);
    
    rena_arena_free(&arena);
}

void test_alloc_zero_size(void) {
    rena_arena arena;
    void *ptr;
    
    rena_arena_init(&arena, 1024, 0);
    
    int result = rena_arena_alloc(&arena, 0, 8, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_INVALID_ARGUMENT);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Sequential Allocation Tests
// ============================================================================

void test_alloc_sequential_small(void) {
    rena_arena arena;
    rena_arena_init(&arena, 1024, 0);
    
    void *ptrs[50];
    int successful = 0;
    
    for (int i = 0; i < 50; i++) {
        if (rena_arena_alloc(&arena, 16, 8, &ptrs[i]) == RENA_ARENA_SUCCESS) {
            successful++;
        }
    }
    
    CU_ASSERT_TRUE(successful == 50);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Free Tests
// ============================================================================

void test_free_basic(void) {
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    void *ptr;
    rena_arena_alloc(&arena, 100, 8, &ptr);
    
    int result = rena_arena_free(&arena);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NULL(arena.head);
    CU_ASSERT_PTR_NULL(arena.tail);
}

void test_free_multiple_pages(void) {
    rena_arena arena;
    rena_arena_init(&arena, 256, 0);
    
    // Force multiple pages
    for (int i = 0; i < 10; i++) {
        void *ptr;
        rena_arena_alloc(&arena, 100, 8, &ptr);
    }
    
    int result = rena_arena_free(&arena);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NULL(arena.head);
    CU_ASSERT_PTR_NULL(arena.tail);
}

void test_free_null_arena(void) {
    int result = rena_arena_free(NULL);
    CU_ASSERT_EQUAL(result, RENA_ARENA_INVALID_ARGUMENT);
}

// ============================================================================
// Parameterized Tests (manually unrolled since CUnit doesn't have this)
// ============================================================================

void test_various_sizes_1_1(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    void *ptr;
    int result = rena_arena_alloc(&arena, 1, 1, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 0, 0);
    CU_ASSERT_PTR_EQUAL(arena.head->current, (char *)ptr + 1);
    CU_ASSERT_PTR_EQUAL(arena.head->begin, ptr);
    memset(ptr, 0xAA, 1);
    rena_arena_free(&arena);
}

void test_various_sizes_8_8(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    void *ptr;
    int result = rena_arena_alloc(&arena, 8, 8, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 7, 0);
    CU_ASSERT_PTR_EQUAL(arena.head->current, (char *)ptr + 8);
    CU_ASSERT_PTR_EQUAL((char *) arena.head + sizeof(rena_arena_page), ptr);
    memset(ptr, 0xAA, 8);
    rena_arena_free(&arena);
}

void test_various_sizes_16_16(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    void *ptr;
    int result = rena_arena_alloc(&arena, 16, 16, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 15, 0);
    CU_ASSERT_PTR_EQUAL(arena.head->current, (char *)ptr + 16);
    CU_ASSERT_PTR_EQUAL((char *) arena.head + sizeof(rena_arena_page), (char *)ptr);
    memset(ptr, 0xAA, 16);
    rena_arena_free(&arena);
}

void test_various_sizes_64_8(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    void *ptr;
    int result = rena_arena_alloc(&arena, 64, 8, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 7, 0);
    CU_ASSERT_PTR_EQUAL(arena.head->current, (char *)ptr + 64);
    CU_ASSERT_PTR_EQUAL((char *) arena.head + sizeof(rena_arena_page), (char *)ptr);
    memset(ptr, 0xAA, 64);
    rena_arena_free(&arena);
}

void test_alloc_alignment_256(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    
    void *ptr;
    int result = rena_arena_alloc(&arena, 100, 256, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr);
    CU_ASSERT_EQUAL((uintptr_t)ptr & 255, 0);
    
    rena_arena_free(&arena);
}

void test_alloc_aligned_after_byte(void) {
    rena_arena arena;
    rena_arena_init(&arena, 8192, 0);
    
    void *ptr1, *ptr2;
    
    // Allocate a single byte to offset the current pointer
    int result = rena_arena_alloc(&arena, 1, 1, &ptr1);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    
    // Now allocate with large alignment - should still succeed and be properly aligned
    result = rena_arena_alloc(&arena, 100, 256, &ptr2);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(ptr2);
    CU_ASSERT_EQUAL((uintptr_t)ptr2 & 255, 0);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Page Boundary Tests
// ============================================================================

void test_alloc_fills_page_exactly(void) {
    rena_arena arena;
    size_t page_size = 256;
    rena_arena_init(&arena, page_size, 0);
    
    // Calculate available space (page_size - header)
    size_t available = page_size - sizeof(rena_arena_page);
    
    // Allocate exactly what fits
    void *ptr;
    int result = rena_arena_alloc(&arena, available, 1, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_EQUAL(arena.head->current, arena.head->end);
    
    // Next allocation should trigger growth
    void *ptr2;
    size_t pages_before = 0;
    for (rena_arena_page *p = arena.head; p; p = p->next) pages_before++;
    
    result = rena_arena_alloc(&arena, 10, 1, &ptr2);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    
    size_t pages_after = 0;
    for (rena_arena_page *p = arena.head; p; p = p->next) pages_after++;
    CU_ASSERT_TRUE(pages_after > pages_before);
    
    rena_arena_free(&arena);
}

void test_alloc_one_byte_too_large(void) {
    rena_arena arena;
    size_t page_size = 256;
    rena_arena_init(&arena, page_size, 0);
    
    // Calculate available space
    size_t available = page_size - sizeof(rena_arena_page);
    
    // Allocate one byte more than fits - should trigger growth
    void *ptr;
    int result = rena_arena_alloc(&arena, available + 1, 1, &ptr);
    
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    
    // Should have created a second page
    CU_ASSERT_PTR_NOT_NULL(arena.head->next);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Page Linking Tests
// ============================================================================

void test_page_linking_verification(void) {
    rena_arena arena;
    rena_arena_init(&arena, 128, 128);
    
    // Force multiple page allocations
    for (int i = 0; i < 5; i++) {
        void *ptr;
        rena_arena_alloc(&arena, 200, 8, &ptr);
    }
    
    // Verify linked list integrity
    int page_count = 0;
    rena_arena_page *prev = NULL;
    for (rena_arena_page *p = arena.head; p; p = p->next) {
        page_count++;
        prev = p;
    }
    
    CU_ASSERT_TRUE(page_count >= 5);
    CU_ASSERT_PTR_EQUAL(prev, arena.tail);
    CU_ASSERT_PTR_NULL(arena.tail->next);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Page Alignment Tests
// ============================================================================

void test_page_cache_line_alignment(void) {
#ifdef RENA_ARENA_PAGE_ALIGN
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    // Verify first page is cache-line aligned
    CU_ASSERT_EQUAL((uintptr_t)arena.head & (RENA_ARENA_PAGE_ALIGN - 1), 0);
    
    // Force another page and verify it's also aligned
    void *ptr;
    rena_arena_alloc(&arena, 5000, 8, &ptr);
    
    if (arena.head->next) {
        CU_ASSERT_EQUAL((uintptr_t)arena.head->next & (RENA_ARENA_PAGE_ALIGN - 1), 0);
    }
    
    rena_arena_free(&arena);
#else
    // Skip test if alignment not defined
    CU_PASS("RENA_ARENA_PAGE_ALIGN not defined");
#endif
}

// ============================================================================
// Bounds Verification Tests
// ============================================================================

void test_allocations_within_page_bounds(void) {
    rena_arena arena;
    rena_arena_init(&arena, 1024, 0);
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        rena_arena_alloc(&arena, 50, 8, &ptrs[i]);
    }
    
    // Verify each allocation is within some page's bounds
    for (int i = 0; i < 10; i++) {
        int found_in_page = 0;
        for (rena_arena_page *p = arena.head; p; p = p->next) {
            if (ptrs[i] >= p->begin && ptrs[i] < p->end) {
                found_in_page = 1;
                break;
            }
        }
        CU_ASSERT_TRUE(found_in_page);
    }
    
    rena_arena_free(&arena);
}

void test_page_current_never_exceeds_end(void) {
    rena_arena arena;
    rena_arena_init(&arena, 512, 0);
    
    // Do many allocations
    for (int i = 0; i < 100; i++) {
        void *ptr;
        rena_arena_alloc(&arena, 32, 8, &ptr);
        
        // Check all pages
        for (rena_arena_page *p = arena.head; p; p = p->next) {
            CU_ASSERT_TRUE(p->current <= p->end);
        }
    }
    
    rena_arena_free(&arena);
}

// ============================================================================
// Small Page Size Tests
// ============================================================================

void test_very_small_initial_page(void) {
    rena_arena arena;
    // Page smaller than a typical allocation
    rena_arena_init(&arena, 64, 0);
    
    CU_ASSERT_PTR_NOT_NULL(arena.head);
    
    // Should still be able to allocate (will trigger growth)
    void *ptr;
    int result = rena_arena_alloc(&arena, 100, 8, &ptr);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    
    rena_arena_free(&arena);
}

// ============================================================================
// Multiple Arenas Tests
// ============================================================================

void test_multiple_independent_arenas(void) {
    rena_arena arena1, arena2, arena3;
    
    rena_arena_init(&arena1, 1024, 0);
    rena_arena_init(&arena2, 2048, 0);
    rena_arena_init(&arena3, 0, 4096);
    
    void *ptr1, *ptr2, *ptr3;
    
    rena_arena_alloc(&arena1, 100, 8, &ptr1);
    rena_arena_alloc(&arena2, 200, 16, &ptr2);
    rena_arena_alloc(&arena3, 300, 8, &ptr3);
    
    // Verify all three allocations succeeded
    CU_ASSERT_PTR_NOT_NULL(ptr1);
    CU_ASSERT_PTR_NOT_NULL(ptr2);
    CU_ASSERT_PTR_NOT_NULL(ptr3);
    
    // Write to each to ensure they don't interfere
    memset(ptr1, 0xAA, 100);
    memset(ptr2, 0xBB, 200);
    memset(ptr3, 0xCC, 300);
    
    // Free in different order
    rena_arena_free(&arena2);
    rena_arena_free(&arena1);
    rena_arena_free(&arena3);
}

// ============================================================================
// Free Edge Cases
// ============================================================================

void test_free_deferred_arena(void) {
    rena_arena arena;
    rena_arena_init(&arena, 0, 0);
    
    // Free without ever allocating
    int result = rena_arena_free(&arena);
    CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
    CU_ASSERT_PTR_NULL(arena.head);
    CU_ASSERT_PTR_NULL(arena.tail);
}

// ============================================================================
// Stress Tests
// ============================================================================

void test_stress_many_small_allocations(void) {
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    for (int i = 0; i < 1000; i++) {
        void *ptr;
        int result = rena_arena_alloc(&arena, 32, 8, &ptr);
        CU_ASSERT_EQUAL(result, RENA_ARENA_SUCCESS);
        memset(ptr, i & 0xFF, 32);
    }
    
    rena_arena_free(&arena);
}

void test_stress_mixed_sizes(void) {
    rena_arena arena;
    rena_arena_init(&arena, 4096, 0);
    
    for (int i = 0; i < 100; i++) {
        void *small, *large;
        
        rena_arena_alloc(&arena, 16, 8, &small);
        rena_arena_alloc(&arena, 512, 16, &large);
        
        memset(small, 0xAA, 16);
        memset(large, 0xBB, 512);
    }
    
    rena_arena_free(&arena);
}

// ============================================================================
// Suite Setup
// ============================================================================

int init_suite(void) {
    return 0;
}

int cleanup_suite(void) {
    return 0;
}

int main() {
    CU_pSuite pSuite = NULL;
    
    // Initialize CUnit registry
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }
    
    // Add suite to registry
    pSuite = CU_add_suite("Arena_Test_Suite", init_suite, cleanup_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    // Add initialization tests
    if ((NULL == CU_add_test(pSuite, "init_with_initial_page", test_init_with_initial_page)) ||
        (NULL == CU_add_test(pSuite, "init_deferred", test_init_deferred)) ||
        (NULL == CU_add_test(pSuite, "init_null_arena", test_init_null_arena)) ||
        
        // Basic allocation tests
        (NULL == CU_add_test(pSuite, "alloc_basic", test_alloc_basic)) ||
        (NULL == CU_add_test(pSuite, "alloc_aligned_64", test_alloc_aligned_64)) ||
        (NULL == CU_add_test(pSuite, "alloc_various_alignments", test_alloc_various_alignments)) ||
        (NULL == CU_add_test(pSuite, "alloc_no_overlap", test_alloc_no_overlap)) ||
        
        // Growth tests
        (NULL == CU_add_test(pSuite, "alloc_triggers_growth", test_alloc_triggers_growth)) ||
        (NULL == CU_add_test(pSuite, "alloc_huge_exceeds_max", test_alloc_huge_exceeds_max)) ||
        (NULL == CU_add_test(pSuite, "alloc_with_deferred_init", test_alloc_with_deferred_init)) ||
        (NULL == CU_add_test(pSuite, "growth_power_of_2", test_growth_power_of_2)) ||
        
        // Error handling tests
        (NULL == CU_add_test(pSuite, "alloc_null_arguments", test_alloc_null_arguments)) ||
        (NULL == CU_add_test(pSuite, "alloc_zero_size", test_alloc_zero_size)) ||
        
        // Sequential allocation tests
        (NULL == CU_add_test(pSuite, "alloc_sequential_small", test_alloc_sequential_small)) ||
        
        // Free tests
        (NULL == CU_add_test(pSuite, "free_basic", test_free_basic)) ||
        (NULL == CU_add_test(pSuite, "free_multiple_pages", test_free_multiple_pages)) ||
        (NULL == CU_add_test(pSuite, "free_null_arena", test_free_null_arena)) ||
        
        // Various sizes tests
        (NULL == CU_add_test(pSuite, "various_sizes_1_1", test_various_sizes_1_1)) ||
        (NULL == CU_add_test(pSuite, "various_sizes_8_8", test_various_sizes_8_8)) ||
        (NULL == CU_add_test(pSuite, "various_sizes_16_16", test_various_sizes_16_16)) ||
        (NULL == CU_add_test(pSuite, "various_sizes_64_8", test_various_sizes_64_8)) ||
        (NULL == CU_add_test(pSuite, "alloc_alignment_256", test_alloc_alignment_256)) ||
        (NULL == CU_add_test(pSuite, "alloc_aligned_after_byte", test_alloc_aligned_after_byte)) ||
        
        // Page boundary tests
        (NULL == CU_add_test(pSuite, "alloc_fills_page_exactly", test_alloc_fills_page_exactly)) ||
        (NULL == CU_add_test(pSuite, "alloc_one_byte_too_large", test_alloc_one_byte_too_large)) ||
        
        // Page linking tests
        (NULL == CU_add_test(pSuite, "page_linking_verification", test_page_linking_verification)) ||
        
        // Page alignment tests
        (NULL == CU_add_test(pSuite, "page_cache_line_alignment", test_page_cache_line_alignment)) ||
        
        // Bounds verification tests
        (NULL == CU_add_test(pSuite, "allocations_within_page_bounds", test_allocations_within_page_bounds)) ||
        (NULL == CU_add_test(pSuite, "page_current_never_exceeds_end", test_page_current_never_exceeds_end)) ||
        
        // Small page size tests
        (NULL == CU_add_test(pSuite, "very_small_initial_page", test_very_small_initial_page)) ||
        
        // Multiple arenas tests
        (NULL == CU_add_test(pSuite, "multiple_independent_arenas", test_multiple_independent_arenas)) ||
        
        // Free edge cases
        (NULL == CU_add_test(pSuite, "free_deferred_arena", test_free_deferred_arena)) ||
        
        // Stress tests
        (NULL == CU_add_test(pSuite, "stress_many_small_allocations", test_stress_many_small_allocations)) ||
        (NULL == CU_add_test(pSuite, "stress_mixed_sizes", test_stress_mixed_sizes)))
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    // Run tests using Basic interface
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    
    // Get failure count before cleanup
    unsigned int failures = CU_get_number_of_failures();
    
    // Clean up registry
    CU_cleanup_registry();
    
    return failures > 0 ? 1 : 0;
}
