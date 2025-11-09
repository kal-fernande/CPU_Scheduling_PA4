// =========================
// sched_core.c — Algorithms + helpers (FCFS/SJF/RR/Priority)
// =========================
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

// ---- bring ready queue API + structs (match ready_queue.c exactly) ----
typedef struct { int *idx, cap, head, tail, len; pthread_mutex_t mu; } readyq_t;
typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    sem_t run_sem;
} proc_t;

void rq_init(readyq_t *q, int capacity);
void rq_destroy(readyq_t *q);
bool rq_empty(readyq_t *q);
void rq_push(readyq_t *q, int proc_index);
int  rq_pop_fcfs(readyq_t *q);
int  rq_pop_min_remaining(readyq_t *q, const proc_t *procs);
int  rq_pop_best_priority(readyq_t *q, const proc_t *procs);

// ---- public helpers ----
void admit_arrivals(proc_t *procs, int nprocs, readyq_t *rq, int current_time) {
    for (int i=0; i<nprocs; ++i) {
        if (!procs[i].admitted && !procs[i].done && procs[i].arrival <= current_time) {
            procs[i].admitted = true; rq_push(rq, i);
        }
    }
}

void inc_waiting_all_except(readyq_t *rq, proc_t *procs, int running_idx) {
    pthread_mutex_lock(&rq->mu);
    int p = rq->head;
    for (int k=0; k<rq->len; ++k, p=(p+1)%rq->cap) {
        int i = rq->idx[p]; if (i != running_idx) procs[i].waiting_time++;
    }
    pthread_mutex_unlock(&rq->mu);
}

// ---- pickers ----
// FCFS: non-preemptive — if something is running, keep it
int pick_next_fcfs(readyq_t *rq, const proc_t *procs, int running_idx) {
    (void)procs;
    if (running_idx >= 0) return running_idx;
    return rq_pop_fcfs(rq);
}

// SJF (non-preemptive): choose smallest remaining when CPU idle
int pick_next_sjf(readyq_t *rq, const proc_t *procs, int running_idx) {
    if (running_idx >= 0) return running_idx;
    return rq_pop_min_remaining(rq, procs);
}

// Round Robin (preemptive): rotate on quantum
int pick_next_rr(readyq_t *rq, const proc_t *procs, int running_idx, int *rr_budget, int quantum) {
    (void)procs;
    if (running_idx < 0 || *rr_budget == 0) {
        if (running_idx >= 0) rq_push(rq, running_idx); // requeue previous
        *rr_budget = quantum;
        return rq_pop_fcfs(rq);
    }
    return running_idx;
}

// Priority (preemptive, lower value = higher priority): reconsider each tick
int pick_next_priority(readyq_t *rq, const proc_t *procs, int running_idx) {
    if (running_idx >= 0) rq_push(rq, running_idx);
    return rq_pop_best_priority(rq, procs);
}

// ---- optional sem lifecycle ----
void proc_sems_init(proc_t *procs, int n) { for (int i=0;i<n;++i) sem_init(&procs[i].run_sem, 0, 0); }
void proc_sems_destroy(proc_t *procs, int n){ for (int i=0;i<n;++i) sem_destroy(&procs[i].run_sem); }
