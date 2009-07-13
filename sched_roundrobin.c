#include "scheduler.h"
#include "lock.h"
#include <stdbool.h>
#include "config.h"
#include <stdlib.h>

#if YARNS_SCHEDULER == YARNS_SCHED_ROUND_ROBIN

// this scheduler keeps separate queues for each core
// basically cycles through jobs
// if it runs out of jobs on any core it will "steal" jobs from another

typedef struct _scheduler_queue_entry scheduler_queue_entry;

struct _scheduler_queue_entry
{
	unsigned long pid;
	scheduler_queue_entry* next;
};

static unsigned long nprocs;
static unsigned long procidx = 0;
static lock_t* proclock;
static scheduler_queue_entry** idxes;

#define HEAD(core) idxes[core]
#define TAIL(core) idxes[nprocs+core]
#define TIMESLICE 10000

void scheduler_init ( unsigned long procs )
{
	int i;
	idxes = (scheduler_queue_entry**)malloc(sizeof(scheduler_queue_entry*)*procs*2);
	proclock = malloc(sizeof(lock_t)*procs);
	for (i = 0; i < procs; i++)
	{
		HEAD(i) = TAIL(i) = 0;
		lock_init(&proclock[i]);
	}
	nprocs = procs;
}

static void insert ( unsigned long pid, unsigned long core )
{
	scheduler_queue_entry* newEntry;
	scheduler_queue_entry* prevTail;
	newEntry = (scheduler_queue_entry*)malloc(sizeof(scheduler_queue_entry));
	newEntry->pid = pid;
	newEntry->next = 0;
	lock_lock(&proclock[core]);
	prevTail = TAIL(core);
	if (prevTail)
	{
		TAIL(core)->next = newEntry;
		TAIL(core) = newEntry;
	}
	else
	{
		HEAD(core) = TAIL(core) = newEntry;
	}
	lock_unlock(&proclock[core]);
}

void scheduler_insert ( unsigned long pid )
{
	int i, done = 0;
	// find an empty processor if any
	for (i = 0; i < nprocs; i++)
	{
		if (!HEAD(i))
		{
			insert(pid, i);
			done = 1;
			break;
		}
	}
	if (!done)
	{
		insert(pid, procidx);
	}
}

static unsigned long jselect ( unsigned long core )
{
	unsigned long rv;
	lock_lock(&proclock[core]);
	if (HEAD(core))
	{
		rv = HEAD(core)->pid;
		HEAD(core) = HEAD(core)->next;
		if (!HEAD(core))
			TAIL(core) = 0;
	}
	else
	{
		rv = 0;
	}
	lock_unlock(&proclock[core]);
	return rv;
}

void scheduler_select ( scheduler_job* job )
{
	unsigned long core = job->processor, pid, idx;
	if (job->pid != 0 && job->runtime != SCHEDULER_UNSCHEDULE)
	{
		insert(job->pid, core);
	}
	pid = jselect(core);
	idx = (core + 1) % nprocs;
	if (idx == core) idx = 0;
	if (!pid)
	{
		while (!pid)
		{
			if (idx == core)
				break;
			// steal from another core
			pid = jselect(idx);
			idx = (idx + 1) % nprocs;
		}
		procidx = core; // hint that this is a good core to insert new jobs
	}
	job->pid = pid;
	job->runtime = TIMESLICE;
}

#endif
