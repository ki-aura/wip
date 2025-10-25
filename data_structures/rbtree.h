#ifndef RBTREE_H
#define RBTREE_H

/*
 * -------------------------
 * BSD Red-Black Tree (RB) API Quick Reference
 * -------------------------
 *
 * == Core Declarations ==
 * RB_HEAD(name, type)           : Declare tree head type "struct name { type *rbh_root; }".
 * RB_ENTRY(type)                : Embed in node struct; provides left/right/parent/color fields.
 *
 * == Initialization ==
 * RB_INITIALIZER(root)          : Static initializer ({ NULL }); e.g. RB_HEAD(...) h = RB_INITIALIZER(&h);
 * RB_INIT(head)                 : Runtime initialize tree (sets root = NULL).
 * RB_ROOT(head)                 : Get root pointer.
 * RB_EMPTY(head)                : True if tree is empty.
 *
 * == Prototypes/Generation ==
 * RB_PROTOTYPE(name, type, field, cmp)
 *    Emit external prototypes (use in header).
 * RB_PROTOTYPE_STATIC(name, type, field, cmp)
 *    Emit static prototypes (internal linkage only).
 * RB_GENERATE(name, type, field, cmp)
 *    Generate implementations (linkable across .c files).
 * RB_GENERATE_STATIC(name, type, field, cmp)
 *    Generate static implementations (usable only in one .c).
 *
 * == Core Operations ==
 * RB_INSERT(name, head, elm)    : Insert node; returns NULL on success or existing node on duplicate.
 * RB_REMOVE(name, head, elm)    : Remove node; returns removed element or NULL if absent.
 * RB_FIND(name, head, elm)      : Find exact match or NULL.
 * RB_NFIND(name, head, elm)     : Find least element >= key (lower bound).
 * RB_NEXT(name, head, elm)      : In-order successor of elm (or NULL).
 * RB_PREV(name, head, elm)      : In-order predecessor of elm (or NULL).
 * RB_MIN(name, head)            : Smallest element in tree (or NULL).
 * RB_MAX(name, head)            : Largest element in tree (or NULL).
 *
 * == Iteration ==
 * RB_FOREACH(var, name, head)                 : Forward in-order iteration.
 * RB_FOREACH_FROM(var, name, start)           : Forward iteration starting at 'start'.
 * RB_FOREACH_SAFE(var, name, head, tmp)       : Forward iteration safe for removals.
 * RB_FOREACH_REVERSE(var, name, head)         : Reverse in-order iteration.
 * RB_FOREACH_REVERSE_FROM(var, name, start)   : Reverse iteration starting at 'start'.
 * RB_FOREACH_REVERSE_SAFE(var, name, head, t) : Reverse iteration safe for removals.
 *
 * == Additional / Less Common ==
 * RB_INSERT_NEXT(name, head, elm, after)      : Insert immediately after a given node.
 * RB_INSERT_PREV(name, head, elm, before)     : Insert immediately before a given node.
 * RB_REINSERT(name, head, elm)                : Reinsert if node's key has changed.
 *
 * == Internals (normally not called directly) ==
 * RB_MINMAX(name, head, flag)   : Backend for RB_MIN/RB_MAX.
 * RB_NEGINF / RB_INF            : Flags for RB_MINMAX.
 * RB_INSERT_COLOR / RB_REMOVE_COLOR
 *                               : Internal balancing fixup functions.
 * RB_ROTATE_LEFT / RB_ROTATE_RIGHT
 *                               : Internal tree rotations.
 * RB_AUGMENT / RB_UPDATE_AUGMENT / RB_AUGMENT_CHECK
 *                               : Hooks for maintaining augmented data.
 *
 * -------------------------
 * Important Notes
 * -------------------------
 * - Use RB_PROTOTYPE + RB_GENERATE (non-static) if functions are shared across multiple .c files.
 *   Use *_STATIC versions only if the tree is private to one .c file.
 *
 * - RB_INITIALIZER is for static/global compile-time initialization.
 *   Use RB_INIT(&head) to init dynamically at runtime.
 *
 * - RB_NEXT / RB_PREV macros expand to use only the node internally
 *   (head parameter is for API symmetry).
 *
 * - RB_INSERT returns NULL when insertion succeeded, or the existing node if a duplicate key exists.
 *
 * - All iteration macros expand to for-loops; SAFE variants allow node deletion inside loop.
 */

#include "tree.h"   /* BSD-style sys/tree.h, vendored for portability */

/* -------------------------- Node Definition -------------------------- */

struct Animal {
    RB_ENTRY(Animal) by_key;    /* link field for key-ordered tree */
    int key;
    const char *name;
};

/* -------------------------- Tree Heads (globals) -------------------------- */

RB_HEAD(AnimalTree_key, Animal);   // defines struct AnimalTree_key
extern struct AnimalTree_key edits;

/* ---------------------- RB Prototype Static ---------------------- */

RB_PROTOTYPE(AnimalTree_key, Animal, by_key, Animal_Keycmp)

/* ---------------------- Comparison Functions ---------------------- */



/* Undef / redefine macros to use wrappers */
#undef RB_INSERT
#undef RB_REMOVE

struct Animal *RB_INSERT_KEY(struct AnimalTree_key *head, struct Animal *node);
struct Animal *RB_REMOVE_KEY(struct AnimalTree_key *head, struct Animal *node);

#define RB_INSERT(headType, head, node) RB_INSERT_KEY(head, node)
#define RB_REMOVE(headType, head, node) RB_REMOVE_KEY(head, node)

extern int RB_SIZE(void);
void RB_CLEAR_TREE(struct AnimalTree_key *head);


#endif 
