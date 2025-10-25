
/*******************************************************************************
 * KHASH.H REPLACEMENT DEVELOPMENT CHECKLIST (Gtree B+ Tree)
 * Strategy: Lazy Deletion (Tombstoning) with Copy-and-Rebuild Compaction
 *
 * This checklist defines the tasks required to replicate khash.h functionality
 * using the B+ tree implementation. It has been refined for clarity and 
 * completeness, and is intended as a working development reference.
 * 
 ******************************************************************************/

/* === 1. STRUCTURAL CHANGES (FOUNDATION) =================================== */
/* [ ] 1.1. Modify BTree Struct (Size Tracking) */
/* - Add size_t active_count;         // Logical size (like kh_size) */
/* - Add size_t total_payload_count; // Physical count incl. tombstones */
/* - Initialize both to 0 in tree_create. */

/* [ ] 1.2. Modify BPayload Struct (Lazy Delete & Data) */
/* - Replace 'int count' with your editor-specific payload: */
/*     struct { unsigned char value; } or equivalent edit_data */
/* - Add char is_deleted;            // Flag for tombstoning */
/* - Ensure memory management for edit_data and key is correct. */

/* === 2. MODIFIED EXISTING FUNCTIONS ======================================= */

/* [ ] 2.1. tree_create */
/* - Initialize active_count and total_payload_count to 0. */

/* [ ] 2.2. node_free_recursive */
/* - Ensure correct freeing of: */
/*   - key (via free_fn) */
/*   - edit_data payload (if dynamically allocated) */
/*   - Works for both active and tombstoned entries. */

/* [ ] 2.3. btree_insert (REWRITE FOR KHASH SEMANTICS) */
/* - Step 1: Lookup using btree_lookup_physical (see 3.1). */
/* - Step 2: If key exists and is tombstoned: */
/*   - Free old edit_data. */
/*   - Set new edit_data. */
/*   - Set is_deleted = 0. */
/*   - Increment active_count. */
/*   - Free incoming key copy (not needed). Return. */
/* - Step 3: If key exists and is active: */
/*   - Overwrite payload (free old edit_data, set new). */
/*   - Do NOT change size counts. */
/*   - Free incoming key copy. Return. */
/* - Step 4: If key is new: */
/*   - Allocate new payload with edit_data and is_deleted = 0. */
/*   - Increment active_count and total_payload_count. */
/*   - Insert as per existing non-full logic. */

/* === 3. NEW CORE FUNCTIONS (KHASH.H EQUIVALENTS) ========================== */

/* [ ] 3.1. btree_lookup_physical (internal) */
/* - Finds payload by key, regardless of is_deleted. */
/* - Returns BPayload* or NULL. */

/* [ ] 3.2. btree_lookup_active (kh_get / kh_exists) */
/* - Calls btree_lookup_physical. */
/* - Returns payload only if is_deleted == 0. Otherwise NULL. */

/* [ ] 3.3. btree_delete_lazy (kh_del) */
/* - Find active key. If found: */
/*   - Free edit_data. */
/*   - Set is_deleted = 1. */
/*   - Decrement active_count. */
/*   - Do NOT change total_payload_count. */
/* - Return 1 on success, 0 if not found. */

/* [ ] 3.4. btree_active_size (kh_size equivalent) */
/* - Return t->active_count (O(1)). */

/* [ ] 3.5. btree_clear (kh_clear equivalent) */
/* - Free all nodes via node_free_recursive. */
/* - Reset t->root = node_create(1). */
/* - Reset active_count and total_payload_count = 0. */
/* - Keep the BTree struct itself intact. */

/* [ ] 3.6. tree_free (kh_destroy equivalent) */
/* - Free all nodes and the BTree struct itself. */

/* === 4. ITERATION FUNCTIONS (TRAVERSAL) =================================== */

/* NOTE: Define iterator state struct: */
/*   typedef struct { BNode *leaf; int index; } BTreeIter; */

