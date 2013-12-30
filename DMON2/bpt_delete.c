#include "bpt.h"

static
node * delete_entry( node * root, node * n, KEY_T key, void * pointer );

/* Retrieves the index of a node's nearest neighbor
 * (sibling) to the left if one exists. If not (the
 * node is the leftmost child), returns -1 to signify
 * this special case.
 */
static
KEY_T get_neighbor_index( node * n )
{
	KEY_T i;

	/* Return the index of the key to the left
	 * of the pointer in the parent pointing
	 * to n.  
	 * If n is the leftmost child, this means
	 * return -1.
	 */
	node *parent = n->parent;
	for (i = 0; i <= parent->num_keys; i++)
		if (parent->pointers[i] == n)
			return i - 1;

#ifdef USER_APP
	printf("Search for nonexistent pointer to node in parent.\n");
	printf("Node:  %#lx\n", (unsigned long)n);
	exit(EXIT_FAILURE);
#endif
	return (KEY_T)-1;
}

/**
 * Delete one entry from node with [key,pointer]
 */
static
node * remove_entry_from_node(node * n, KEY_T key, node * pointer)
{
	KEY_T i, num_pointers;

	// Remove the key and shift other keys accordingly.
	i = 0;
	while (n->keys[i] != key)
		i++;
	for (++i; i < n->num_keys; i++)
		n->keys[i - 1] = n->keys[i];

	// Remove the pointer and shift other pointers accordingly.
	// First determine number of pointers.
	num_pointers = (n->is_leaf==TRUE) ? n->num_keys : n->num_keys + 1;
	i = 0;
	while (n->pointers[i] != pointer)
		i++;
	for (++i; i < num_pointers; i++)
		n->pointers[i - 1] = n->pointers[i];

	// One key fewer.
	n->num_keys--;

	// Set the other pointers to NULL for tidiness.
	// A leaf uses the last pointer to point to the next leaf.
	if (n->is_leaf == TRUE)
		for (i = n->num_keys; i < order - 1; i++)
			n->pointers[i] = NULL;
	else
		for (i = n->num_keys + 1; i < order; i++)
			n->pointers[i] = NULL;

	return n;
}

static
node * adjust_root(node * root)
{
	node * new_root;

	/* Case: nonempty root.
	 * Key and pointer have already been deleted,
	 * so nothing to be done.
	 */
	if (root->num_keys > 0)
		return root;

	/* Case: empty root. 
	 */

	// If it has a child, promote 
	// the first (only) child
	// as the new root.

	if (root->is_leaf == FALSE)
	{
		new_root = root->pointers[0];
		new_root->parent = NULL;
	}

	// If it is a leaf (has no children),
	// then the whole tree is empty.

	else
		new_root = NULL;

	Free_Node(root);

	return new_root;
}

/* Coalesces(combine) a node that has become
 * too small after deletion with a neighboring
 * node that can accept the additional entries
 * without exceeding the maximum.
 */
