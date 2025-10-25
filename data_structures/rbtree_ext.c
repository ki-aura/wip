#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rbtree.h"

void test_ext(void) {
	struct Animal *found;
    struct Animal *p = malloc(sizeof(*p));
    if (!p) { perror("malloc"); return; }
    

printf("in ext ******\n");


    p->key = 42;
    p->name = "Tiger";

    RB_INSERT(AnimalTree_key, &keyHead, p);
    printf("[test_ext] After insert, tree size: %d\n", RB_SIZE());

    struct Animal nsearch = {.key = 8};
    found = RB_FIND(AnimalTree_key, &keyHead, &nsearch);
    if (found) {
		RB_REMOVE(AnimalTree_key, &keyHead, found);
		printf("[test_ext] After delete, tree size: %d\n", RB_SIZE());
    }


///////////////////////////////////////

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
    
printf("end of replicated main tests ******\n");

///////////////////////////////////////


}