/* [ ] 4.1. btree_begin_active (kh_begin) */
/* - Descend to leftmost leaf. */
/* - Advance within leaf->keys until first active (is_deleted == 0). */
/* - Return BPayload* and fill BTreeIter with leaf + index. */
/* - If no active found, return NULL. */

/* [ ] 4.2. btree_next_active (kh_next) */
/* - Increment iterator index; if at end, move to next leaf. */
/* - Skip tombstoned entries. */
/* - Return next active payload or NULL (end marker). */

/* [ ] 4.3. btree_end_active (kh_end marker) */
/* - Define as NULL return from iterator functions. */
/* - Makes iteration termination explicit. */

/* === 5. COMPACTION / MAINTENANCE ========================================== */

/* [ ] 5.1. btree_compaction (copy-and-rebuild) */
/* - Create new_tree = tree_create(...). */
/* - Iterate over active entries (4.1/4.2). */
/* - Re-insert into new_tree with btree_insert. */
/* - Swap root and counters into t: */
/*   - root, active_count, total_payload_count, cmp_fn, dup_fn, free_fn. */
/* - Free old tree’s nodes (but not BTree struct itself). */
/* - Use periodically or when fragmentation is high. */

/* === 6. SPECIALIZATION FOR HEX EDITOR ===================================== */

/* [ ] 6.1. Key Type */
/* - Use file offset (size_t or uint64_t) as the key type. */

/* [ ] 6.2. Payload Type */
/* - Store unsigned char value in BPayload. */
/* - Add is_deleted flag. */

/* [ ] 6.3. Semantics */
/* - Multiple changes to same offset: only last value is stored. */
/* - Undo: tombstone the entry. */
/* - Redo/new edit at same offset: resurrect the tombstone and update value. */

/* [ ] 6.4. Accessors */
/* - Provide helper functions for offset-based get/put/delete to simplify use. */

/* === 7. GENERAL NOTES ===================================================== */
/* - Iterators are invalidated after clear() or compaction(). */
/* - Key equality is defined strictly by cmp_fn. */
/* - Memory management must be consistent for both keys and payloads. */
/* - Concurrency is not addressed; assume single-threaded use. */



/*
 * === Developer Notes: Extending Gtree ===
 *
 * The core B+ Tree implementation is already generic: it delegates all
 * key operations through user-supplied function pointers (cmp/dup/free).
 * String-specific logic only appears in example helpers and the demo app.
 *
 * Extension opportunities:
 *
 * 1. Custom Comparator
 *    - Already supported: comparator function is passed at tree creation.
 *    - You can define multiple comparator sets (string, int, offsets, etc.)
 *      and select which one to use by passing the right function pointers.
 *    - Ensure comparator defines a total ordering (no inconsistent results).
 *
 * 2. Generic Payload - see further notes below
 * ------------------
 * Context:
 *   - Current design assumes a narrow value type (e.g. integers or counts).
 *   - To make the tree reusable, values should be treated as opaque payloads
 *     that the tree stores but does not interpret.
 *
 * Implementation Approach:
 *   - Define a payload wrapper type:
 *       typedef struct {
 *           void *data;      // pointer to caller’s object
 *           size_t size;     // optional: size of data in bytes
 *       } gtree_val_t;
 *   - Store `gtree_val_t` (or just `void*`) in leaf entries.
 *   - Ownership model must be documented:
 *       Option A: Tree copies data on insert and frees it on delete.
 *       Option B: Tree stores caller’s pointer only; caller manages memory.
 *   - Choose based on whether you want the tree to *own* the data or just
 *     reference it.
 *
 * Example Usage:
 *   - Storing integers by value:
 *       int x = 42;
 *       gtree_val_t val = { &x, sizeof(int) };
 *       gtree_insert(tree, key, val);
 *
 *   - Storing dynamically allocated structs:
 *       Person *p = malloc(sizeof(Person));
 *       ... fill p ...
 *       gtree_val_t val = { p, sizeof(Person) };
 *       gtree_insert(tree, key, val);
 *
 * Benefits:
 *   - Tree can store arbitrary objects (records, blobs, strings).
 *   - Caller can decide ownership rules.
 *
 * Caution:
 *   - Be explicit: Who allocates and who frees?
 *   - If the tree copies payloads, it must also know how to free them
 *     (require a destructor callback from caller).
 *
 * General Note:
 *   - Comparator abstracts *ordering of keys*.
 *   - Payload abstraction generalizes *what values* are stored.
 *   - Together, these two changes make the tree a generic container library
 *     similar in spirit to C++ STL maps, but in plain C.
 *
 * 3. Iteration / Range Queries
 *    - Leaf nodes already form a linked list, enabling ordered traversal.
 *    - Provide an iterator API:
 *        tree_iterator_init(tree, start_key);
 *        tree_iterator_next(iter);
 *        tree_iterator_destroy(iter);
 *    - Enables range queries (find all between K1..K2) and ordered scans.
 *    - Implementation is straightforward: walk to first matching leaf, then
 *      step through leaf links and key slots until end condition met.
 *
 * 4. Testing Hooks
 *    - Current implementation has ad hoc printing for strings.
 *    - To make testing generic:
 *        * Add a user-supplied print function (like cmp/dup/free).
 *        * Implement validation utilities:
 *            verify_balance(tree);
 *            verify_ordering(tree);
 *            dump_structure(tree);
 *    - Keep debug code behind `#ifdef DEBUG` or compile-time flags so it
 *      does not affect performance in production.
 *
 * === Summary ===
 * Core tree is already generic and not string-bound.
 * Next steps to improve reusability:
 *   - Split out demo (word count) from core tree.
 *   - Provide multiple comparator sets (string, int, offset).
 *   - Add iterator API and testing/debug hooks.
 *   - Define clear ownership rules for payload data.
 */

