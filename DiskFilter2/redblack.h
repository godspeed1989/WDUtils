#ifndef _REDBLACK_H_
#define _REDBLACK_H_

#include "common.h"

typedef enum { RED, BLACK } rb_color;

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define RB_KEY_T LONGLONG

typedef struct _rb_node_t {
    struct _rb_node_t *parent;
    struct _rb_node_t *right;
    struct _rb_node_t *left;
    rb_color color;
    /* the node key is the base */
    RB_KEY_T key;

    /* custom data field */
    void *client;
}rb_node_t;

typedef struct _rb_tree_t {
    struct _rb_node_t *root;
    /* Sentinel NIL node to mark leaves rather than NULL.  Simplifies
     * node deletion; see CLR.
     *
     * While the leaves in one tree can all share this leaf node,
     * we need to write to its parent on certain operations, so we
     * need a separate copy per tree.
     */
    struct _rb_node_t NIL_node;
    void (*free_payload_func)(void*);
}rb_tree_t;

/* Allocate a new, empy tree.  free_payload_func, if non-null, will be called
 * to free the client field whenever a node is freed.
 */
void
rb_tree_create(rb_tree_t *tree, void (*free_payload_func)(void*));

/* Remove and free all nodes in the tree and free the tree itself */
void
rb_tree_destroy(rb_tree_t *tree);

/* Insert a node into the tree.  If an existing node overlaps
 * returns that node; else, adds a new node and returns NULL.
 */
rb_node_t *
rb_insert(rb_tree_t *tree, RB_KEY_T key, void *client);

/* Find the node with key */
rb_node_t *
rb_find(rb_tree_t *tree, RB_KEY_T key);

/* Remove a node from the RB tree */
void
rb_delete(rb_tree_t *tree, rb_node_t *node, BOOLEAN free);

/* Remove and free all nodes in the tree */
void
rb_clear(rb_tree_t *tree);

#endif /* _REDBLACK_H_ */
