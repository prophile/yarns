#include "scheduler.h"
#include "config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#if YARNS_SCHEDULER == YARNS_SCHED_RANDOM

#define JOBLIST_LEN (1024*16)-1

static int compar_xor = 0xFEEDFACE;

static int compar ( const void* _a, const void* _b )
{
	unsigned long a = *(const unsigned long*)_a;
	unsigned long b = *(const unsigned long*)_b;
	srand(a ^ compar_xor);
	a = rand();
	srand(b ^ compar_xor);
	b = rand();
	return a - b;
}

typedef struct _scheduler_joblist
{
	unsigned long n;
	unsigned long jobs[JOBLIST_LEN];
} scheduler_joblist;

static void shuffle ( scheduler_joblist* selectedList )
{
	srand(compar_xor);
	compar_xor = rand();
	qsort(selectedList->jobs, selectedList->n, sizeof(unsigned long), compar);
}

struct _scheduler
{
	scheduler_joblist* baselist;
};

scheduler* scheduler_init ()
{
	scheduler* sched = malloc(sizeof(scheduler));
	sched->baselist = malloc(sizeof(scheduler_joblist));
	sched->baselist->n = 0;
	printf("Initted random scheduler\n");
	compar_xor = time(NULL);
	return sched;
}

void scheduler_free ( scheduler* sched )
{
	free(sched->baselist);
	free(sched);
}

void scheduler_insert ( scheduler* sched, unsigned long pid )
{
	scheduler_joblist* selectedList = sched->baselist;
	assert(selectedList->n < JOBLIST_LEN);
	selectedList->jobs[selectedList->n++] = pid;
	shuffle(selectedList);
}

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	unsigned long oldpid = job->pid;
	bool shouldReschedule = job->runtime != SCHEDULER_UNSCHEDULE;
	scheduler_joblist* selectedList = sched->baselist;
	if (selectedList->n == 0)
	{
		if (shouldReschedule)
		{
			job->runtime = YARNS_TIMESLICE;
			return;
		}
		else
		{
			job->pid = 0;
			job->runtime = 0;
		}
	}
	else
	{
		job->pid = selectedList->jobs[--selectedList->n];
		job->runtime = YARNS_TIMESLICE;
	}
	if (shouldReschedule)
	{
		scheduler_insert(sched, oldpid);
	}
}

#endif