/*
 * === Developer Notes: Making Gtree Fully Generic ===
 *
 * Objective:
 *   - Enable the B+ Tree to store arbitrary key/value pairs instead of being
 *     tied to string keys and a fixed "count" payload.
 *   - Make tree internals fully reusable in multiple contexts (maps, indexes,
 *     caches, hex editor edits, etc.).
 *
 * 1. Custom Comparator
 *    - The tree already supports a comparator via `cmp_func_t` passed to
 *      `tree_create()`.
 *    - Define multiple comparator sets for different key types:
 *        * Strings: strcmp-based
 *        * Integers/Offsets: numeric comparison
 *        * Custom structs: user-defined total ordering
 *    - Always ensure the comparator defines a consistent total order.
 *    - Example for integers:
 *        static int int_cmp(const void *a, const void *b) {
 *            int ia = *(const int*)a, ib = *(const int*)b;
 *            return (ia > ib) - (ia < ib);
 *        }
 *    - Pass the chosen comparator and corresponding dup/free functions to
 *      tree_create().
 *
 * 2. Generic Payload
 *    - Replace the fixed `count` in BPayload with an opaque `void *value`:
 *        typedef struct { void *key; void *value; } BPayload;
 *    - Ownership of keys and values must be defined by user:
 *        * Key: always duplicated/freed via tree callbacks
 *        * Value: optionally duplicated/freed via user-supplied callbacks
 *    - Extend tree ops with value duplication/free:
 *        typedef struct {
 *            cmp_func_t cmp_key;
 *            dup_func_t dup_key;
 *            free_func_t free_key;
 *            dup_func_t dup_val;  // optional
 *            free_func_t free_val; // optional
 *        } GTreeOps;
 *    - Benefits:
 *        * Tree can store any type of value (int*, structs, offsets, etc.)
 *        * Application logic (e.g., incrementing counts) lives outside tree.
 *
 * 3. Iteration / Range Queries
 *    - Leaf nodes form a linked list, enabling ordered traversal.
 *    - Provide a standard iterator API:
 *        tree_iterator_init(tree, start_key);
 *        tree_iterator_next(iter);
 *        tree_iterator_destroy(iter);
 *    - Enables:
 *        * Forward and reverse scans
 *        * Range queries (keys between K1 and K2)
 *        * Skipping tombstoned/filtered payloads
 *    - Implementation:
 *        * Step down to first leaf containing start_key
 *        * Move through leaf->next pointers and leaf indices until done
 *
 * 4. Testing / Debugging Hooks
 *    - Implement generic testing helpers:
 *        * verify_balance(tree);
 *        * verify_ordering(tree);
 *        * dump_structure(tree);
 *    - Make them optional or behind a compile-time flag (#ifdef DEBUG)
 *    - Provide a user-supplied print function for values to make debugging
 *      payload-agnostic
 *
 * 5. Migration Path / Minimal Example
 *    - Word-count demo (previously `int count`) -> generic payload:
 *        * Store `int*` as value in BPayload
 *        * Use dup/free callbacks for value to increment counts or replace
 *          values on update
 *    - Example insert logic:
 *        * Lookup key via tree
 *        * If exists:
 *            - Update value (e.g., increment int*)
 *        * Else:
 *            - Allocate BPayload
 *            - Duplicate key/value
 *            - Insert payload into tree
 *    - Tree internals do not need to know the type of value
 *
 * Summary:
 *    - Tree is fully generic for any key/value type
 *    - Comparators, dup/free callbacks allow maximum flexibility
 *    - Leaf links enable ordered iteration/range queries
 *    - Testing hooks ensure correctness for all payload types
 *    - Migration of existing apps (word count, hex editor edits) is straightforward
 */
 
 
