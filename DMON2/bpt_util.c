#include "bpt.h"

/* give the height of the tree, which length in number of edges */
KEY_T height( node * root )
{
	KEY_T height = 0;
	node * c = root;
	while (c->is_leaf == FALSE)
	{
		c = (node *)c->pointers[0];
		height++;
	}
	return height;
}

node * Make_Node( void )
{
	node * new_node;

	new_node = (node*)MALLOC(sizeof(node));
	assert(new_node);
	// keys: order - 1
	new_node->keys = (KEY_T*)MALLOC( (order - 1) * sizeof(KEY_T) );
	assert(new_node->keys);
	// pointers: order
	new_node->pointers = (void*)MALLOC( order * sizeof(void *) );
	assert(new_node->pointers);

	new_node->is_leaf = FALSE;
	new_node->num_keys = 0;
	new_node->parent = NULL;
	new_node->next = NULL;
	return new_node;
}

void Free_Node( node * n )
{
	if(n)
	{
		FREE(n->pointers);
		FREE(n->keys);
		FREE(n);
	}
}

record * Make_Record( VAL_T value )
{
	record * new_record = (record *)MALLOC(sizeof(record));
	assert(new_record);
	new_record->value = value;
	return new_record;
}

void Free_Record( record * r )
{
	if(r)
	{
		FREE(r);
	}
}

#ifdef USER_APP
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static node * queue = NULL;

static
void enqueue( node * new_node )
{
	node * c;
	if (queue == NULL) {
		queue = new_node;
		queue->next = NULL;
	}
	else {
		c = queue;
		while(c->next != NULL) {
			c = c->next;
		}
		c->next = new_node;
		new_node->next = NULL;
	}
}

static
node * dequeue( void )
{
	node * n = queue;
	queue = queue->next;
	n->next = NULL;
	return n;
}

void Print_Tree_File( node * root )
{
	FILE *fp;
	time_t tim;
	struct tm *at;
	char filename[64];

	node * n = NULL;
	KEY_T i = 0;

	time(&tim);
	at = localtime(&tim);
	strftime(filename, 63, "%Y%m%d%H%M%S.dot", at);
	fp = fopen(filename, "w");
	if( fp == NULL ) {
		printf("open %s error\n", filename);
		return;
	}
	fprintf(fp, "digraph {\n");
	fprintf(fp, "graph[ordering=\"out\"];\n");
	fprintf(fp, "node[fontcolor=\"#990000\",shape=plaintext];\n");
	fprintf(fp, "edge[arrowsize=0.6,fontsize=6];\n");
	if( root == NULL ) {
		fprintf(fp, "null[shape=box]\n");
		return;
	}
	queue = NULL;
	enqueue(root);
	while ( queue != NULL ) {
		n = dequeue();
		fprintf(fp, "n%p[label=\"", n);
		for (i = 0; i < n->num_keys; i++) {
			fprintf(fp, " %lu ", n->keys[i]);
		}
		fprintf(fp, "\",shape=box];\n");
		if (n->is_leaf == FALSE) {
			for (i = 0; i <= n->num_keys; i++) {
				fprintf(fp, " n%p -> n%p;\n", n, n->pointers[i]);
				enqueue(n->pointers[i]);
			}
		}
		else {
			if(n->pointers[order - 1])
				fprintf(fp, " n%p -> n%p[constraint=FALSE];\n", n, n->pointers[order - 1]);
		}
	}
	fprintf(fp, "}\n");
	fclose(fp);
}
#endif
