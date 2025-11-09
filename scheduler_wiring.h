// scheduler_wiring.h
#pragma once
#include "cmdparser.h"
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>

// Mirror the core structs used by ready_queue.c and sched_core.c
typedef struct { int *idx, cap, head, tail, len; pthread_mutex_t mu; } readyq_t;

typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    sem_t run_sem; // per-proc semaphore (one tick per post)
} proc_t;

// Core ready-queue / scheduler API provided by your two core files
void rq_init(readyq_t *q, int capacity);
void rq_destroy(readyq_t *q);
void rq_push(readyq_t *q, int proc_index);
int  rq_pop_fcfs(readyq_t *q);

void admit_arrivals(proc_t *procs, int nprocs, readyq_t *rq, int current_time);
void inc_waiting_all_except(readyq_t *rq, proc_t *procs, int running_idx);
int  pick_next_fcfs(readyq_t *rq, const proc_t *procs, int running_idx);
int  pick_next_sjf (readyq_t *rq, const proc_t *procs, int running_idx);
int  pick_next_rr  (readyq_t *rq, const proc_t *procs, int running_idx, int *rr_budget, int quantum);
int  pick_next_priority(readyq_t *rq, const proc_t *procs, int running_idx);
void proc_sems_init(proc_t *procs, int n);
void proc_sems_destroy(proc_t *procs, int n);

// Runs the simulation. Returns the makespan (total time).
// Optionally returns a malloc'd timeline array of length *out_tl_len (indices into procs; -1 = idle).
// Caller owns *out_timeline and must free() it if non-NULL.
int run_scheduler(proc_t *procs, int nprocs, scheduler_t alg, int quantum,
                  int **out_timeline, int *out_tl_len);
