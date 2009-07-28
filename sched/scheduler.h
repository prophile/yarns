#ifndef __YARNS_SCHED__
#define __YARNS_SCHED__

typedef enum _scheduler_priority
{
	SCHED_PRIO_TITANIC = 0,
	SCHED_PRIO_VERY_HIGH = 1,
	SCHED_PRIO_HIGH = 2,
	SCHED_PRIO_NORMAL = 3,
	SCHED_PRIO_LOW = 4,
	SCHED_PRIO_VERY_LOW = 5
} scheduler_priority;

typedef struct _scheduler_job
{
	unsigned long pid; // 0 if no previous job, return 0 for shutdown
	unsigned long runtime; // number of µs to run; remaining µs at end if voluntary, 0 if involuntary, SCHEDULER_UNSCHEDULE for unscheduling
	unsigned long data; // extra data defined by scheduler, 0 by default
	unsigned long next; // suggested next job, in only, 0 if none, feel free to ignore
	scheduler_priority priority; // initially SCHED_PRIO_NORMAL, only for scheduler's use (ignored by rest of system)
} scheduler_job;

typedef struct _scheduler scheduler;

#define SCHEDULER_UNSCHEDULE ~0UL

scheduler* scheduler_init ();
void scheduler_free ( scheduler* sched );
void scheduler_insert ( scheduler* sched, unsigned long pid, scheduler_priority prio );
void scheduler_select ( scheduler* sched, scheduler_job* job );

#endif
