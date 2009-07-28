#ifndef __YARNS_RBTREE__
#define __YARNS_RBTREE__

#include <stdbool.h>

typedef struct _rbtree rbtree;

rbtree* rbtree_new ();
void rbtree_free ( rbtree* tree );

void rbtree_insert ( rbtree* tree, unsigned long key, void* value );
void rbtree_remove ( rbtree* tree, unsigned long key );
bool rbtree_search ( rbtree* tree, unsigned long key, void** value );
unsigned long rbtree_size ( rbtree* tree );
inline static bool rbtree_empty ( rbtree* tree )
	{ return rbtree_size(tree) == 0; }
unsigned long rbtree_min ( rbtree* tree );
unsigned long rbtree_max ( rbtree* tree );

#endif
