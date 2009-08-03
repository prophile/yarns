#ifndef __YARNS_WAIT_GRAPH__
#define __YARNS_WAIT_GRAPH__

#include "timer.h"
#include "yarn.h"

typedef struct _wait_graph wait_graph;

wait_graph* wait_graph_new ();
void wait_graph_delete ( wait_graph* graph );

void wait_graph_lock ( wait_graph* graph );
void wait_graph_unlock ( wait_graph* graph );

// process all time objects
void wait_graph_time_process ( wait_graph* graph );

// get the next signal token
unsigned long wait_graph_next_token ( wait_graph* graph );
// signal a new token
void wait_graph_signal ( wait_graph* graph, unsigned long token );

void wait_graph_insert ( wait_graph* graph, yarn_t yarn );
void wait_graph_insert_token ( wait_graph* graph, yarn_t yarn, unsigned long token );
void wait_graph_insert_time ( wait_graph* graph, yarn_t yarn, yarns_time_t time );

#endif
