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
static unsigned long* single_jobs;
static unsigned long* jobcounts;

void smp_sched_init ( unsigned long procs )
{
	int i;
	DEBUG("smp_sched_init with procs=%lu\n", procs);
	nprocs = procs;
	locks = yalloc(procs*sizeof(lock_t));
	schedulers = yalloc(procs*sizeof(scheduler*));
	jobcounts = yalloc(procs*sizeof(unsigned long));
	single_jobs = yalloc(procs*sizeof(unsigned long));
	DEBUG("setting up proc 0, with scheduler\n");
	lock_init(&locks[0]);
	schedulers[0] = scheduler_init();
	for (i = 1; i < procs; i++)
	{
		DEBUG("setting up proc %lu\n", i);
		lock_init(locks + i);
		// commented out because yalloc now guarantees 0
		//schedulers[i] = NULL;
		//jobcounts[i] = 0;
	}
}

static unsigned long select_core_unused ()
{
	unsigned long i;
	for (i = 1; i < nprocs; i++)
	{
		if (!schedulers[i])
			return i;
	}
	return ~0UL;
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
	last_min_cd = 2;
	last_min = min;
	return min;
}

static unsigned long select_core_most_load ()
{
	unsigned long i, max = ~0UL, maxVal = 0, jc;
	for (i = 0; i < nprocs; i++)
	{
		jc = jobcounts[i];
		if (!single_jobs[i] && jc > maxVal) // ignore cores allocated for single jobs
		{
			maxVal = jc;
			max = i;
		}
	}
	if (maxVal == 0)
		return 0; // doesn't matter, scheduler will return pid = 0 anyway
	return max;
}

static scheduler* scheduler_for_core ( unsigned long core )
{
	if (!schedulers[core])
		schedulers[core] = scheduler_init();
	return schedulers[core];
}

void smp_sched_insert ( unsigned long pid, scheduler_priority prio )
{
	unsigned long target;
	if (prio == SCHED_PRIO_TITANIC)
	{
		// titanic means we should donate an entire core
		target = select_core_unused();
		if (target != ~0UL)
		{
			single_jobs[target] = pid;
			jobcounts[target] = ~0UL;
			return;
		}
	}
	target = select_core_least_load();
	DEBUG("scheduling process %lu for processor %lu\n", pid, target);
	lock_lock(locks + target);
	scheduler_insert(scheduler_for_core(target), pid, prio);
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
	scheduler_select(scheduler_for_core(core), job);
	lock_unlock(locks + core);
}

void smp_sched_select ( unsigned long core, scheduler_job* job )
{
	unsigned c = core;
	if (single_jobs[core])
	{
		if (job->pid == single_jobs[core] && job->runtime == SCHEDULER_UNSCHEDULE)
		{
			// we no longer want this core. report to sleep it and free stuff up
			single_jobs[core] = 0;
			jobcounts[core] = 0;
			job->pid = 0;
			job->runtime = 0;
			job->data = 0;
			job->priority = SCHED_PRIO_NORMAL;
			return;
		}
		job->pid = single_jobs[core];
		job->runtime = ~0UL - 1; // basically this means "run forever" :)
		job->data = 0;
		job->priority = SCHED_PRIO_TITANIC;
	}
	if (job->pid != 0 && job->runtime == SCHEDULER_UNSCHEDULE)
	{
		DEBUG("unscheduled job %lu for core %lu\n", job->pid, core);
		jobcounts[core]--;
	}
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
		//DEBUG("got job %lu for core %lu\n", job->pid, core);
	}
	if (job->pid == 0)
	{
		job->runtime = 0;
	}
}

#else

scheduler* _main_scheduler;

#endif
