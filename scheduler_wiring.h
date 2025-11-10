#pragma once
#include <pthread.h>
#include <stdbool.h>

/* Avoid POSIX macro collisions with our enum labels in cmdparser.h */
#ifdef SCHED_RR
#undef SCHED_RR
#endif
#ifdef SCHED_FIFO
#undef SCHED_FIFO
#endif
#ifdef SCHED_OTHER
#undef SCHED_OTHER
#endif

#include "cmdparser.h"   // provides scheduler_t (SCHED_NONE/FCFS/SJF/RR/PRIORITY)

/* ---- portable gate (replaces unnamed POSIX semaphores) ---- */
typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    int             count;   // counting semaphore
} gate_t;

static inline void gate_init(gate_t *g, int initial){
    pthread_mutex_init(&g->mu, NULL);
    pthread_cond_init(&g->cv, NULL);
    g->count = (initial < 0) ? 0 : initial;
}
static inline void gate_destroy(gate_t *g){
    pthread_cond_destroy(&g->cv);
    pthread_mutex_destroy(&g->mu);
}
static inline void gate_post(gate_t *g){
    pthread_mutex_lock(&g->mu);
    g->count += 1;
    pthread_cond_signal(&g->cv);
    pthread_mutex_unlock(&g->mu);
}
static inline void gate_wait(gate_t *g){
    pthread_mutex_lock(&g->mu);
    while (g->count == 0) pthread_cond_wait(&g->cv, &g->mu);
    g->count -= 1;
    pthread_mutex_unlock(&g->mu);
}
/* ----------------------------------------------------------- */

/* Shared structs */
typedef struct {
    int *idx, cap, head, tail, len;
    pthread_mutex_t mu;
} readyq_t;

typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    gate_t run_gate;   // was: sem_t run_sem
} proc_t;

/* Core ready-queue / scheduler API */
void rq_init(readyq_t *q, int capacity);
void rq_destroy(readyq_t *q);
bool rq_empty(readyq_t *q);
void rq_push(readyq_t *q, int proc_index);
int  rq_pop_fcfs(readyq_t *q);
int  rq_pop_min_remaining(readyq_t *q, const proc_t *procs);
int  rq_pop_best_priority(readyq_t *q, const proc_t *procs);

void admit_arrivals(proc_t *procs, int nprocs, readyq_t *rq, int current_time);
void inc_waiting_all_except(readyq_t *rq, proc_t *procs, int running_idx);
int  pick_next_fcfs(readyq_t *rq, const proc_t *procs, int running_idx);
int  pick_next_sjf (readyq_t *rq, const proc_t *procs, int running_idx);
int  pick_next_rr  (readyq_t *rq, const proc_t *procs, int running_idx, int *rr_budget, int quantum);
int  pick_next_priority(readyq_t *rq, const proc_t *procs, int running_idx);

/* Main entry */
int run_scheduler(proc_t *procs, int nprocs, scheduler_t alg, int quantum,
                  int **out_timeline, int *out_tl_len);