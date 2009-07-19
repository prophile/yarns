#include "scheduler.h"
#include "alloc.h"
#include "debug.h"
#include "rbtree.h"
#include "config.h"
#include <assert.h>

#if YARNS_SCHEDULER == YARNS_SCHED_RB

#define DEBUG_MODULE DEBUG_SCHEDULER

#define RB_UNITS 1000000

struct _scheduler
{
	unsigned long vtime;
	rbtree* processTree;
};

scheduler* scheduler_init ()
{
	scheduler* sched = (scheduler*)yalloc(sizeof(scheduler));
	sched->processTree = rbtree_new();
	sched->vtime = 0;
	DEBUG("allocated new RB scheduler\n");
	return sched;
}

void scheduler_free ( scheduler* sched )
{
	rbtree_free(sched->processTree);
	yfree(sched);
}

static void insert ( scheduler* sched, unsigned long pid, unsigned long time )
{
	unsigned long vtime = sched->vtime + time;
	sched->vtime += 1;
	rbtree_insert(sched->processTree, time, (void*)pid);
	DEBUG("RB inserted job %lu for vtime %lu\n", pid, vtime);
}

void scheduler_insert ( scheduler* sched, unsigned long pid )
{
	insert(sched, pid, RB_UNITS);
}

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	unsigned long oldpid = job->pid;
	bool shouldReschedule = (job->runtime != SCHEDULER_UNSCHEDULE) && (job->pid != 0);
	unsigned long minTime = rbtree_min(sched->processTree);
	unsigned long runtime, totalTime = job->data;
	if (job->runtime > 0 && job->runtime != SCHEDULER_UNSCHEDULE)
	{
		runtime = totalTime - job->runtime;
	}
	else
	{
		runtime = totalTime;
	}
	sched->vtime += runtime;
	if (minTime == ~0UL)
	{
		job->pid = 0;
		job->runtime = 0;
		job->data = 0;
	}
	else
	{
		void* pidPtr;
		bool found = rbtree_search(sched->processTree, minTime, &pidPtr);
		if (found)
		{
			job->pid = (unsigned long)pidPtr;
			job->runtime = job->data = minTime - sched->vtime;
			if (job->runtime < YARNS_TIMESLICE)
			{
				job->runtime = YARNS_TIMESLICE;
				job->data = RB_UNITS;
			}
		}
		else
		{
			job->pid = 0;
			job->runtime = 0;
			job->data = 0;
		}
	}
	if (shouldReschedule)
	{
		insert(sched, oldpid, totalTime);
	}
}

#endif
