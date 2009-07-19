#ifndef __YARNS_SCHED__
#define __YARNS_SCHED__

typedef struct _scheduler_job
{
	unsigned long pid; // 0 if no previous job, return 0 for shutdown
	unsigned long runtime; // number of µs to run; remaining µs at end if voluntary, 0 if involuntary, SCHEDULER_UNSCHEDULE for unscheduling
	unsigned long data; // extra data defined by scheduler, 0 by default
} scheduler_job;

typedef struct _scheduler scheduler;

#define SCHEDULER_UNSCHEDULE ~0UL

scheduler* scheduler_init ();
void scheduler_free ( scheduler* sched );
void scheduler_insert ( scheduler* sched, unsigned long pid );
void scheduler_select ( scheduler* sched, scheduler_job* job );

#endif