static
node * coalesce_nodes(node * root, node * n, node * neighbor, KEY_T neighbor_index, KEY_T k_prime)
{
	KEY_T i, j, neighbor_insertion_index, n_start, n_end, new_k_prime;
	node * tmp;
	BOOL split;

	/* Swap neighbor with node if node is on the
	 * extreme left and neighbor is to its right.
	 */
	if (neighbor_index == (KEY_T)-1)
	{
		tmp = n;
		n = neighbor;
		neighbor = tmp;
	}

	/* Starting point in the neighbor for copying
	 * keys and pointers from n.
	 * Recall that n and neighbor have swapped places
	 * in the special case of n being a leftmost child.
	 */
	neighbor_insertion_index = neighbor->num_keys;

	/*
	 * Nonleaf nodes may sometimes need to remain split,
	 * if the insertion of k_prime would cause the resulting
	 * single coalesced node to exceed the limit order - 1.
	 * The variable split is always false for leaf nodes
	 * and only sometimes set to true for nonleaf nodes.
	 */
	split = FALSE;

	/* Case:  nonleaf node.
	 * Append k_prime and the following pointer.
	 * If there is room in the neighbor, append
	 * all pointers and keys from the neighbor.
	 * Otherwise, append only cut(order) - 2 keys and
	 * cut(order) - 1 pointers.
	 */
	if (n->is_leaf == FALSE)
	{
		/* Append k_prime.
		 */
		neighbor->keys[neighbor_insertion_index] = k_prime;
		neighbor->num_keys++;

		/* Case (default):  there is room for all of n's keys and pointers
		 * in the neighbor after appending k_prime.
		 */
		n_end = n->num_keys;

		/* Case (special): k cannot fit with all the other keys and pointers
		 * into one coalesced node.
		 */
		n_start = 0; // Only used in this special case.
		if (n->num_keys + neighbor->num_keys >= order) {
			split = TRUE;
			n_end = CUT(order) - 2;
		}

		for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
			n->num_keys--;
			n_start++;
		}

		/* The number of pointers is always
		 * one more than the number of keys.
		 */
		neighbor->pointers[i] = n->pointers[j];

		/* If the nodes are still split, remove the first key from
		 * n.
		 */
		if (split == TRUE) {
			new_k_prime = n->keys[n_start];
			for (i = 0, j = n_start + 1; i < n->num_keys; i++, j++) {
				n->keys[i] = n->keys[j];
				n->pointers[i] = n->pointers[j];
			}
			n->pointers[i] = n->pointers[j];
			n->num_keys--;
		}

		/* All children must now point up to the same parent.
		 */

		for (i = 0; i < neighbor->num_keys + 1; i++) {
			tmp = (node *)neighbor->pointers[i];
			tmp->parent = neighbor;
		}
	}

	/* In a leaf, append the keys and pointers of
	 * n to the neighbor.
	 * Set the neighbor's last pointer to point to
	 * what had been n's right neighbor.
	 */

	else {
		for (i = neighbor_insertion_index, j = 0; j < n->num_keys; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
		}
		neighbor->pointers[order - 1] = n->pointers[order - 1];
	}

	if (split == FALSE) {
		root = delete_entry(root, n->parent, k_prime, n);
		Free_Node(n);
	}
	else
		for (i = 0; i < n->parent->num_keys; i++)
			if (n->parent->pointers[i + 1] == n) {
				n->parent->keys[i] = new_k_prime;
				break;
			}

	return root;
}

/* Redistributes entries between two nodes when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small node's entries without exceeding the
 * maximum
 */
static
node * redistribute_nodes(node * root, node * n, node * neighbor,
						KEY_T neighbor_index, KEY_T k_prime_index, KEY_T k_prime)
{
	KEY_T i;
	node * tmp;

	/* Case: n has a neighbor to the left. 
	 * Pull the neighbor's last key-pointer pair over
	 * from the neighbor's right end to n's left end.
	 */

	if (neighbor_index != (KEY_T)-1) {
		if (n->is_leaf == FALSE)
			n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
		for (i = n->num_keys; i > 0; i--) {
			n->keys[i] = n->keys[i - 1];
			n->pointers[i] = n->pointers[i - 1];
		}
		if (n->is_leaf == FALSE) {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys];
			tmp = (node *)n->pointers[0];
			tmp->parent = n;
			neighbor->pointers[neighbor->num_keys] = NULL;
			n->keys[0] = k_prime;
			n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
		}
		else {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
			neighbor->pointers[neighbor->num_keys - 1] = NULL;
			n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
			n->parent->keys[k_prime_index] = n->keys[0];
		}
	}

	/* Case: n is the leftmost child.
	 * Take a key-pointer pair from the neighbor to the right.
	 * Move the neighbor's leftmost key-pointer pair
	 * to n's rightmost position.
	 */

	else {  
		if (n->is_leaf == TRUE) {
			n->keys[n->num_keys] = neighbor->keys[0];
			n->pointers[n->num_keys] = neighbor->pointers[0];
			n->parent->keys[k_prime_index] = neighbor->keys[1];
		}
		else {
			n->keys[n->num_keys] = k_prime;
			n->pointers[n->num_keys + 1] = neighbor->pointers[0];
			tmp = (node *)n->pointers[n->num_keys + 1];
			tmp->parent = n;
			n->parent->keys[k_prime_index] = neighbor->keys[0];
		}
		for (i = 0; i < neighbor->num_keys; i++) {
			neighbor->keys[i] = neighbor->keys[i + 1];
			neighbor->pointers[i] = neighbor->pointers[i + 1];
		}
		if (n->is_leaf == FALSE)
			neighbor->pointers[i] = neighbor->pointers[i + 1];
	}

	/* n now has one more key and one more pointer;
	 * the neighbor has one fewer of each.
	 */

	n->num_keys++;
	neighbor->num_keys--;

	return root;
}


