/*
 * Gtree - Generic B+ Tree Implementation
 *
 * This is a generic B+ tree that supports arbitrary key types through
 * user-provided comparison, duplication, and free functions.
 * It supports:
 * - Leaf and internal nodes with ORDER_MAX_KEYS keys
 * - Payload storage (here, BPayload with a key and count)
 * - Forward and reverse traversal
 *
 * The tree can be reused for different payloads by providing appropriate
 * cmp_fn, dup_fn, and free_fn. For example:
 * - Strings (shown in example)
 * - Numeric offsets with associated data
 * - Complex structs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

// --- Function Pointer Definitions ---
// cmp_func_t: compare two keys (returns negative/zero/positive like strcmp)
// dup_func_t: duplicates a key for storage in the tree
// free_func_t: frees a key previously duplicated

typedef int (*cmp_func_t)(const void *a, const void *b);
typedef void* (*dup_func_t)(const void *key);
typedef void (*free_func_t)(void *key);

// --- B+ Tree Structure Definitions ---

#define ORDER_MAX_KEYS 16
#define KEYS_ARR_SIZE (ORDER_MAX_KEYS + 1)     // allow temporary overflow during split
#define CHILDS_ARR_SIZE (ORDER_MAX_KEYS + 2)   // allow temporary overflow during split

typedef struct {
    void *key;   // pointer to the key data (user-managed type)
    int count;   // example payload; could be replaced with user struct/data
} BPayload;

typedef struct BNode {
    int is_leaf;           // 1 = leaf, 0 = internal node
    int num_keys;          // number of keys currently stored
    void **keys;           // array of keys or payload pointers
    struct BNode **children; // child pointers (NULL for leaf)
    struct BNode *next;    // leaf: next sibling (for iteration)
    struct BNode *prev;    // leaf: previous sibling (for reverse iteration)
} BNode;

typedef struct {
    BNode *root;           // pointer to root node
    cmp_func_t cmp_fn;     // key comparison function
    dup_func_t dup_fn;     // key duplication function
    free_func_t free_fn;   // key free function
} BTree;

// --- String-Specific Utility Functions (Example Implementation) ---

// Compare two strings
static int string_cmp(const void *a, const void *b) {
    return strcmp((const char*)a, (const char*)b);
}

// Duplicate a string
static void* string_dup(const void *key) {
    return strdup((const char*)key);
}

// Free a string
static void string_free(void *key) {
    free(key);
}

// === Integer key functions ===

static int int_cmp(const void *a, const void *b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);  // returns +1, 0, -1
}

static void* int_dup(const void *key) {
    int *copy = malloc(sizeof(int));
    if (!copy) return NULL;
    *copy = *(const int*)key;
    return copy;
}

static void int_free(void *key) {
    free(key);
}

// --- Core Tree Utility Functions ---

// Allocate a new node (leaf or internal)
static BNode *node_create(int is_leaf) {
    BNode *n = calloc(1, sizeof(BNode));
    if (!n) { perror("calloc"); exit(EXIT_FAILURE); }
    n->is_leaf = is_leaf ? 1 : 0;
    n->keys = calloc(KEYS_ARR_SIZE, sizeof(void*));
    n->children = calloc(CHILDS_ARR_SIZE, sizeof(BNode*));
    if (!n->keys || !n->children) { perror("calloc"); exit(EXIT_FAILURE); }
    return n;
}

// Recursively free a node and all its children and payloads
static void node_free_recursive(BTree *t, BNode *n) {
    if (!n) return;
    if (n->is_leaf) {
        for (int i = 0; i < n->num_keys; ++i) {
            BPayload *p = (BPayload*)n->keys[i];
            if (p) {
                t->free_fn(p->key);
                free(p);
            }
        }
    } else {
        for (int i = 0; i <= n->num_keys; ++i) {
            if (n->children[i]) node_free_recursive(t, n->children[i]);
        }
    }
    free(n->keys);
    free(n->children);
    free(n);
}

// Create a new B+ tree and initialize function pointers
static BTree *tree_create(cmp_func_t cmp, dup_func_t dup, free_func_t key_free) {
    BTree *t = malloc(sizeof(BTree));
    if (!t) { perror("malloc"); exit(EXIT_FAILURE); }
    t->root = node_create(1);  // start with a leaf root
    t->cmp_fn = cmp;
    t->dup_fn = dup;
    t->free_fn = key_free;
    return t;
}

// Free the entire tree
static void tree_free(BTree *t) {
    if (!t) return;
    if (t->root) node_free_recursive(t, t->root);
    free(t);
}

// --- Tree Search and Structural Functions ---

// Find the index in node->keys to route a key to the correct child
// Returns the first index i such that node->keys[i] > key
// Keys <= key are routed to children[i]
static int node_find_index(BTree *t, BNode *n, const void *key) {
    int low = 0;
    int high = n->num_keys;
    int mid;

    while (low < high) {
        mid = low + (high - low) / 2;
        if (t->cmp_fn(n->keys[mid], key) <= 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

// Split a full child node into two nodes and update the parent
static void split_child(BTree *t, BNode *parent, int idx) {
    BNode *child = parent->children[idx];
    assert(child != NULL);

    BNode *newn = node_create(child->is_leaf);
    int total = child->num_keys;
    int mid = total / 2;

    if (child->is_leaf) {
        // LEAF SPLIT: move half the keys to new leaf
        int new_count = total - mid;
        for (int j = 0; j < new_count; ++j) {
            newn->keys[j] = child->keys[mid + j];
            child->keys[mid + j] = NULL;
        }
        newn->num_keys = new_count;
        child->num_keys = mid;

        // Update sibling pointers for iteration
        newn->next = child->next;
        if (newn->next) newn->next->prev = newn;
        newn->prev = child;
        child->next = newn;

        // Insert promotion key into parent and adjust child pointers
        for (int j = parent->num_keys; j >= idx + 1; --j) parent->children[j+1] = parent->children[j];
        parent->children[idx+1] = newn;
        for (int j = parent->num_keys - 1; j >= idx; --j) parent->keys[j+1] = parent->keys[j];
        parent->keys[idx] = ((BPayload*)newn->keys[0])->key; // promotion key (alias)
        parent->num_keys++;
    } else {
        // INTERNAL SPLIT: move half keys and children to new internal node
        void *promoted_key = child->keys[mid];
        int right_count = total - (mid + 1);
        for (int j = 0; j < right_count; ++j) newn->keys[j] = child->keys[mid + 1 + j];
        for (int j = 0; j <= right_count; ++j) newn->children[j] = child->children[mid + 1 + j];
        newn->num_keys = right_count;
        child->num_keys = mid;

        // Insert new node and promoted key into parent
        for (int j = parent->num_keys; j >= idx + 1; --j) parent->children[j+1] = parent->children[j];
        parent->children[idx+1] = newn;
        for (int j = parent->num_keys - 1; j >= idx; --j) parent->keys[j+1] = parent->keys[j];
        parent->keys[idx] = promoted_key;
        parent->num_keys++;
    }
}

// Search a leaf for an existing key and increment its count if found
// Returns the payload if found, NULL if not
static BPayload *leaf_find_and_increment(BTree *t, BNode *leaf, const void *key) {
    for (int i = 0; i < leaf->num_keys; ++i) {
        BPayload *p = (BPayload*)leaf->keys[i];
        if (t->cmp_fn(p->key, key) == 0) {
            p->count++;
            return p;
        }
    }
    return NULL;
}

// Insert a key into a node that is guaranteed to be non-full
static void insert_nonfull(BTree *t, BNode *node, void *key_ptr) {
    if (node->is_leaf) {
        // If key exists, update payload
        BPayload *existing_payload = leaf_find_and_increment(t, node, key_ptr);
        if (existing_payload) {
            t->free_fn(key_ptr);
            return;
        }

        // Key is new: create payload
        BPayload *new_payload = malloc(sizeof(BPayload));
        if (!new_payload) { perror("malloc"); exit(EXIT_FAILURE); }
        new_payload->key = t->dup_fn(key_ptr);
        new_payload->count = 1;

        t->free_fn(key_ptr);

        // Find insert position (linear search)
        int i = node->num_keys - 1;
        while (i >= 0 && t->cmp_fn(((BPayload*)node->keys[i])->key, new_payload->key) > 0) {
            node->keys[i+1] = node->keys[i];
            i--;
        }
        int pos = i + 1;

        // Insert new payload
        node->keys[pos] = new_payload;
        node->num_keys++;
    } else {
        // Internal node: find child and recurse
        int i = node_find_index(t, node, key_ptr);
        BNode *child = node->children[i];
        assert(child != NULL);

        // Split child if full
        if ((size_t)child->num_keys == ORDER_MAX_KEYS) {
            split_child(t, node, i);
            i = node_find_index(t, node, key_ptr); // re-route after split
        }

        insert_nonfull(t, node->children[i], key_ptr);
    }
}

// --- Public Tree API ---

// Insert a key into the tree; duplicates are incremented
static void btree_insert(BTree *t, const void *token) {
    if (!token) return;

    void *key_copy = t->dup_fn(token);
    if (!key_copy) { perror("dup_fn"); exit(EXIT_FAILURE); }

    BNode *r = t->root;
    if ((size_t)r->num_keys == ORDER_MAX_KEYS) {
        // Root is full: create new root and split
        BNode *s = node_create(0);
        s->children[0] = r;
        t->root = s;
        split_child(t, s, 0);
        insert_nonfull(t, s, key_copy);
    } else {
        insert_nonfull(t, r, key_copy);
    }
}

// --- Traversal and Printing Functions ---

static void print_payload(BPayload *p) {
    printf("%s (%d)\n", (const char*)p->key, p->count);
}

// Forward iteration (leaf->next) for printing all payloads in order
static void btree_print(BTree *t) {
    if (!t || !t->root) return;
    BNode *n = t->root;
    while (!n->is_leaf) n = n->children[0];
    while (n) {
        for (int i = 0; i < n->num_keys; ++i) print_payload((BPayload*)n->keys[i]);
        n = n->next;
    }
}

// Reverse iteration (leaf->prev) for printing all payloads in reverse order
static void btree_print_reverse(BTree *t) {
    if (!t || !t->root) return;
    BNode *n = t->root;
    while (!n->is_leaf) n = n->children[n->num_keys];
    while (n) {
        for (int i = n->num_keys - 1; i >= 0; --i) print_payload((BPayload*)n->keys[i]);
        n = n->prev;
    }
}

// --- Example File Processing ---

// Reads words from a file and inserts them into the tree
static void process_file(BTree *t, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { 
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    
    char buf[4096];
    while (fscanf(f, "%4095s", buf) == 1) {
        if (buf[0] == '\0') continue;
        btree_insert(t, buf);
    }
    if (ferror(f)) { 
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 2;
    }

    BTree *tree = tree_create(string_cmp, string_dup, string_free);
    
    process_file(tree, argv[1]);
    
    printf("Forwards...\n");
    btree_print(tree);
    printf("\nBackwards...\n\n");
    btree_print_reverse(tree);
    
    tree_free(tree);
    return 0;
}

