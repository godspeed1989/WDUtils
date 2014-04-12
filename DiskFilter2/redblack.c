#include "redblack.h"

#define NIL(tree) (&(tree)->NIL_node)

/* Allocate a new node */
static rb_node_t *
rb_new_node(rb_tree_t *tree, RB_KEY_T key, void *client)
{
    rb_node_t *node = (rb_node_t *) RB_ALLOC(sizeof(rb_node_t));
    ASSERT(node != NULL);

    if (node != NULL) {
        node->parent = NIL(tree);
        node->right = NIL(tree);
        node->left = NIL(tree);
        node->color = RED;
        node->key = key;
        node->client = client;
    }

    return node;
}

/* Free a node */
static void
rb_free_node(rb_tree_t *tree, rb_node_t *node, BOOLEAN free_payload)
{
    if (tree != NULL && free_payload && tree->free_payload_func != NULL)
        (tree->free_payload_func)(node->client);
    RB_FREE(node);
}

static void
rb_clear_helper(rb_tree_t *tree, rb_node_t *node)
{
    if (node != NIL(tree)) {
        rb_clear_helper(tree, node->left);
        rb_clear_helper(tree, node->right);
        rb_free_node(tree, node, TRUE/*free payload*/);
    }
}

/* Free all nodes in the tree */
void
rb_clear(rb_tree_t *tree)
{
    rb_clear_helper(tree, tree->root);
    tree->root = NIL(tree);
}

rb_node_t *
rb_find(rb_tree_t *tree, RB_KEY_T key)
{
    rb_node_t *iter = tree->root;

    while (iter != NIL(tree)) {
        if (key == iter->key) {
            return iter;
        }
        else if (key < iter->key) {
            iter = iter->left;
        }
        else {
            iter = iter->right;
        }
    }

    return NULL;
}

static void
rb_right_rotate(rb_tree_t *tree, rb_node_t *y)
{
    rb_node_t *x = y->left;
    ASSERT(y != NIL(tree));

    y->left = x->right;
    if (x->right != NIL(tree)) {
        x->right->parent = y;
    }

    x->parent = y->parent;
    if (y->parent == NIL(tree)) {
        tree->root = x;
    }
    else if (y == y->parent->left) {
        y->parent->left = x;
    }
    else {
        y->parent->right = x;
    }

    x->right = y;
    y->parent = x;
}

static void
rb_left_rotate(rb_tree_t *tree, rb_node_t *x)
{
    rb_node_t *y = x->right;
    ASSERT(x != NIL(tree));

    x->right = y->left;
    if (y->left != NIL(tree)) {
        y->left->parent = x;
    }

    y->parent = x->parent;
    if (x->parent == NIL(tree)) {
        tree->root = y;
    }
    else if (x == x->parent->left) {
        x->parent->left = y;
    }
    else {
        x->parent->right = y;
    }

    y->left = x;
    x->parent = y;
}

static void
rb_delete_fixup(rb_tree_t *tree, rb_node_t *x)
{
    while (x != tree->root && x->color == BLACK) {
        if (x == x->parent->left) {
            rb_node_t *w = x->parent->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                rb_left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->parent;
            }
            else {
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    rb_right_rotate(tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                rb_left_rotate(tree, x->parent);
                x = tree->root;
            }
        }
        else {
            rb_node_t *w = x->parent->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                rb_right_rotate(tree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == BLACK && w->left->color == BLACK) {
                w->color = RED;
                x = x->parent;
            }
            else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    rb_left_rotate(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                rb_right_rotate(tree, x->parent);
                x = tree->root;
            }
        }
    }

    x->color = BLACK;
}

/* Find immediate successor of node 'x' */
static rb_node_t *
rb_successor(rb_tree_t *tree, rb_node_t *x)
{
    if (x->right != NIL(tree)) {
        x = x->right;
        while (x->left != NIL(tree)) {
            x = x->left;
        }
        return x;
    }
    else {
        rb_node_t *y = x->parent;
        while (y != NIL(tree) && x == y->right) {
            x = y;
            y = y->parent;
        }
        return y;
    }
}

