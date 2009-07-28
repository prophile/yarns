#include "scheduler.h"
#include "alloc.h"
#include "rbtree.h"
#include <assert.h>
#include "config.h"
#include "debug.h"

#if YARNS_SCHEDULER == YARNS_SCHED_FAIR

#define DEBUG_MODULE DEBUG_SCHEDULER

#define SCHED_FAIR_MIN_GRANULARITY 5

typedef struct _scheduler_task scheduler_task;

struct _scheduler_task
{
	unsigned long pid;
	unsigned long runtime;
	unsigned long timebase;
	float weight;
	float inverseWeight;
};

struct _scheduler
{
	rbtree* processTree;
	float load;
};

scheduler* scheduler_init ()
{
	scheduler* sched = yalloc(sizeof(scheduler));
	sched->processTree = rbtree_new();
	sched->load = 0.0f;
	return sched;
}

void scheduler_free ( scheduler* sched )
{
	rbtree_free(sched->processTree);
	yfree(sched);
}

static float prio_to_weight ( scheduler_priority prio )
{
	return 1.0f;
}

static void timeline_insert ( scheduler* sched, scheduler_task* task, unsigned long timebase )
{
	timebase += SCHED_FAIR_MIN_GRANULARITY;
	while (rbtree_search(sched->processTree, timebase, 0))
		timebase++;
	DEBUG("put in task %lu on timeline for vtime %lu\n", task->pid, timebase);
	rbtree_insert(sched->processTree, timebase, task);
	task->timebase = timebase;
}

void scheduler_insert ( scheduler* sched, unsigned long pid, scheduler_priority prio )
{
	scheduler_task* newTask;
	unsigned long min = rbtree_empty(sched->processTree) ? 0 : rbtree_min(sched->processTree);
	newTask = yalloc(sizeof(scheduler_task));
	newTask->pid = pid;
	newTask->runtime = 0;
	newTask->weight = prio_to_weight(prio);
	newTask->inverseWeight = 1.0f / newTask->weight;
	sched->load += newTask->weight;
	timeline_insert(sched, newTask, min);
}

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	scheduler_task* lastTask = (scheduler_task*)job->data;
	if (lastTask && job->runtime == SCHEDULER_UNSCHEDULE)
	{
		sched->load -= lastTask->weight;
		yfree(lastTask);
	}
	else if (lastTask)
	{
		assert(lastTask->runtime >= job->runtime);
		timeline_insert(sched, lastTask, lastTask->timebase + (lastTask->runtime - job->runtime) * lastTask->inverseWeight);
	}
	if (rbtree_empty(sched->processTree))
	{
		job->pid = 0;
		job->runtime = 0;
		job->data = 0;
	}
	else
	{
		unsigned long min = rbtree_min(sched->processTree);
		bool found = rbtree_search(sched->processTree, min, (void**)&lastTask);
		assert(found);
		rbtree_remove(sched->processTree, min);
		job->pid = lastTask->pid;
		job->data = (unsigned long)lastTask;
		if (rbtree_empty(sched->processTree))
		{
			job->runtime = lastTask->runtime = YARNS_TIMESLICE * 10;
		}
		else
		{
			unsigned long mmin = rbtree_min(sched->processTree);
			unsigned long rt = mmin - min;
			if (rt < SCHED_FAIR_MIN_GRANULARITY)
				rt = SCHED_FAIR_MIN_GRANULARITY;
			job->runtime = lastTask->runtime = rt;
		}
	}
}

#endif
