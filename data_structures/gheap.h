#ifndef GHEAP_H
#define GHEAP_H

// Required includes for standard types
#include <stddef.h>  // For size_t
#include <stdbool.h> // For bool type

// Opaque structure definition for the heap handle.
// The actual contents of 'struct heap' are hidden in the .c file (gheap.c).
typedef struct heap heap_t;

// --- Function pointer types for Genericity ---

// Type definition for the user-defined comparison function.
// This function determines the heap's order (e.g., min-heap or max-heap).
// It must return:
//   - A negative value if *a* has higher priority (is "smaller") than *b*.
//   - Zero if *a* and *b* have equal priority.
//   - A positive value if *a* has lower priority (is "larger") than *b*.
typedef int (*heap_cmp_fn)(const void *a, const void *b);

// Type definition for the user-defined matching function.
// This function is used by heap_find_and_pop to check if an item
// matches a secondary key (like a task ID).
// It returns true if *item* matches the criteria defined by *key*, false otherwise.
typedef bool (*heap_match_fn)(const void *item, const void *key);

// --- Lifecycle Functions ---

// Allocates and initializes a new heap structure.
// Requires user-provided comparison and match functions to be generic.
heap_t *heap_create(heap_cmp_fn cmp, heap_match_fn match);

// Destroys the heap structure and frees its internal memory (but not the items stored).
int heap_destroy(heap_t *h);

// --- Heap Operations (Core API) ---

// Inserts a new item (a void pointer) into the heap and maintains the heap property.
int heap_push(heap_t *h, void *item);

// Removes and returns the highest priority item (the root) from the heap.
void *heap_pop(heap_t *h);

// Searches for an item using the user-defined 'match' function, removes the
// first matching item found, and rebalances the heap.
void *heap_find_and_pop(heap_t *h, const void *key);

// Returns the highest priority item (the root) without removing it.
void *heap_peek(heap_t *h);

// --- Utility Function ---

// Returns the current number of elements in the heap.
size_t heap_size(heap_t *h);

#endif // GHEAP_H