#include "rbtree.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct _node node;

struct _rbtree
{
	unsigned long count;
	node* root;
};

#define COLOUR_RED 0
#define COLOUR_BLACK 1

#define UNIMPLEMENTED fprintf(stderr, "UNIMPLEMENTED @ %s line %d\n", __FILE__, __LINE__); exit(1)

struct _node
{
	unsigned long key;
	void* value;
	node* parent;
	node* left;
	node* right;
	bool colour;
};

rbtree* rbtree_new ()
{
	rbtree* tree = (rbtree*)yalloc(sizeof(rbtree));
	tree->count = 0;
	tree->root = 0;
	return tree;
}

static void node_nuke ( node* node )
{
	if (node->left)  node_nuke(node->left);
	if (node->right) node_nuke(node->right);
	yfree(node);
}

void rbtree_free ( rbtree* tree )
{
	if (tree->root) node_nuke(tree->root);
	yfree(tree);
}

static node* grandparent ( node* n )
{
	return (n && n->parent) ? n->parent->parent : 0;
}

static node* uncle ( node* n )
{
	node* gp = grandparent(n);
	if (!gp) return 0;
	return gp->left == n->parent ? gp->right : gp->left;
}

static node* sibling ( node* n )
{
	if (!(n->parent)) return 0;
	return n == n->parent->left ? n->parent->right : n->parent->left;
}

static void rotate_left ( rbtree* tree, node* n )
{
	node* p = n;
	node* q = p->right;
	UNIMPLEMENTED;
}

static void rotate_right ( rbtree* tree, node* n )
{
	UNIMPLEMENTED;
}

static void rb_insert ( rbtree* tree, node* n )
{
	if (n->parent)
	{
		n->colour = COLOUR_RED;
		if (n->parent->colour == COLOUR_RED)
		{
			node* g = grandparent(n);
			node* u = uncle(n);
			if (u && u->colour == COLOUR_RED)
			{
				n->parent->colour = COLOUR_BLACK;
				u->colour = COLOUR_BLACK;
				g->colour = COLOUR_RED;
				rb_insert(tree, n);
			}
			else
			{
				if (n == n->parent->right && n->parent == g->left)
				{
					rotate_left(tree, n->parent);
					n = n->left;
				}
				else if (n == n->parent->left && n->parent == g->right)
				{
					rotate_right(tree, n->parent);
					n = n->right;
				}
				n->parent->colour = COLOUR_BLACK;
				g->colour = COLOUR_RED;
				if (n == n->parent->left && n->parent == g->left)
					rotate_right(tree, g);
				else
					rotate_left(tree, g);
			}
		}
	}
	else
	{
		n->colour = COLOUR_BLACK;
	}
}

static node* bt_insert ( node** np, node* parent, unsigned long key, bool* addition )
{
	node* n = *np;
	if (n)
	{
		if (n->key == key)
		{
			*addition = 0;
			return n;
		}
		else if (n->key > key)
		{
			return bt_insert(&n->left, n, key, addition);
		}
		else
		{
			return bt_insert(&n->right, n, key, addition);
		}
	}
	else
	{
		*addition = 1;
		n = yalloc(sizeof(node));
		*np = n;
		n->parent = parent;
		n->left = 0;
		n->right = 0;
		n->key = key;
		return n;
	}
}

void rbtree_insert ( rbtree* tree, unsigned long key, void* value )
{
	bool addition;
	node* n = bt_insert(&(tree->root), 0, key, &addition);
	if (addition)
	{
		tree->count++;
	}
	n->value = value;
	rb_insert(tree, n);
}

static bool is_leaf ( node* n )
{
	return n->left == 0 && n->right == 0;
}

static void replace_node ( rbtree* tree, node* a, node* b )
{
	UNIMPLEMENTED;
}

