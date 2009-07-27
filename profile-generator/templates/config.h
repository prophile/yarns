#ifndef __YARNS_CONFIG__
#define __YARNS_CONFIG__

// general config
#define YARNS_STACK_PAGES 8
#define YARNS_ENABLE_SMP
#define YARNS_DEAD_SLEEP_TIME {$dead_sleep_time}

// schedulers
#define YARNS_SCHED_MULTILEVEL 1
#define YARNS_SCHED_ROUND_ROBIN 2
#define YARNS_SCHED_RANDOM 3
#define YARNS_SCHED_RB 4
#define YARNS_SCHED_STAIRCASE 5
#define YARNS_SCHEDULER {$scheduler}
// in µs
#define YARNS_TIMESLICE {$timeslice}

// synergy methods
#define YARNS_SYNERGY_COOPERATIVE 1
#define YARNS_SYNERGY_MARKED 2
#define YARNS_SYNERGY_PREEMPTIVE 3
#define YARNS_SYNERGY {$synergy}

// memory allocation and control
{if $memory_access_control}#define YARNS_MEMORY_ACCESS_CONTROL{/if}
{if $use_system_allocator}#define YARNS_USE_SYSTEM_ALLOCATOR{/if}

// target
#define YARNS_TARGET_MACH 1
#define YARNS_TARGET_LINUX 2
#define YARNS_SELECTED_TARGET {$target}

// maxima
#define YARNS_MAX_PROCESSES {$max_processes}

// overrides
{if $override_core_count}#define YARNS_OVERRIDE_CORE_COUNT {$override_core_count}{/if}

#endif
