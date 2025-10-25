#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"   /* BSD-style sys/tree.h, vendored for portability */

/*
 * Example: Dual Red-Black Tree in C
 *
 * This program demonstrates how to:
 * 1. Define a struct suitable for RB trees.
 * 2. Maintain two different RB trees over the same data:
 *      - One ordered by integer key.
 *      - One ordered by string name.
 * 3. Insert, find, iterate, and remove nodes safely.
 *
 * Designed as a learning reference for someone new to RB trees.
 */

/* -------------------------- Node Definition -------------------------- */

/* 
 * Each node has two RB_ENTRY fields. Each RB_ENTRY stores the internal
 * pointers and color bits for a specific tree. 
 * - by_key: used in the tree ordered by `key`.
 * - by_name: used in the tree ordered by `name`.
 * You can add additional payload fields freely.
 */
struct Animal {
    RB_ENTRY(Animal) by_key;    /* link field for key-ordered tree */
    RB_ENTRY(Animal) by_name;   /* link field for name-ordered tree */
    int key;                    /* integer key */
    const char *name;           /* string name */
};

/* -------------------------- Tree Heads -------------------------- */

/* Each tree needs its own head (root pointer + metadata) */
RB_HEAD(AnimalTree_key, Animal);   /* head for key-ordered tree */
RB_HEAD(AnimalTree_name, Animal);  /* head for name-ordered tree */

/* ---------------------- Comparison Functions ---------------------- */

/* Compare nodes by integer key */
static inline int Animal_Keycmp(struct Animal *a, struct Animal *b) {
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return 0;
}

/* Compare nodes by string name */
static inline int Animal_Strcmp(struct Animal *a, struct Animal *b) {
    return strcmp(a->name, b->name);
}

/* ---------------------- Generate RB-tree Functions ---------------------- */

/*
 * RB_PROTOTYPE_STATIC:
 * - Declares the functions/macros for this tree type.
 * - Must refer to the correct RB_ENTRY field for the tree.
 * RB_GENERATE_STATIC:
 * - Expands the functions/macros for actual use.
 * - Only one copy per C file; multiple C files require non-static versions.
 */
RB_PROTOTYPE(AnimalTree_key, Animal, by_key, Animal_Keycmp)
RB_GENERATE(AnimalTree_key, Animal, by_key, Animal_Keycmp)

RB_PROTOTYPE(AnimalTree_name, Animal, by_name, Animal_Strcmp)
RB_GENERATE(AnimalTree_name, Animal, by_name, Animal_Strcmp)

/* -------------------------- Main Program -------------------------- */

int main(void) {
    /* Initialize the tree heads. Can use RB_INITIALIZER for static init. */
    struct AnimalTree_key keyHead = RB_INITIALIZER(&keyHead);
    struct AnimalTree_name nameHead = RB_INITIALIZER(&nameHead);

    struct Animal *p, *found;
    int i;

    const char *names[] = {"Dog", "Cat", "Horse", "Mouse", "Elephant"};
    int keys[] = {5, 2, 8, 1, 3};

    /* ---------------------- Insert Nodes ---------------------- */
    for (i = 0; i < 5; i++) {
        p = malloc(sizeof(*p));
        if (!p) { perror("malloc"); return 1; }

        p->key = keys[i];
        p->name = names[i];

        /* Insert into both trees. Check for duplicates. */
        if (RB_INSERT(AnimalTree_key, &keyHead, p) != NULL ||
            RB_INSERT(AnimalTree_name, &nameHead, p) != NULL) {
            fprintf(stderr, "Duplicate key or name: %d, %s\n", p->key, p->name);
            free(p);
        }
    }

    /* ---------------------- Find Nodes ---------------------- */
    struct Animal search = {.key = 3};
    found = RB_FIND(AnimalTree_key, &keyHead, &search);
    if (found)
        printf("Found by key %d -> %s\n", found->key, found->name);

    /* Find the smallest node >= key (lower bound) */
    search.key = 4; // 4 doesn't exist; should return key 5
    found = RB_NFIND(AnimalTree_key, &keyHead, &search);
    if (found)
        printf("Next >= 4 by key: %d -> %s\n", found->key, found->name);

    /* Find by name */
    struct Animal searchName = {.name = "Mouse"};
    found = RB_FIND(AnimalTree_name, &nameHead, &searchName);
    if (found)
        printf("Found by name: %s -> %d\n", found->name, found->key);

    /* ---------------------- Remove Node ---------------------- */
    search.key = 2;
    found = RB_FIND(AnimalTree_key, &keyHead, &search);
    if (found) {
        /* Must remove from BOTH trees before freeing */
        RB_REMOVE(AnimalTree_key, &keyHead, found);
        RB_REMOVE(AnimalTree_name, &nameHead, found);
        printf("Removed key %d / name %s\n", found->key, found->name);
        free(found);
    }

    /* ---------------------- Iteration Examples ---------------------- */

    /* Forward iteration by key using RB_FOREACH */
    printf("All animals by key:\n");
    RB_FOREACH(p, AnimalTree_key, &keyHead) {
        printf("  %d -> %s\n", p->key, p->name);
    }

    /* Reverse iteration by name using RB_FOREACH_REVERSE */
    printf("All animals by name (reverse):\n");
    RB_FOREACH_REVERSE(p, AnimalTree_name, &nameHead) {
        printf("  %s -> %d\n", p->name, p->key);
    }

    /* Manual forward iteration by key */
    printf("Manual iteration by key:\n");
    for (p = RB_MIN(AnimalTree_key, &keyHead);
         p != NULL;
         p = RB_NEXT(AnimalTree_key, &keyHead, p)) {
        printf("%d -> %s\n", p->key, p->name);
    }

    /* Manual reverse iteration by name */
    printf("Manual iteration by name (backwards):\n");
    for (p = RB_MAX(AnimalTree_name, &nameHead);
         p != NULL;
         p = RB_PREV(AnimalTree_name, &nameHead, p)) {
        printf("%s -> %d\n", p->name, p->key);
    }

    /* ---------------------- Cleanup ---------------------- */
    while ((p = RB_MIN(AnimalTree_key, &keyHead)) != NULL) {
        RB_REMOVE(AnimalTree_key, &keyHead, p);
        RB_REMOVE(AnimalTree_name, &nameHead, p);
        free(p);
    }

    return 0;
}
