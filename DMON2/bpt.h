#ifndef __BPT_H__
#define __BPT_H__

#ifndef WINVER
#define USER_APP
#endif

#ifdef USER_APP
  #include <stdio.h>
  #include <stdlib.h>
  #include <assert.h>
  #define MALLOC(n)		malloc(n)
  #define FREE(p)		free(p)
#else
  #include <Ntddk.h>
  #define BPT_POOL_TAG	'bptD'
  #define MALLOC(n)		ExAllocatePoolWithTag (	\
							NonPagedPool,		\
							(SIZE_T)(n),		\
							BPT_POOL_TAG		\
						)
  #define FREE(p)		ExFreePoolWithTag(p,BPT_POOL_TAG)
  #define assert(expr)	ASSERT(expr)
#endif

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef BOOL
#define BOOL  int
#endif

#ifndef ULONG
typedef unsigned long ULONG;
#endif
#define KEY_T      ULONG
#define VAL_T      ULONG
#define order     (KEY_T)(8)

typedef struct _node
{
	// node: array of nodes corresponding to keys
	// leaf: with a maximum of order-1 key-pointer
	//       The last pointer points to the right leaf
	void ** pointers;
	KEY_T * keys;
	KEY_T num_keys;
	BOOL is_leaf;
	struct _node * parent;
	struct _node * next;
} node;

typedef struct _record
{
	VAL_T value;
} record;

static KEY_T CUT( KEY_T length )
{
	if (length % 2 == 0)
		return length/2;
	else
		return length/2 + 1;
}

node * Make_Node ( void );
void Free_Node ( node * n );
record * Make_Record ( VAL_T value );
void Free_Record ( record * n );

node * Insert ( node * root, KEY_T key, VAL_T value );
node * Delete ( node * root, KEY_T key );

node * Find_Leaf ( node * root, KEY_T key );
record * Find_Record ( node * root, KEY_T key );
node* Get_Leftmost_Leaf(node *root);
node * Destroy_Tree ( node * root );

#ifdef USER_APP
void Print_Tree_File( node * root );
#endif

#endif