/* Deletes an entry from the B+ tree.
 * Removes the record and its key and pointer from the leaf,
 * and then makes all appropriate changes to preserve the B+ tree properties.
 */
static
node * delete_entry( node * root, node * n, KEY_T key, void * pointer )
{
	KEY_T min_keys;
	node * neighbor;
	KEY_T neighbor_index;
	KEY_T k_prime_index, k_prime;
	KEY_T capacity;

	// Remove key and pointer from node.

	n = remove_entry_from_node(n, key, pointer);

	/* Case:  deletion from the root. 
	 */

	if (n == root) 
		return adjust_root(root);


	/* Case:  deletion from a node below the root.
	 * (Rest of function body.)
	 */

	/* Determine minimum allowable size of node,
	 * to be preserved after deletion.
	 */

	min_keys = (n->is_leaf==TRUE) ? CUT(order - 1) : CUT(order) - 1;

	/* Case:  node stays at or above minimum.
	 * (The simple case.)
	 */

	if (n->num_keys >= min_keys)
		return root;

	/* Case:  node falls below minimum.
	 * Either coalescence or redistribution
	 * is needed.
	 */

	/* Find the appropriate neighbor node with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between the pointer to node n and the pointer
	 * to the neighbor.
	 */

	neighbor_index = get_neighbor_index( n );
	k_prime_index = (neighbor_index == (KEY_T)-1) ? 0 : neighbor_index;
	k_prime = n->parent->keys[k_prime_index];
	neighbor = (neighbor_index == (KEY_T)-1) ? n->parent->pointers[1] :
									n->parent->pointers[neighbor_index];

	capacity = (n->is_leaf == TRUE) ? order : order - 1;

	/* Coalescence(combine). */

	if (neighbor->num_keys + n->num_keys < capacity)
		return coalesce_nodes(root, n, neighbor, neighbor_index, k_prime);

	/* Redistribution. */

	else
		return redistribute_nodes(root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}

/* Master deletion function. */
node * Delete(node * root, KEY_T key)
{
	node * key_leaf;
	record * key_record;

	key_record = Find_Record(root, key);
	key_leaf = Find_Leaf(root, key);
	if (key_record != NULL && key_leaf != NULL)
	{
		root = delete_entry(root, key_leaf, key, key_record);
		Free_Record(key_record);
	}
	return root;
}

static
void destroy_tree_nodes(node * root)
{
	KEY_T i;
	if (root->is_leaf == TRUE)
		for (i = 0; i < root->num_keys; i++)
			Free_Record((record*)root->pointers[i]);
	else
		for (i = 0; i < root->num_keys + 1; i++)
			destroy_tree_nodes(root->pointers[i]);

	Free_Node(root);
}

node * Destroy_Tree(node * root)
{
	destroy_tree_nodes(root);
	return NULL;
}
