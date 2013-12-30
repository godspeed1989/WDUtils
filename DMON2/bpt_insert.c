#include "bpt.h"

static
node * insert_into_parent(node * root, node * left, KEY_T key, node * right);

static
node * make_leaf( void )
{
	node * leaf = Make_Node();
	leaf->is_leaf = TRUE;
	return leaf;
}

/* Inserts a new pointer to a record and its corresponding key into a leaf.
 * Returns the altered leaf.
 */
static
node * insert_into_leaf( node * leaf, KEY_T key, record * pointer )
{
	KEY_T i, insertion_point;

	insertion_point = 0;
	while (insertion_point < leaf->num_keys)
	{
		if(leaf->keys[insertion_point] >= key)
			break;
		insertion_point++;
	}

	for (i = leaf->num_keys; i > insertion_point; i--)
	{
		leaf->keys[i] = leaf->keys[i - 1];
		leaf->pointers[i] = leaf->pointers[i - 1];
	}
	leaf->keys[insertion_point] = key;
	leaf->pointers[insertion_point] = pointer;
	leaf->num_keys++;
	return leaf;
}

/* Inserts a new key and pointer to a new record into a leaf.
 * so as to exceed the tree's order, causing the leaf to be split
 * in half.
 */
static
node * insert_into_leaf_after_splitting(node * root, node * leaf, KEY_T key, record * pointer)
{
	node * new_leaf;
	KEY_T * temp_keys;
	void ** temp_pointers;

	KEY_T insertion_index, split, new_key, i, j;

	temp_keys = (KEY_T*)MALLOC( order * sizeof(KEY_T) );
	assert(temp_keys);
	temp_pointers = (void**)MALLOC( order * sizeof(void *) );
	assert(temp_pointers);

	// locate insert index
	insertion_index = 0;
	while (insertion_index < order - 1 && leaf->keys[insertion_index] < key)
		insertion_index++;

	for (i = 0, j = 0; i < leaf->num_keys; i++, j++)
	{
		if (j == insertion_index) j++; // leave for insertion
		temp_keys[j] = leaf->keys[i];
		temp_pointers[j] = leaf->pointers[i];
	}
	temp_keys[insertion_index] = key;
	temp_pointers[insertion_index] = pointer;

	split = CUT(order - 1);
	// old leaf
	leaf->num_keys = split;
	for (i = 0; i < split; i++)
	{
		leaf->pointers[i] = temp_pointers[i];
		leaf->keys[i] = temp_keys[i];
	}
	// new leaf
	new_leaf = make_leaf();
	new_leaf->num_keys = order - split;
	for (i = split, j = 0; i < order; i++, j++)
	{
		new_leaf->pointers[j] = temp_pointers[i];
		new_leaf->keys[j] = temp_keys[i];
	}
	new_leaf->parent = leaf->parent;
	// The last pointer points to the leaf to the right
	new_leaf->pointers[order - 1] = leaf->pointers[order - 1];
	leaf->pointers[order - 1] = new_leaf;

	for (i = leaf->num_keys; i < order - 1; i++)
		leaf->pointers[i] = NULL;
	for (i = new_leaf->num_keys; i < order - 1; i++)
		new_leaf->pointers[i] = NULL;

	new_key = new_leaf->keys[0];

	FREE(temp_pointers);
	FREE(temp_keys);

	return insert_into_parent(root, leaf, new_key, new_leaf);
}

/* get index of leaf
 */
static
KEY_T get_child_index(node * parent, node * child)
{
	KEY_T index = 0;
	while (index <= parent->num_keys && parent->pointers[index] != child)
		index++;
	return index;
}

/* Inserts a new key and pointer into a node 'n' into which 
 * these can fit without violating the B+ tree properties.
 */
static
node * insert_into_node(node * root, node * n, KEY_T left_index, KEY_T key, node * right)
{
	KEY_T i;
	for (i = n->num_keys; i > left_index; i--)
	{
		n->pointers[i + 1] = n->pointers[i];
		n->keys[i] = n->keys[i - 1];
	}
	n->keys[left_index] = key;
	n->pointers[left_index + 1] = right;
	n->num_keys++;
	return root;
}

/* Inserts a new key and pointer into a node old_node,
 * causing the node's size to exceed the order,
 * and causing the node to split into two.
 */
