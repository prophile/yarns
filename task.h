#ifndef __YARNS_TASK__
#define __YARNS_TASK__

typedef struct _task* task_t;

task_t task_new ( void (*routine)(void*), void* udata );
void task_yield ( task_t target );
void task_process ();

#endif
