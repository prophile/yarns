#include "wait_graph.h"
#include "yarn-internal.h"
#include "rbtree.h"
#include "pool.h"
#include "lock.h"
#include "alloc.h"

#define DEPENDENCY_TYPE_TIME 0
#define DEPENDENCY_TYPE_TOKEN 1

#ifndef MAX
#define MAX(a,b) ((a)<(b)?(a):(b))
#endif

typedef struct _task_entry task_entry;
typedef struct _dependency dependency;

struct _task_entry
{
	yarn_t target;
	unsigned long ndeps;
	task_entry* prev;
	task_entry* next;
};

struct _dependency
{
	unsigned long type;
	task_entry* te;
	union
	{
		yarns_time_t targetTime;
		unsigned long token;
	} data;
	dependency* next;
};

struct _wait_graph
{
	unsigned long tokenIndex;
	pool* graphPool;
	lock_t lock;
	task_entry* firstTask;
	dependency* firstDependency;
};

wait_graph* wait_graph_new ()
{
	wait_graph* wg = yalloc(sizeof(wait_graph));
	wg->tokenIndex = 1;
	wg->graphPool = pool_create(MAX(sizeof(dependency), sizeof(task_entry)), 1024);
	lock_init(&(wg->lock));
	wg->firstTask = 0;
	wg->firstDependency = 0;
	return wg;
}

void wait_graph_delete ( wait_graph* graph )
{
	pool_destroy(graph->graphPool);
	yfree(graph);
}

static dependency* dependency_eliminate ( wait_graph* graph, dependency* dep, dependency* prev )
{
	task_entry* entry;
	dependency* rv = dep->next;
	if (prev)
		prev->next = rv;
	else
		graph->firstDependency = rv;
	entry = rv->te;
	if (--entry->ndeps == 0)
	{
		yarn_resume(entry->target);
		if (entry->next)
			entry->next->prev = entry->prev;
		if (entry->prev)
			entry->prev->next = entry->next;
		else
			graph->firstTask = entry;
		pool_free(graph->graphPool, entry);
	}
	pool_free(graph->graphPool, dep);
	return rv;
}

void wait_graph_time_process ( wait_graph* graph )
{
	yarns_time_t time = yarns_time();
	dependency* prev = 0;
	dependency* dep = graph->firstDependency;
	while (dep)
	{
		if (dep->type == DEPENDENCY_TYPE_TIME)
		{
			if (dep->data.targetTime < time)
			{
				dep = dependency_eliminate(graph, dep, prev);
			}
		}
		if (!dep)
			return;
		prev = dep;
		dep = dep->next;
	}
}

void wait_graph_lock ( wait_graph* graph )
{
	lock_lock(&(graph->lock));
}

void wait_graph_unlock ( wait_graph* graph )
{
	lock_unlock(&(graph->lock));
}

unsigned long wait_graph_next_token ( wait_graph* graph )
{
	return graph->tokenIndex++;
}

void wait_graph_signal ( wait_graph* graph, unsigned long token )
{
	dependency* prev = 0;
	dependency* dep = graph->firstDependency;
	while (dep)
	{
		if (dep->type == DEPENDENCY_TYPE_TOKEN)
		{
			if (dep->data.token == token)
			{
				dep = dependency_eliminate(graph, dep, prev);
			}
		}
		if (!dep)
			return;
		prev = dep;
		dep = dep->next;
	}
}

void wait_graph_insert ( wait_graph* graph, yarn_t yarn )
{
	task_entry* entry = pool_allocate(graph->graphPool);
	entry->target = yarn;
	entry->ndeps = 0;
	entry->next = graph->firstTask;
	graph->firstTask->prev = entry;
	graph->firstTask = entry;
	entry->prev = 0;
}

void wait_graph_insert_token ( wait_graph* graph, yarn_t yarn, unsigned long token )
{
	task_entry* entry = graph->firstTask;
	dependency* dep;
	while (entry)
	{
		if (entry->target == yarn)
			continue;
		entry = entry->next;
	}
	if (!entry) return;
	dep = pool_allocate(graph->graphPool);
	dep->type = DEPENDENCY_TYPE_TOKEN;
	dep->te = entry;
	dep->data.token = token;
	dep->next = graph->firstDependency;
	entry->ndeps++;
	graph->firstDependency = dep;
}

void wait_graph_insert_time ( wait_graph* graph, yarn_t yarn, yarns_time_t time )
{
	task_entry* entry = graph->firstTask;
	dependency* dep;
	while (entry)
	{
		if (entry->target == yarn)
			continue;
		entry = entry->next;
	}
	if (!entry) return;
	dep = pool_allocate(graph->graphPool);
	dep->type = DEPENDENCY_TYPE_TIME;
	dep->te = entry;
	dep->data.targetTime = time;
	dep->next = graph->firstDependency;
	entry->ndeps++;
	graph->firstDependency = dep;
}