/* --- Original payload --- */
typedef struct {
    void *key;
    int count;       // fixed payload
} BPayload;

/* --- Generic payload --- */
typedef struct {
    void *key;
    void *value;     // opaque pointer to arbitrary data (int*, struct*, etc.)
} BPayload;

/* --- Example: integer value callbacks --- */

// Key callbacks (e.g., for file offsets as integers)
static int int_cmp(const void *a, const void *b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}
static void* int_dup(const void *key) {
    int *copy = malloc(sizeof(int));
    if (!copy) { perror("malloc"); exit(EXIT_FAILURE); }
    *copy = *(const int*)key;
    return copy;
}
static void int_free(void *key) {
    free(key);
}

// Value callbacks (optional, for generic payload)
static void* int_val_dup(const void *val) {
    int *copy = malloc(sizeof(int));
    if (!copy) { perror("malloc"); exit(EXIT_FAILURE); }
    *copy = *(const int*)val;
    return copy;
}
static void int_val_free(void *val) {
    free(val);
}

/* --- Insert example for generic int payload --- */
static void insert_int(BTree *t, int key_val, int new_count) {
    int *key_copy = int_dup(&key_val);

    BNode *r = t->root;

    // Lookup existing payload (replace leaf_find_and_increment)
    BPayload *existing = NULL; // implement tree search returning BPayload*
    if (existing) {
        // Payload exists: update value
        int *v = (int*)existing->value;
        *v = new_count;  // or *v += 1 if increment
        int_free(key_copy);
        return;
    }

    // Payload does not exist: create new
    BPayload *p = malloc(sizeof(BPayload));
    if (!p) { perror("malloc"); exit(EXIT_FAILURE); }
    p->key = key_copy;                 // already duplicated
    p->value = int_val_dup(&new_count); // duplicate payload value

    // Insert into tree (replace btree_insert leaf logic)
    insert_nonfull(t, r, p->key); // ensure your insert_nonfull uses p->value as well
}

/* --- Notes for migration ---
 * 1. Everywhere you used `count`, replace with `value` pointer.
 * 2. Leaf search and insertion functions now deal with opaque `void*` value.
 * 3. Callbacks handle memory ownership for key and value separately.
 * 4. For hex editor edits:
 *      - key: file offset (int/size_t)
 *      - value: unsigned char edit byte
 *      - tombstone flag can be added to BPayload as is_deleted
 */

