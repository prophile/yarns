#include "smp_scheduler.h"
#include "lock.h"
#include <stdlib.h>

#ifdef YARNS_ENABLE_SMP

static unsigned long nprocs;
static lock_t* locks;
static scheduler** schedulers;
static unsigned long activeproc;

void smp_sched_init ( unsigned long procs )
{
	int i;
	nprocs = procs;
	locks = malloc(procs*sizeof(lock_t));
	schedulers = malloc(procs*sizeof(scheduler*));
	for (i = 0; i < procs; i++)
	{
		lock_init(locks + i);
		schedulers[i] = scheduler_init();
	}
}

void smp_sched_insert ( unsigned long pid )
{
	unsigned long target = activeproc;
	lock_lock(locks + target);
	scheduler_insert(schedulers[target], pid);
	lock_unlock(locks + target);
}

static void doselect ( unsigned long core, scheduler_job* job )
{
	lock_lock(locks + core);
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
		activeproc = core;
		do
		{
			c = (c+1) % nprocs;
			doselect(c, job);
			if (job->pid != 0)
				return;
		} while (c != core);
	}
	if (job->pid == 0)
	{
		job->runtime = 0;
	}
}

#else

scheduler* _main_scheduler;

#endif
