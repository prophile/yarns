#include "scheduler.h"
#include <stdbool.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "alloc.h"
#include "debug.h"

#if YARNS_SCHEDULER == YARNS_SCHED_ROUND_ROBIN

#define DEBUG_MODULE DEBUG_SCHEDULER

// this scheduler keeps separate queues for each core
// basically cycles through jobs
// if it runs out of jobs on any core it will "steal" jobs from another

typedef struct _scheduler_queue_entry scheduler_queue_entry;

struct _scheduler_queue_entry
{
	unsigned long pid;
	scheduler_queue_entry* next;
};

struct _scheduler
{
	scheduler_queue_entry* head;
	scheduler_queue_entry* tail;
};

scheduler* scheduler_init ()
{
	scheduler* sched = (scheduler*)yalloc(sizeof(scheduler));
	sched->head = sched->tail = 0;
	DEBUG("Initted round robin scheduler.\n");
	return sched;
}

void scheduler_insert ( scheduler* sched, unsigned long pid, scheduler_priority prio )
{
	// the round robin is fair to everything and we ignore the priority
	scheduler_queue_entry* newEntry;
	scheduler_queue_entry* prevTail;
	newEntry = (scheduler_queue_entry*)yalloc(sizeof(scheduler_queue_entry));
	newEntry->pid = pid;
	newEntry->next = 0;
	prevTail = sched->tail;
	if (prevTail)
	{
		sched->tail->next = newEntry;
		sched->tail = newEntry;
	}
	else
	{
		sched->head = sched->tail = newEntry;
	}
}

static unsigned long jselect ( scheduler* sched )
{
	unsigned long rv;
	if (sched->head)
	{
		rv = sched->head->pid;
		sched->head = sched->head->next;
		if (!sched->head)
			sched->tail = 0;
	}
	else
	{
		rv = 0;
	}
	return rv;
}

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	unsigned long pid, idx;
	if (job->pid != 0 && job->runtime != SCHEDULER_UNSCHEDULE)
	{
		scheduler_insert(sched, job->pid, SCHED_PRIO_NORMAL);
	}
	pid = jselect(sched);
	job->pid = pid;
	job->runtime = pid == 0 ? YARNS_TIMESLICE : 0;
}

#endif
