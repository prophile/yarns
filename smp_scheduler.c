#include "smp_scheduler.h"
#include "lock.h"
#include <stdlib.h>
#include <stdio.h>
#include "alloc.h"
#include <assert.h>
#include "debug.h"

#ifdef YARNS_ENABLE_SMP

#define DEBUG_MODULE DEBUG_SMP_SCHEDULER

static unsigned long nprocs;
static lock_t* locks;
static scheduler** schedulers;
static unsigned long* jobcounts;

void smp_sched_init ( unsigned long procs )
{
	int i;
	DEBUG("smp_sched_init with procs=%lu\n", procs);
	nprocs = procs;
	locks = yalloc(procs*sizeof(lock_t));
	schedulers = yalloc(procs*sizeof(scheduler*));
	jobcounts = yalloc(procs*sizeof(unsigned long));
	for (i = 0; i < procs; i++)
	{
		DEBUG("setting up scheduler for proc %lu\n", i);
		lock_init(locks + i);
		schedulers[i] = scheduler_init();
		jobcounts[i] = 0;
	}
}

static unsigned long select_core_least_load ()
{
	static unsigned long last_min = 0, last_min_cd = 0;
	unsigned long i, min = ~0UL, minVal = ~0UL, jc;
	if (last_min_cd > 0)
	{
		last_min_cd--;
		return last_min;
	}
	for (i = 0; i < nprocs; i++)
	{
		jc = jobcounts[i];
		if (jc < minVal)
		{
			minVal = jc;
			min = i;
		}
	}
	assert(minVal != ~0UL); // this makes very little sense
	last_min_cd = 3;
	last_min = min;
	return min;
}

static unsigned long select_core_most_load ()
{
	unsigned long i, max = ~0UL, maxVal = 0, jc;
	for (i = 0; i < nprocs; i++)
	{
		jc = jobcounts[i];
		if (jc > maxVal)
		{
			maxVal = jc;
			max = i;
		}
	}
	if (maxVal == 0)
		return 0; // doesn't matter, scheduler will return pid = 0 anyway
	return max;
}

void smp_sched_insert ( unsigned long pid, scheduler_priority prio )
{
	unsigned long target = select_core_least_load();
	DEBUG("scheduling process %lu for processor %lu\n", pid, target);
	lock_lock(locks + target);
	scheduler_insert(schedulers[target], pid, prio);
	jobcounts[target]++;
	lock_unlock(locks + target);
}

static void doselect ( unsigned long core, scheduler_job* job )
{
	lock_lock(locks + core);
	if (job->runtime == SCHEDULER_UNSCHEDULE && job->pid != 0)
	{
		jobcounts[core]--;
	}
	scheduler_select(schedulers[core], job);
	lock_unlock(locks + core);
}

void smp_sched_select ( unsigned long core, scheduler_job* job )
{
	unsigned c = core;
	doselect(c, job);
	if (job->pid == 0)
	{
		// no job found, designate this core for receiving jobs
		c = select_core_most_load();
		if (c != core && (jobcounts[c] - jobcounts[core]) >= 2)
		{
			doselect(c, job);
			if (job->pid != 0)
			{
				DEBUG("stole job %lu for core %lu from core %lu\n", job->pid, core, c);
				return;
			}
		}
	}
	else
	{
		DEBUG("got job %lu for core %lu\n", job->pid, core);
	}
	if (job->pid == 0)
	{
		job->runtime = 0;
	}
}

#else

scheduler* _main_scheduler;

#endif
