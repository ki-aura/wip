// gheap.c - Generic Min-Heap Implementation

#include "gheap.h" // Includes type definitions for heap_t, heap_cmp_fn, heap_match_fn
#include <stdlib.h>    // For malloc, realloc, free
#include <string.h>    // For utility functions (though not heavily used here)
#include <stdio.h>     // For utility functions (though not heavily used here)

// Initial size for the internal array that stores heap elements
#define HEAP_INITIAL_CAPACITY 16

// The main heap structure definition (opaque to the user).
// This is the core data structure that holds the heap's state.
struct heap {
    void **data;                // Array of void pointers to store the actual user data items.
                                // Using void* makes the heap generic (can hold pointers to any type).
    size_t size;                // Current number of elements stored in the heap.
    size_t capacity;            // Maximum number of elements the current 'data' array can hold.
    heap_cmp_fn cmp;            // Pointer to the user-provided function for comparing two items.
                                // This defines the heap's priority (e.g., min-heap vs. max-heap).
    heap_match_fn match;        // Pointer to the user-provided function for matching an item against a key.
                                // Used primarily for searching/deleting non-root elements.
};

// --- Internal helpers ---

// Swaps two void pointers in the data array.
static void swap(void **a, void **b) {
    void *tmp = *a;
    *a = *b;
    *b = tmp;
}

// Restores the heap property by moving a newly added or modified item up the tree.
// It stops when the item is at the root (idx=0) or is greater than/equal to its parent.
static void sift_up(heap_t *h, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        
        // Use the user-defined comparison function (h->cmp) to check the heap property.
        // For a MIN-HEAP, we check if the child is smaller than the parent (cmp < 0).
        if (h->cmp(h->data[idx], h->data[parent]) >= 0) break; 
        
        // If the child is smaller (higher priority), swap them.
        swap(&h->data[idx], &h->data[parent]);
        
        // Move up to the parent's position and repeat.
        idx = parent;
    }
}

// Restores the heap property by moving an item at 'idx' down the tree.
// Used after 'pop' or internal modifications (e.g., in heap_find_and_pop).
static void sift_down(heap_t *h, size_t idx) {
    size_t left, right, smallest;
    while (1) {
        left  = 2 * idx + 1;
        right = 2 * idx + 2;
        smallest = idx; // Assume current node is the smallest (highest priority)

        // Check if the left child exists and is smaller than the current smallest.
        // Again, this relies on the user-defined h->cmp function.
        if (left < h->size && h->cmp(h->data[left], h->data[smallest]) < 0)
            smallest = left;
            
        // Check if the right child exists and is smaller than the current smallest.
        if (right < h->size && h->cmp(h->data[right], h->data[smallest]) < 0)
            smallest = right;

        // If the current node is the smallest, the heap property is satisfied here.
        if (smallest == idx) break;

        // Swap the current node with the smallest child.
        swap(&h->data[idx], &h->data[smallest]);
        
        // Continue sifting down from the new position.
        idx = smallest;
    }
}

// --- Public API ---

// Allocates memory and initializes a new heap structure.
// This function establishes the heap's genericity by accepting function pointers:
// - cmp: The comparison function that dictates the heap order (priority).
// - match: The matching function used to find arbitrary elements by a secondary key.
heap_t *heap_create(heap_cmp_fn cmp, heap_match_fn match) {
    if (!cmp || !match) return NULL; // Must have valid function pointers

    // Allocate the heap control structure
    heap_t *h = malloc(sizeof(*h));
    if (!h) return NULL;

    // Allocate the internal data array of void pointers
    h->data = malloc(sizeof(void*) * HEAP_INITIAL_CAPACITY);
    if (!h->data) { free(h); return NULL; }

    // Initialize state variables
    h->size = 0;
    h->capacity = HEAP_INITIAL_CAPACITY;
    h->cmp = cmp;     // Store the user's comparison function
    h->match = match; // Store the user's match function
    return h;
}

// Frees all memory associated with the heap structure itself (but not the stored items).
// NOTE: The caller is responsible for freeing the items still in the heap before calling this.
int heap_destroy(heap_t *h) {
    if (!h) return -1;
    free(h->data); // Free the internal array
    free(h);       // Free the heap structure
    return 0;
}

// Returns the current number of elements in the heap.
size_t heap_size(heap_t *h) {
    if (!h) return 0;
    return h->size;
}

// Inserts a new item into the heap.
int heap_push(heap_t *h, void *item) {
    if (!h || !item) return -1;

    // Check if the internal array is full and needs to be resized (doubled).
    if (h->size == h->capacity) {
        size_t new_cap = h->capacity * 2;
        // Use realloc to grow the array
        void **tmp = realloc(h->data, sizeof(void*) * new_cap);
        if (!tmp) return -1;
        h->data = tmp;
        h->capacity = new_cap;
    }

    // Place the new item at the end of the array.
    h->data[h->size] = item;
    
    // Restore the heap property by sifting the new item up.
    sift_up(h, h->size);
    
    // Increment the size.
    h->size++;
    return 0;
}

// Removes and returns the highest priority item (the root/minimum element).
void *heap_pop(heap_t *h) {
    if (!h || h->size == 0) return NULL;

    void *top = h->data[0]; // Save the top element to return
    h->size--;              // Decrease size

    if (h->size > 0) {
        // Move the last element to the root position.
        h->data[0] = h->data[h->size];
        
        // Restore the heap property by sifting the new root down.
        sift_down(h, 0);
    }
    return top;
}

// Returns the highest priority item (the root) without removing it.
void *heap_peek(heap_t *h) {
    if (!h || h->size == 0) return NULL;
    return h->data[0];
}

// Searches for and removes the first item matching the given key, then rebalances the heap.
void *heap_find_and_pop(heap_t *h, const void *key) {
    if (!h || h->size == 0 || !key) return NULL;
    // Linear search through the heap array to find the item.
    for (size_t i = 0; i < h->size; ++i) {
        // Use the user-defined matching function (h->match) to check if the item matches the key.
        if (h->match(h->data[i], key)) {
            void *found = h->data[i];
            h->size--; // Decrement size immediately
            
            // If the found element wasn't the last element in the array:
            if (i != h->size) {
				// 1. Move last item to the hole
				h->data[i] = h->data[h->size]; 
				// 2. sift down then up; low efficiency but safe (and function is low use)
				// as it always ensures heap balancing property is fully restored				
				sift_down(h, i);
				sift_up(h, i);
				}
            return found; // Return the removed item
        }
    }
    return NULL; // Item not found
}