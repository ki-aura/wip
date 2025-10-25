#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rbtree_ext.h"
#include "rbtree.h"

// compile using cc -o rbtree rbtree_main.c rbtree_ext.c rbtree.c

int main(void) {
    /* Initialize the global trees at runtime */

    struct Animal *p, *found;
    int i;

    const char *names[] = {"Dog", "Cat", "Horse", "Mouse", "Elephant"};
    int keys[] = {5, 2, 8, 1, 3};

    /* Insert Nodes */
    for (i = 0; i < 5; i++) {
        p = malloc(sizeof(*p));
        if (!p) { perror("malloc"); return 1; }

        p->key = keys[i];
        p->name = names[i];

        if (RB_INSERT(AnimalTree_key, &keyHead, p) != NULL) {
            fprintf(stderr, "Duplicate key or name: %d, %s\n", p->key, p->name);
            free(p);
        }
    }

    /* Find example */
    struct Animal search = {.key = 3};
    found = RB_FIND(AnimalTree_key, &keyHead, &search);
    if (found)
        printf("Found by key %d -> %s\n", found->key, found->name);

  	/* Find the smallest node >= key (lower bound) */
    search.key = 4; // 4 doesn't exist; should return key 5
    found = RB_NFIND(AnimalTree_key, &keyHead, &search);
    if (found)
        printf("Next >= 4 by key: %d -> %s\n", found->key, found->name);

    /* ---------------------- Remove Node ---------------------- */
    search.key = 2;
    found = RB_FIND(AnimalTree_key, &keyHead, &search);
    if (found) {
        /* Must remove from BOTH trees before freeing */
        RB_REMOVE(AnimalTree_key, &keyHead, found);
        printf("Removed key %d / name %s\n", found->key, found->name);
        free(found);
    }

    /* Forward iteration by key using RB_FOREACH */
    printf("All animals by key:\n");
    RB_FOREACH(p, AnimalTree_key, &keyHead) {
        printf("  %d -> %s\n", p->key, p->name);
    }

    // Reverse iteration by name using RB_FOREACH_REVERSE 
    printf("All animals by name (reverse):\n");
    RB_FOREACH_REVERSE(p, AnimalTree_key, &keyHead) {
        printf("  %s -> %d\n", p->name, p->key);
    }

    /* size */
    printf("size of tree is %d\n", RB_SIZE());

    /* Call external test */
    test_ext();

    /* Manual forward iteration by key */
    printf("Manual iteration by key:\n");
    for (p = RB_MIN(AnimalTree_key, &keyHead);
         p != NULL;
         p = RB_NEXT(AnimalTree_key, &keyHead, p)) {
        printf("%d -> %s\n", p->key, p->name);
    }

    /* Manual reverse iteration by name */
    printf("Manual iteration (backwards):\n");
    for (p = RB_MAX( AnimalTree_key, &keyHead);
         p != NULL;
         p = RB_PREV( AnimalTree_key, &keyHead, p)) {
        printf("%s -> %d\n", p->name, p->key);
    }
    

    /* Cleanup */
    RB_CLEAR_TREE(&keyHead);
    printf("checking for empty %d\n", RB_EMPTY(&keyHead));
    printf("size of tree is %d\n", RB_SIZE());

    return 0;
}


