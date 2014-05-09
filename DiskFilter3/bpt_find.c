#include "bpt.h"

/* Traces the path from the root to a leaf, searching by key.
 * Returns the leaf containing the given key.
 */
node * Find_Leaf( node * root, KEY_T key )
{
	KEY_T i = 0;
	node * c = root;
	if (c == NULL)
		return c;
	while (c->is_leaf == FALSE)
	{
		i = 0;
		while (i < c->num_keys)
		{
			if (key < c->keys[i])
				break;
			i++;
		}
		c = (node *)c->pointers[i];
	}
	return c;
}

/* Finds and returns the record to which a key refers. */
record * Find_Record( node * root, KEY_T key )
{
	KEY_T i = 0;
	node * c = Find_Leaf( root, key );
	if (c == NULL)
		return NULL;
	for (i = 0; i < c->num_keys; i++)
	{
		if (c->keys[i] == key)
		{
			return (record *)c->pointers[i];
		}
	}
	return NULL;
}

node* Get_Leftmost_Leaf(node *root)
{
	while (root != NULL && root->is_leaf == FALSE)
	{
		root = root->pointers[0];
	}
	return root;
}
