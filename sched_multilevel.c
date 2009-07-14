#include "scheduler.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#if YARNS_SCHEDULER == YARNS_SCHED_MULTILEVEL

#define TIMESLICE 10000
#define LAYERS 20

typedef struct _sched_queue_job sched_queue_job;

struct _sched_queue_job
{
	unsigned long pid;
	unsigned long skipper_index;
	sched_queue_job* next;
};

struct _scheduler
{
	unsigned long skipper_index;
	sched_queue_job* layer_heads[LAYERS];
	sched_queue_job* layer_tails[LAYERS];
};

static void panic ( const char* msg )
{
	printf("Panic: %s\n", msg);
	_Exit(1);
}

scheduler* scheduler_init ()
{
	scheduler* sched;
	int i;
	sched = (scheduler*)malloc(sizeof(scheduler));
	sched->skipper_index = 1;
	for (i = 0; i < LAYERS; i++)
	{
		sched->layer_heads[i] = 0;
		sched->layer_tails[i] = 0;
	}
	return sched;
}

static void trueselect ( scheduler* sched, scheduler_job* job, int secondary );

void scheduler_free ( scheduler* sched )
{
	scheduler_job j;
	do
	{
		trueselect(sched, &j, 0);
		j.runtime = SCHEDULER_UNSCHEDULE;
	} while (j.pid != 0);
}

static void insert ( scheduler* sched, unsigned long pid, int skip, int level )
{
	if (!sched->layer_heads[level])
	{
		sched->layer_heads[level] = sched->layer_tails[level] = malloc(sizeof(sched_queue_job));
		sched->layer_heads[level]->pid = pid;
		sched->layer_heads[level]->next = 0;
		sched->layer_heads[level]->skipper_index = skip ? sched->skipper_index : 0;
	}
	else
	{
		if (!sched->layer_tails[level])
			panic("bad layer tail");
		sched->layer_tails[level]->next = malloc(sizeof(sched_queue_job));
		sched->layer_tails[level] = sched->layer_tails[level]->next;
		sched->layer_tails[level]->pid = pid;
		sched->layer_tails[level]->next = 0;
		sched->layer_tails[level]->skipper_index = skip ? sched->skipper_index : 0;
	}
}

void scheduler_insert ( scheduler* sched, unsigned long pid )
{
	insert(sched, pid, 0, 0);
}

static void trueselect ( scheduler* sched, scheduler_job* job, int secondary )
{
	int i, found = 0;
	if (job->pid > 0)
	{
		if (job->runtime == 0)
		{
			insert(sched, job->pid, 0, (job->data == LAYERS - 1) ? job->data : (job->data + 1));
		}
		else
		{
			insert(sched, job->pid, 1, job->data);
		}
	}
	for (i = 0; i < LAYERS; i++)
	{
		if (!sched->layer_heads[i])
			continue;
		sched_queue_job* qjob = sched->layer_heads[i];
		sched->layer_heads[i] = sched->layer_heads[i]->next;
		if (sched->layer_heads[i] == 0)
			sched->layer_tails[i] = 0;
		if (qjob->skipper_index == sched->skipper_index)
			continue;
		job->pid = qjob->pid;
		job->data = i;
		job->runtime = TIMESLICE;
		free(qjob);
		found = 1;
		break;
	}
	if (!found)
	{
		if (secondary)
		{
			job->pid = 0;
			job->runtime = 0;
			job->data = 0;
		}
		else
		{
			sched->skipper_index++;
			trueselect(sched, job, 1);
		}
	}
}

void scheduler_select ( scheduler* sched, scheduler_job* job )
{
	trueselect(sched, job, 0);
}

#endif
