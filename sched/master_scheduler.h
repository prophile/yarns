#ifndef __YARNS_MASTER_SCHED__
#define __YARNS_MASTER_SCHED__

#include "config.h"
#include "scheduler.h"

#define SCHED_ALLOCATE_PRIMARY sched_allocate_fair
#define SCHED_ALLOCATE_SECONDARY sched_allocate_staircase

#ifdef YARNS_ENABLE_SMP

void master_sched_init ( unsigned long nprocs );
void master_sched_schedule ( unsigned long pid, scheduler_priority prio );
void master_sched_select ( unsigned long core, scheduler_job* job );
void master_sched_reschedule ( unsigned long core, scheduler_job* job );
void master_sched_unschedule ( unsigned long core, scheduler_job* job );

#else

extern scheduler* _main_scheduler;

static inline void master_sched_init ( unsigned long nprocs )
	{ _main_scheduler = SCHED_ALLOCATE_PRIMARY(); }
static inline void master_sched_schedule ( unsigned long pid, scheduler_priority prio )
	{ scheduler_schedule(_main_scheduler, pid, prio); }
static inline void master_sched_select ( unsigned long core, scheduler_job* job )
	{ scheduler_select(_main_scheduler, job); }
static inline void master_sched_reschedule ( unsigned long core, scheduler_job* job )
	{ scheduler_reschedule(_main_scheduler, job); }
static inline void master_sched_unschedule ( unsigned long core, scheduler_job* job )
	{ scheduler_unschedule(_main_scheduler, job); }

#endif

#endif
