extern "C"
{
#include "alloc.h"
#include "rbtree.h"
}

#include <map>
#include <algorithm>

using namespace std;

template <class T> class yallocator;

template <> class yallocator<void>
{
public:
	typedef void* pointer;
	typedef const void* const_pointer;
	typedef void value_type;
	template <class U> struct rebind { typedef yallocator<U> other; };
};

template <class T> class yallocator
{
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;
	template <class U> struct rebind { typedef yallocator<U> other; };
	
	yallocator()
	{
	}
	
	yallocator(const yallocator& o)
	{
	}
	
	template <class U> yallocator(const yallocator<U>&)
	{
	}
	
	~yallocator ()
	{
	}
	
	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }
	
	pointer allocate(size_type len, yallocator<void>::const_pointer hint = 0)
	{
		return (pointer)yalloc(len*sizeof(T));
	}
	
	void deallocate(pointer p, size_type n)
	{
		yfree(p);
	}
	
	size_type max_size () const
	{
		return ~(size_type)0;
	}
	
	void construct ( pointer p, const T& val ) {}
	void destroy ( pointer p ) {}
};

typedef map<unsigned long, void*, less<unsigned long>, yallocator<std::pair<const unsigned long, void*> > > rtree;

struct _rbtree
{
	rtree tree;
	void* operator new ( size_t len )
	{
		return yalloc(len);
	}
	
	void operator delete ( void* ptr )
	{
		yfree(ptr);
	}
};

rbtree* rbtree_new ()
{
	rbtree* rt = new rbtree;
	return rt;
}

void rbtree_free ( rbtree* tree )
{
	delete tree;
}

void rbtree_insert ( rbtree* tree, unsigned long key, void* value )
{
	tree->tree[key] = value;
}

void rbtree_remove ( rbtree* tree, unsigned long key )
{
	rtree::iterator iter = tree->tree.find(key);
	if (iter != tree->tree.end())
		tree->tree.erase(iter);
}

bool rbtree_search ( rbtree* tree, unsigned long key, void** value )
{
	rtree::iterator iter = tree->tree.find(key);
	if (iter != tree->tree.end())
	{
		*value = iter->second;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned long rbtree_size ( rbtree* tree )
{
	return tree->tree.size();
}

unsigned long rbtree_min ( rbtree* tree )
{
	return tree->tree.begin()->first;
}

unsigned long rbtree_max ( rbtree* tree )
{
	return tree->tree.rbegin()->first;
}

