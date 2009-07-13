#ifndef __YARNS_CONFIG__
#define __YARNS_CONFIG__

#define YARNS_STACK_PAGES 4
//#define YARNS_ENABLE_SMP

// schedulers
#define YARNS_SCHED_MULTILEVEL 1
#define YARNS_SCHED_ROUND_ROBIN 2
#define YARNS_SCHEDULER YARNS_SCHED_MULTILEVEL

// target
#define YARNS_TARGET_MACH 1
#define YARNS_TARGET_LINUX 2
#define YARNS_SELECTED_TARGET YARNS_TARGET_MACH

#endif