static void delete_case6 ( rbtree* tree, node* n )
{
	node* s = sibling(n);
	
	s->colour = n->parent->colour;
	n->parent->colour = COLOUR_BLACK;
	
	if (n == n->parent->left)
	{
		s->right->colour = COLOUR_BLACK;
		rotate_left(tree, n->parent);
	}
	else
	{
		s->left->colour = COLOUR_BLACK;
		rotate_right(tree, n->parent);
	}
}

static void delete_case5 ( rbtree* tree, node* n )
{
	node* s = sibling(n);
	
	if (s->colour == COLOUR_BLACK)
	{
		if ((n == n->parent->left) &&
		    (s->right->colour == COLOUR_BLACK) &&
		    (s->left->colour == COLOUR_RED))
		{
			s->colour = COLOUR_RED;
			s->left->colour = COLOUR_BLACK;
			rotate_right(tree, s);
		}
		else if ((n == n->parent->right) &&
		         (s->left->colour == COLOUR_BLACK) &&
		         (s->right->colour == COLOUR_RED))
		{
			s->colour = COLOUR_RED;
			s->right->colour = COLOUR_BLACK;
			rotate_left(tree, s);
		}
	}
	delete_case6(tree, n);
}

static void delete_case4 ( rbtree* tree, node* n )
{
	node* s = sibling(n);
	if ((n->parent->colour == COLOUR_RED) &&
	    (s->colour == COLOUR_BLACK) &&
	    (s->left->colour == COLOUR_BLACK) &&
	    (s->right->colour == COLOUR_BLACK))
	{
		s->colour = COLOUR_RED;
		n->parent->colour = COLOUR_BLACK;
	}
	else
	{
		delete_case5(tree, n);
	}
}

static void delete_case1 ( rbtree* tree, node* n );

static void delete_case3 ( rbtree* tree, node* n )
{
	node* s = sibling(n);
	
	if ((n->parent->colour == COLOUR_BLACK) &&
	    (s->colour == COLOUR_BLACK) &&
	    (s->left->colour == COLOUR_BLACK) &&
	    (s->right->colour == COLOUR_BLACK))
	{
		s->colour = COLOUR_RED;
		delete_case1(tree, n);
	}
	else
	{
		delete_case4(tree, n);
	}
}

static void delete_case2 ( rbtree* tree, node* n )
{
	node* s = sibling(n);
	
	if (s->colour == COLOUR_RED)
	{
		n->parent->colour = COLOUR_RED;
		s->colour = COLOUR_BLACK;
		if (n == n->parent->left)
			rotate_left(tree, n->parent);
		else
			rotate_right(tree, n->parent);
	}
	delete_case3(tree, n);
}

static void delete_case1 ( rbtree* tree, node* n )
{
	if (n->parent)
		delete_case2(tree, n);
}

static void delete_one_child ( rbtree* tree, node* n )
{
	node* child = is_leaf(n->right) ? n->left : n->right;
	
	replace_node(tree, n, child);
	if (n->colour == COLOUR_BLACK)
	{
		if (child->colour == COLOUR_RED)
			child->colour = COLOUR_BLACK;
		else
			delete_case1(tree, child);
	}
	yfree(n);
}

void rbtree_remove ( rbtree* tree, unsigned long key )
{
	UNIMPLEMENTED;
}

static node* bt_search ( node* base, unsigned long key )
{
	if (base == 0) return 0;
	if (base->key == key) return base;
	else if (base->key > key) return bt_search(base->left, key);
	else return bt_search(base->right, key);
}

bool rbtree_search ( rbtree* tree, unsigned long key, void** value )
{
	node* n = bt_search(tree->root, key);
	if (n)
	{
		*value = n->value;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned long rbtree_size ( rbtree* tree )
{
	return tree->count;
}

unsigned long rbtree_min ( rbtree* tree )
{
	node* n;
	if (!tree->root)
		return ~0UL;
	n = tree->root;
	while (n->left) n = n->left;
	return n->key;
}

unsigned long rbtree_max ( rbtree* tree )
{
	node* n;
	if (!tree->root)
		return ~0UL;
	n = tree->root;
	while (n->right) n = n->right;
	return n->key;
}
