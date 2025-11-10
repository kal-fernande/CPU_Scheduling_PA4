#ifndef METRICS_H
#define METRICS_H

#include "scheduler_wiring.h"

// Holds global
typedef struct {
    double avg_wait, avg_resp, avg_turn;
    double throughput;       // jobs / tick
    double cpu_utilization; 
} metrics_t;


metrics_t compute_and_print_metrics(proc_t *procs, int nprocs, int makespan, const int *timeline, int tl_len);

#endif