/* Remove a node from the RB tree */
void
rb_delete(rb_tree_t *tree, rb_node_t *z, BOOLEAN free)
{
    rb_node_t *y, *x;
    void *client_tmp;
    ASSERT(z != NIL(tree));

    if (z->left == NIL(tree) || z->right == NIL(tree)) {
        y = z;
    }
    else {
        y = rb_successor(tree, z);
    }

    x = (y->left != NIL(tree)) ? y->left : y->right;
    x->parent = y->parent;

    if (y->parent == NIL(tree)) {
        tree->root = x;
    }
    else if (y == y->parent->left) {
        y->parent->left = x;
    }
    else {
        y->parent->right = x;
    }

    if (y != z) {
        /* y's contents are being moved into z's node */
        client_tmp = z->client;
        z->key = y->key;
        z->client = y->client;
        y->client = client_tmp;
    }

    if (y->color == BLACK) {
        rb_delete_fixup(tree, x);
    }

    rb_free_node(tree, y, free/*free payload*/);
}

/* Binary tree insertion.  First step when inserting a node into
 * an RB tree.
 */
static rb_node_t *
bt_insert(rb_tree_t *tree, rb_node_t *node)
{
    rb_node_t *iter = NIL(tree);
    rb_node_t **p_iter = &tree->root;

    RB_KEY_T nbase = node->key;

    while (*p_iter != NIL(tree)) {
        RB_KEY_T ibase;

        iter = *p_iter;
        ibase = iter->key;

        if (nbase == ibase) {
            return iter;
        }
        else if (nbase < ibase) {
            p_iter = &(iter->left);
        }
        else {
            p_iter = &(iter->right);
        }
    }

    *p_iter = node;
    node->parent = iter;

    /* successful insertion */
    return NULL;
}


/* Insert node 'x' into the RB tree. */
static rb_node_t *
rb_insert_helper(rb_tree_t *tree, rb_node_t *x)
{
    rb_node_t *node = bt_insert(tree, x);
    if (node != NULL) {
        /* the new node overlaps with an existing node */
        return node;
    }

    while (x != tree->root && x->parent->color == RED) {
        if (x->parent == x->parent->parent->left) {
            rb_node_t *y = x->parent->parent->right;
            if (y != NIL(tree) && y->color == RED) {
                x->parent->color = BLACK;
                y->color = BLACK;
                x->parent->parent->color = RED;
                x = x->parent->parent;
            }
            else {
                if (x == x->parent->right) {
                    x = x->parent;
                    rb_left_rotate(tree, x);
                }
                x->parent->color = BLACK;
                x->parent->parent->color = RED;
                rb_right_rotate(tree, x->parent->parent);
            }
        }
        else {
            rb_node_t *y = x->parent->parent->left;
            if (y != NIL(tree) && y->color == RED) {
                x->parent->color = BLACK;
                y->color = BLACK;
                x->parent->parent->color = RED;
                x = x->parent->parent;
            }
            else {
                if (x == x->parent->left) {
                    x = x->parent;
                    rb_right_rotate(tree, x);
                }
                x->parent->color = BLACK;
                x->parent->parent->color = RED;
                rb_left_rotate(tree, x->parent->parent);
            }
        }
    }

    tree->root->color = BLACK;
    return NULL;
}

/* Insert a node into the tree.  If an existing node
 * returns that node; else, adds a new node and returns NULL.
 */
rb_node_t *
rb_insert(rb_tree_t *tree, RB_KEY_T key, void *client)
{
    rb_node_t *node = rb_new_node(tree, key, client);
    rb_node_t *existing = rb_insert_helper(tree, node);
    if (existing != NULL)
        rb_free_node(tree, node, FALSE/*do not free payload*/);
    return existing;
}

/* Returns node with highest key */
rb_node_t *
rb_max_node(rb_tree_t *tree)
{
    rb_node_t *iter = tree->root;
    if (iter != NIL(tree)) {
        while (iter->right != NIL(tree))
            iter = iter->right;
    }
    return iter;
}

/* Returns node with lowest key */
rb_node_t *
rb_min_node(rb_tree_t *tree)
{
    rb_node_t *iter = tree->root;
    if (iter != NIL(tree)) {
        while (iter->left != NIL(tree))
            iter = iter->left;
    }
    return iter;
}

void
rb_tree_create(rb_tree_t *tree, void (*free_payload_func)(void*))
{
    tree->NIL_node.parent = NIL(tree);
    tree->NIL_node.right = NIL(tree);
    tree->NIL_node.left = NIL(tree);
    tree->NIL_node.color = BLACK;
    tree->NIL_node.key = 0;
    tree->NIL_node.client = NULL;

    tree->root = NIL(tree);
    tree->free_payload_func = free_payload_func;
}

void
rb_tree_destroy(rb_tree_t *tree)
{
    ASSERT(tree != NULL);
    rb_clear(tree);
}
