#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>


typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    int             count;  
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


typedef struct { int *idx, cap, head, tail, len; pthread_mutex_t mu; } readyq_t;

typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    gate_t run_gate;   // was: sem_t run_sem
} proc_t;

void rq_init(readyq_t *q, int capacity);
void rq_destroy(readyq_t *q);
bool rq_empty(readyq_t *q);
void rq_push(readyq_t *q, int proc_index);
int  rq_pop_fcfs(readyq_t *q);
int  rq_pop_min_remaining(readyq_t *q, const proc_t *procs);
int  rq_pop_best_priority(readyq_t *q, const proc_t *procs);


void admit_arrivals(proc_t *procs, int nprocs, readyq_t *rq, int current_time) {
    int *to_push = (int*)malloc(sizeof(int) * nprocs);
    int push_count = 0;

    pthread_mutex_lock(&rq->mu);
    for (int i = 0; i < nprocs; ++i) {
        if (!procs[i].admitted && !procs[i].done && procs[i].arrival <= current_time) {
            procs[i].admitted = true;         // write under rq->mu (fix #4)
            to_push[push_count++] = i;        // defer queue mutation until after unlock
        }
    }
    pthread_mutex_unlock(&rq->mu);

    for (int k = 0; k < push_count; ++k) rq_push(rq, to_push[k]);
    free(to_push);
}

void inc_waiting_all_except(readyq_t *rq, proc_t *procs, int running_idx) {
    pthread_mutex_lock(&rq->mu);
    int p = rq->head;
    for (int k = 0; k < rq->len; ++k, p = (p + 1) % rq->cap) {
        int i = rq->idx[p];
        if (i != running_idx) procs[i].waiting_time++; 
    }
    pthread_mutex_unlock(&rq->mu);
}


int pick_next_fcfs(readyq_t *rq, const proc_t *procs, int running_idx) {
    (void)procs;
    if (running_idx >= 0) return running_idx;
    return rq_pop_fcfs(rq);
}

int pick_next_sjf(readyq_t *rq, const proc_t *procs, int running_idx) {
    if (running_idx >= 0) return running_idx;
    return rq_pop_min_remaining(rq, procs);
}

int pick_next_rr(readyq_t *rq, const proc_t *procs, int running_idx, int *rr_budget, int quantum) {
    (void)procs;
    if (running_idx < 0 || *rr_budget == 0) {
        if (running_idx >= 0) rq_push(rq, running_idx); // requeue previous
        *rr_budget = quantum;
        return rq_pop_fcfs(rq);
    }
    return running_idx;
}

int pick_next_priority(readyq_t *rq, const proc_t *procs, int running_idx) {
    if (running_idx >= 0) rq_push(rq, running_idx);
    return rq_pop_best_priority(rq, procs);
}

void proc_sems_init(proc_t *procs, int n)  { for (int i = 0; i < n; ++i) gate_init(&procs[i].run_gate, 0); }
void proc_sems_destroy(proc_t *procs, int n){ for (int i = 0; i < n; ++i) gate_destroy(&procs[i].run_gate); }

