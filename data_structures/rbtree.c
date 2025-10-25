#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rbtree.h"


/* ---------------------- RB Function Prototypes & Wrappers ---------------------- */

int Animal_Keycmp(struct Animal *a, struct Animal *b);
struct Animal *RB_INSERT_WRAPPER(struct AnimalTree_key *head, struct Animal *node);
struct Animal *RB_REMOVE_WRAPPER(struct AnimalTree_key *head, struct Animal *node);


/* -------------------------- Global Trees -------------------------- */
struct AnimalTree_key keyHead = RB_INITIALIZER(&keyHead);
int tree_size_key = 0;

/* -------------------------- Wrappers and Size -------------------------- */

struct Animal *RB_INSERT_KEY(struct AnimalTree_key *head, struct Animal *node) {
    struct Animal *res = AnimalTree_key_RB_INSERT(head, node);
    if (res == NULL) tree_size_key++;
    return res;
}

struct Animal *RB_REMOVE_KEY(struct AnimalTree_key *head, struct Animal *node) {
    struct Animal *res = AnimalTree_key_RB_REMOVE(head, node);
    if (res != NULL) tree_size_key--;
    return res;
}

void RB_CLEAR_TREE(struct AnimalTree_key *head) {
    struct Animal *p;
    while ((p = RB_MIN(AnimalTree_key, head)) != NULL) {
        RB_REMOVE(AnimalTree_key, head, p);
        free(p);
    }
}

int RB_SIZE(void) {
	return tree_size_key;
}
	
/* ---------------------- Generate RB-tree Functions ---------------------- */

int Animal_Keycmp(struct Animal *a, struct Animal *b) {
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return 0;
}

RB_GENERATE(AnimalTree_key, Animal, by_key, Animal_Keycmp)

   

