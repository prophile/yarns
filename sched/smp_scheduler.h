#ifndef __YARNS_SMP_SCHED__
#define __YARNS_SMP_SCHED__

#include "config.h"
#include "scheduler.h"

#ifdef YARNS_ENABLE_SMP

void smp_sched_init ( unsigned long nprocs );
void smp_sched_insert ( unsigned long pid, scheduler_priority prio );
void smp_sched_select ( unsigned long core, scheduler_job* job );

#else

extern scheduler* _main_scheduler;

static inline void smp_sched_init ( unsigned long nprocs )
	{ _main_scheduler = scheduler_init(); }
static inline void smp_sched_insert ( unsigned long pid, scheduler_priority prio )
	{ scheduler_insert(_main_scheduler, pid, prio); }
static inline void smp_sched_select ( unsigned long core, scheduler_job* job )
	{ scheduler_select(_main_scheduler, job); }

#endif

#endif
