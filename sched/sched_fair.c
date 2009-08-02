#include "scheduler.h"
#include "alloc.h"
#include "rbtree.h"
#include <assert.h>
#include "config.h"
#include "debug.h"

#define DEBUG_MODULE DEBUG_SCHEDULER

#define SCHED_FAIR_MIN_GRANULARITY 50

typedef struct _scheduler_task scheduler_task;

struct _scheduler_task
{
	unsigned long pid;
	unsigned long runtime; // for tracking during scheduling
	unsigned long timebase; // overall timebase
	float weight; // weighting
	float inverseWeight; // 1 / weighting
};

typedef struct _scheduler_fair scheduler_fair;

struct _scheduler_fair
{
	rbtree* processTree; // overall rbtree timeline
	unsigned long timebaseOffset; // offset from expected end time (used to avoid collisions)
	float load; // overall load (sum of weights)
};

static void scheduler_fair_deallocate ( scheduler* sched )
{
	scheduler_fair* fair = (scheduler_fair*)sched->context;
	rbtree_free(fair->processTree);
	yfree(fair);
	yfree(sched);
}

static float prio_to_weight ( scheduler_priority prio )
{
	// map priority to weight
	switch (prio)
	{
		case SCHED_PRIO_TITANIC: return 5.0f;
		case SCHED_PRIO_VERY_HIGH: return 3.0f;
		case SCHED_PRIO_HIGH: return 2.0f;
		case SCHED_PRIO_NORMAL: return 1.0f;
		case SCHED_PRIO_LOW: return 0.6f;
		case SCHED_PRIO_VERY_LOW: return 0.2f;
	}
}

static void timeline_insert ( scheduler_fair* sched, scheduler_task* task, unsigned long timebase )
{
	// add a minimum granularity so we don't get everything crashing into itself
	timebase += SCHED_FAIR_MIN_GRANULARITY;
	// add the offset
	timebase += (sched->timebaseOffset += SCHED_FAIR_MIN_GRANULARITY);
	// avoid collisions
	while (rbtree_search(sched->processTree, timebase, 0))
		timebase++;
	DEBUG("put in task %lu on timeline for vtime %lu\n", task->pid, timebase);
	// insert into the timeline
	rbtree_insert(sched->processTree, timebase, task);
	task->timebase = timebase;
}

static void scheduler_fair_schedule ( scheduler* mst, unsigned long pid, scheduler_priority prio )
{
	scheduler_fair* sched = (scheduler_fair*)mst->context;
	scheduler_task* newTask;
	// get the lowest time
	unsigned long min = rbtree_empty(sched->processTree) ? 0 : rbtree_min(sched->processTree);
	// allocate a new task
	newTask = yalloc(sizeof(scheduler_task));
	newTask->pid = pid;
	newTask->runtime = 0;
	newTask->weight = prio_to_weight(prio);
	newTask->inverseWeight = 1.0f / newTask->weight;
	sched->load += newTask->weight;
	// insert into the timeline
	timeline_insert(sched, newTask, min);
}

static void scheduler_fair_reschedule ( scheduler* mst, scheduler_job* job )
{
	scheduler_fair* sched = (scheduler_fair*)mst->context;
	scheduler_task* lastTask = (scheduler_task*)job->data;
	if (lastTask)
	{
		// add used time and reinsert into the timeline
		assert(lastTask->runtime >= job->runtime);
		timeline_insert(sched, lastTask, lastTask->timebase + (lastTask->runtime - job->runtime) * lastTask->inverseWeight);
		sched->timebaseOffset = 0;
	}
}

static void scheduler_fair_unschedule ( scheduler* mst, scheduler_job* job )
{
	scheduler_fair* sched = (scheduler_fair*)mst->context;
	scheduler_task* lastTask = (scheduler_task*)job->data;
	if (lastTask)
	{
		// unscheduling.. account for weight loss
		sched->load -= lastTask->weight;
		yfree(lastTask);
	}
}

static void scheduler_fair_select ( scheduler* mst, scheduler_job* job )
{
	scheduler_fair* sched = (scheduler_fair*)mst->context;
	scheduler_task* lastTask;
	if (rbtree_empty(sched->processTree))
	{
		// no jobs
		job->pid = 0;
		job->runtime = 0;
		job->data = 0;
	}
	else
	{
		unsigned long min = rbtree_min(sched->processTree); // find the minimum process
		bool found = rbtree_search(sched->processTree, min, (void**)&lastTask);
		assert(found);
		rbtree_remove(sched->processTree, min); // remove it from the tree
		job->pid = lastTask->pid; // have it selected
		job->data = (unsigned long)lastTask;
		if (rbtree_empty(sched->processTree)) // tree now empty?
		{
			job->runtime = lastTask->runtime = YARNS_TIMESLICE * 10; // run for a nice long period of time
		}
		else
		{
			unsigned long mmin = rbtree_min(sched->processTree); // find out how long we run until it's time for the next process to have its share
			unsigned long rt = mmin - min;
			if (rt < SCHED_FAIR_MIN_GRANULARITY) // make sure we get a sensible amount of CPU time though
				rt = SCHED_FAIR_MIN_GRANULARITY;
			job->runtime = lastTask->runtime = rt;
		}
	}
}

scheduler* sched_allocate_fair ()
{
	scheduler* mst = yalloc(sizeof(scheduler));
	scheduler_fair* sched = yalloc(sizeof(scheduler));
	sched->processTree = rbtree_new();
	sched->timebaseOffset = 0;
	sched->load = 0.0f;
	mst->context = sched;
	mst->deallocate = scheduler_fair_deallocate;
	mst->schedule = scheduler_fair_schedule;
	mst->select = scheduler_fair_select;
	mst->reschedule = scheduler_fair_reschedule;
	mst->unschedule = scheduler_fair_unschedule;
	return mst;
}
