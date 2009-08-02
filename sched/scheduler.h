#ifndef __YARNS_SCHEDULER__
#define __YARNS_SCHEDULER__

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
	unsigned long next; // suggested next job, in only, 0 if none, feel free to ignore; SCHEDULER_WANT_IDLE if wants no next job
	scheduler_priority priority; // initially SCHED_PRIO_NORMAL, only for scheduler's use (ignored by rest of system)
} scheduler_job;

typedef struct _scheduler scheduler;

struct _scheduler
{
	void (*deallocate)(scheduler* sched);
	void (*schedule)(scheduler* sched, unsigned long pid, scheduler_priority prio);
	void (*select)(scheduler* sched, scheduler_job* job);
	void (*reschedule)(scheduler* sched, scheduler_job* job);
	void (*unschedule)(scheduler* sched, scheduler_job* job);
	void* context;
};

#define scheduler_deallocate(sched) sched->deallocate(sched)
#define scheduler_schedule(sched,pid,prio) sched->schedule(sched,pid,prio)
#define scheduler_select(sched,job) sched->select(sched,job)
#define scheduler_reschedule(sched,job) sched->reschedule(sched,job)
#define scheduler_unschedule(sched,job) sched->unschedule(sched,job)

scheduler* sched_allocate_round_robin ();
scheduler* sched_allocate_staircase ();
scheduler* sched_allocate_fair ();

#endif
