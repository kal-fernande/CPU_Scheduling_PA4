#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* -------- Portable per-proc gate (replaces unnamed POSIX semaphores) -------- */
typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    int             count;   // acts like a counting semaphore
} gate_t;

static inline void gate_init(gate_t *g, int initial) {
    pthread_mutex_init(&g->mu, NULL);
    pthread_cond_init(&g->cv, NULL);
    g->count = (initial < 0) ? 0 : initial;
}
static inline void gate_destroy(gate_t *g) {
    pthread_cond_destroy(&g->cv);
    pthread_mutex_destroy(&g->mu);
}
static inline void gate_post(gate_t *g) {
    pthread_mutex_lock(&g->mu);
    g->count += 1;
    pthread_cond_signal(&g->cv);
    pthread_mutex_unlock(&g->mu);
}
static inline void gate_wait(gate_t *g) {
    pthread_mutex_lock(&g->mu);
    while (g->count == 0) pthread_cond_wait(&g->cv, &g->mu);
    g->count -= 1;
    pthread_mutex_unlock(&g->mu);
}
/* --------------------------------------------------------------------------- */

typedef struct {
    int *idx;               
    int cap, head, tail, len;
    pthread_mutex_t mu;    
} readyq_t;

typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    gate_t run_gate;        
} proc_t;


// ---- API (used by your CLI / main.c) ----
void rq_init(readyq_t *q, int capacity);
void rq_destroy(readyq_t *q);
bool rq_empty(readyq_t *q);
void rq_push(readyq_t *q, int proc_index);
int  rq_pop_fcfs(readyq_t *q);
int  rq_pop_min_remaining(readyq_t *q, const proc_t *procs);   // SJF helper
int  rq_pop_best_priority(readyq_t *q, const proc_t *procs);   // Priority helper

// ---- implementation ----
void rq_init(readyq_t *q, int capacity) {
    q->idx  = (int*)malloc(sizeof(int) * capacity);
    q->cap  = capacity;
    q->head = q->tail = q->len = 0;
    pthread_mutex_init(&q->mu, NULL);
}

void rq_destroy(readyq_t *q) {
    if (!q) return;
    free(q->idx); q->idx = NULL;
    q->cap = q->head = q->tail = q->len = 0;
    pthread_mutex_destroy(&q->mu);
}

/* (fix #1) Make rq_empty synchronized to avoid data race on len */
bool rq_empty(readyq_t *q) {
    bool empty;
    pthread_mutex_lock(&q->mu);
    empty = (q->len == 0);
    pthread_mutex_unlock(&q->mu);
    return empty;
}

/* Idempotent push (ignores duplicates) + (fix #2) capacity guard (non-blocking drop) */
void rq_push(readyq_t *q, int i) {
    pthread_mutex_lock(&q->mu);

    // reject duplicates
    int p = q->head;
    for (int k = 0; k < q->len; ++k, p = (p + 1) % q->cap) {
        if (q->idx[p] == i) { pthread_mutex_unlock(&q->mu); return; }
    }

    // capacity guard: drop if full (simple/non-blocking)
    if (q->len == q->cap) {
        pthread_mutex_unlock(&q->mu);
        return;
    }

    q->idx[q->tail] = i;
    q->tail = (q->tail + 1) % q->cap;
    q->len++;
    pthread_mutex_unlock(&q->mu);
}

int rq_pop_fcfs(readyq_t *q) {
    pthread_mutex_lock(&q->mu);
    if (q->len == 0) { pthread_mutex_unlock(&q->mu); return -1; }
    int i = q->idx[q->head];
    q->head = (q->head + 1) % q->cap;
    q->len--;
    pthread_mutex_unlock(&q->mu);
    return i;
}

static int remove_entry_unlocked(readyq_t *q, int target) {
    int *tmp = (int*)malloc(sizeof(int) * q->cap);
    int p = q->head, tail = 0;
    for (int k = 0; k < q->len; ++k, p = (p + 1) % q->cap) {
        int v = q->idx[p];
        if (v != target) tmp[tail++] = v;
    }
    memcpy(q->idx, tmp, sizeof(int) * tail);
    q->head = 0; q->tail = tail; q->len = tail;
    free(tmp);
    return target;
}

/* (fix #4) Reads of procs[] under q->mu; writers must hold q->mu too */
int rq_pop_min_remaining(readyq_t *q, const proc_t *procs) {
    pthread_mutex_lock(&q->mu);
    if (q->len == 0) { pthread_mutex_unlock(&q->mu); return -1; }
    int best = -1, best_rem = 0x3fffffff, p = q->head;
    for (int k = 0; k < q->len; ++k, p = (p + 1) % q->cap) {
        int i = q->idx[p];
        if (procs[i].remaining < best_rem) { best_rem = procs[i].remaining; best = i; }
    }
    if (best >= 0) remove_entry_unlocked(q, best);
    pthread_mutex_unlock(&q->mu);
    return best;
}

int rq_pop_best_priority(readyq_t *q, const proc_t *procs) {
    pthread_mutex_lock(&q->mu);
    if (q->len == 0) { pthread_mutex_unlock(&q->mu); return -1; }
    int best = -1, best_pr = 0x3fffffff, p = q->head;
    for (int k = 0; k < q->len; ++k, p = (p + 1) % q->cap) {
        int i = q->idx[p];
        if (procs[i].priority < best_pr) { best_pr = procs[i].priority; best = i; }
    }
    if (best >= 0) remove_entry_unlocked(q, best);
    pthread_mutex_unlock(&q->mu);
    return best;
}
