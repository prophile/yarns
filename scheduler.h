#ifndef __YARNS_SCHED__
#define __YARNS_SCHED__

typedef struct _scheduler_job
{
	unsigned long pid; // 0 if no previous job, return 0 for shutdown
	unsigned long runtime; // number of ns to run; remaining ns at end if voluntary, 0 if involuntary, SCHEDULER_UNSCHEDULE for unscheduling
	unsigned long processor; // changes ignored, indicates number of processors
	unsigned long data; // extra data defined by scheduler, 0 by default
} scheduler_job;

#define SCHEDULER_UNSCHEDULE ~0UL

void scheduler_init ( unsigned long procs ); // number of processors
void scheduler_insert ( unsigned long pid );
void scheduler_select ( scheduler_job* job );

#endif