static
node * insert_into_node_after_splitting(node * root, node * old_node, KEY_T left_index,  KEY_T key, node * right)
{
	KEY_T i, j, split, k_prime;
	node * new_node;
	KEY_T * temp_keys;
	void ** temp_pointers;

	/* First create a temporary set of keys and pointers
	 * to hold everything in order, including the new key and pointer,
	 * inserted in their correct places. 
	 * Then create a new node and copy half of the keys and pointers
	 * to the old node and the other half to the new.
	 */
	temp_keys = (KEY_T*)MALLOC( order * sizeof(KEY_T) );
	assert(temp_keys);
	temp_pointers = (void**)MALLOC( (order + 1) * sizeof(void *) );
	assert(temp_pointers);

	for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++)
	{
		if (j == left_index + 1) j++;
		temp_pointers[j] = old_node->pointers[i];
	}
	temp_pointers[left_index + 1] = right;
	for (i = 0, j = 0; i < old_node->num_keys; i++, j++)
	{
		if (j == left_index) j++;
		temp_keys[j] = old_node->keys[i];
	}
	temp_keys[left_index] = key;

	/* Create the new node and copy
	 * half the keys and pointers to the
	 * old and half to the new.
	 */  
	split = CUT(order);
	// reconstruct old node
	old_node->num_keys = split - 1;
	for (i = 0; i < old_node->num_keys; i++)
	{
		old_node->pointers[i] = temp_pointers[i];
		old_node->keys[i] = temp_keys[i];
	}
	old_node->pointers[split - 1] = temp_pointers[split - 1];
	k_prime = temp_keys[split - 1];
	// construct new node
	new_node = Make_Node();
	for (++i, j = 0; i < order; i++, j++)
	{
		new_node->pointers[j] = temp_pointers[i];
		new_node->keys[j] = temp_keys[i];
		new_node->num_keys++;
	}
	new_node->pointers[j] = temp_pointers[i];
	new_node->parent = old_node->parent;
	for (i = 0; i <= new_node->num_keys; i++)
	{
		node *child = new_node->pointers[i];
		child->parent = new_node;
	}

	FREE(temp_pointers);
	FREE(temp_keys);
	/* Insert a new key into the parent of the two
	 * nodes resulting from the split, with
	 * the old node to the left and the new to the right.
	 */
	return insert_into_parent(root, old_node, k_prime, new_node);
}

/* Creates a new root for two subtrees
 * and inserts the appropriate key into
 * the new root.
 */
static
node * insert_into_new_root(node * left, KEY_T key, node * right)
{
	node * root = Make_Node();
	root->keys[0] = key;
	root->pointers[0] = left;
	root->pointers[1] = right;
	root->num_keys++;
	root->parent = NULL;
	left->parent = root;
	right->parent = root;
	return root;
}

/* Inserts a new node (leaf or internal node) into the B+ tree.
 * Returns the root of the tree after insertion.
 */
static
node * insert_into_parent(node * root, node * left, KEY_T key, node * right)
{
	KEY_T left_index;
	node * parent;

	parent = left->parent;

	/* new root. */
	if (parent == NULL)
		return insert_into_new_root(left, key, right);

	/* Find the parent's pointer to the left node. */
	left_index = get_child_index(parent, left);

	/* the new key fits into the node. */
	if (parent->num_keys < order - 1)
		return insert_into_node(root, parent, left_index, key, right);

	/* split a node in order to preserve the B+ tree properties. */
	return insert_into_node_after_splitting(root, parent, left_index, key, right);
}

/* First insertion */
static
node * start_new_tree(KEY_T key, record * pointer)
{
	node * root = make_leaf();
	root->keys[0] = key;
	root->pointers[0] = pointer;
	root->pointers[order - 1] = NULL;
	root->parent = NULL;
	root->num_keys++;
	return root;
}

/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
node * Insert( node * root, KEY_T key, VAL_T value )
{
	record * pointer;
	node * leaf;

	/* The current implementation ignores duplicates. */
	if (Find_Record(root, key) != NULL)
		return root;

	/* Create a new record for the value. */
	pointer = Make_Record(value);

	/* Case: the tree does not exist yet. */
	if (root == NULL) 
		return start_new_tree(key, pointer);

	/* Case: the tree already exists. */
	leaf = Find_Leaf(root, key);

	/* Case: leaf has room for key and pointer. */
	if (leaf->num_keys < order - 1)
	{
		leaf = insert_into_leaf(leaf, key, pointer);
		return root;
	}

	/* Case:  leaf must be split. */
	return insert_into_leaf_after_splitting(root, leaf, key, pointer);
}
