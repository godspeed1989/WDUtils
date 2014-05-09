#ifndef __BPT_H__
#define __BPT_H__

#include "common.h"

#define KEY_T      LONGLONG
#define VAL_T      void*
#define order     (KEY_T)(128)

typedef struct _node
{
	// node: array of nodes corresponding to keys
	// leaf: with a maximum of order-1 key-pointer
	//       The last pointer points to the right leaf
	void ** pointers;
	KEY_T * keys;
	KEY_T num_keys;
	BOOLEAN is_leaf;
	struct _node * parent;
#ifdef PRINT_BPT
	struct _node * next;
#endif
} node;

struct _CACHE_BLOCK;
typedef struct _CACHE_BLOCK record;

KEY_T CUT( KEY_T length );

node * Make_Node ( void );
void Free_Node ( node * n );

extern void Free_Record ( record * n );

node * Insert ( node * root, KEY_T key, record * r );
node * Delete ( node * root, KEY_T key, BOOLEAN free );

node * Find_Leaf ( node * root, KEY_T key );
record * Find_Record ( node * root, KEY_T key );
node* Get_Leftmost_Leaf(node *root);
node * Destroy_Tree ( node * root );

#ifdef USER_APP
void Print_Tree_File( node * root );
#endif

#endif
