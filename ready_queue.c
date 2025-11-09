// =========================
// ready_queue.c  — Ready queue implementation (thread-safe)
// =========================
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

// ---- shared structs (keep identical to sched_core.c) ----
typedef struct {
    int *idx;               // circular buffer storing indices into proc[]
    int cap, head, tail, len;
    pthread_mutex_t mu;     // protect producers/consumers
} readyq_t;

typedef struct {
    char pid[32];
    int arrival, burst, priority;
    int remaining, started_time, finish_time, response_time, waiting_time;
    bool admitted, done;
    sem_t run_sem; // per-proc semaphore (one tick per post)
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
    q->idx = (int*)malloc(sizeof(int) * capacity);
    q->cap = capacity;
    q->head = q->tail = q->len = 0;
    pthread_mutex_init(&q->mu, NULL);
}

void rq_destroy(readyq_t *q) {
    if (!q) return;
    free(q->idx); q->idx = NULL;
    q->cap = q->head = q->tail = q->len = 0;
    pthread_mutex_destroy(&q->mu);
}

bool rq_empty(readyq_t *q) { return q->len == 0; }

// Idempotent push (ignores duplicates)
void rq_push(readyq_t *q, int i) {
    pthread_mutex_lock(&q->mu);
    int p = q->head;
    for (int k=0; k<q->len; ++k, p=(p+1)%q->cap) {
        if (q->idx[p] == i) { pthread_mutex_unlock(&q->mu); return; }
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
    int p = q->head, tail=0;
    for (int k=0; k<q->len; ++k, p=(p+1)%q->cap) {
        int v = q->idx[p];
        if (v != target) tmp[tail++] = v;
    }
    memcpy(q->idx, tmp, sizeof(int)*tail);
    q->head = 0; q->tail = tail; q->len = tail;
    free(tmp);
    return target;
}

int rq_pop_min_remaining(readyq_t *q, const proc_t *procs) {
    pthread_mutex_lock(&q->mu);
    if (q->len == 0) { pthread_mutex_unlock(&q->mu); return -1; }
    int best=-1, best_rem=0x3fffffff, p=q->head;
    for (int k=0; k<q->len; ++k, p=(p+1)%q->cap) {
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
    int best=-1, best_pr=0x3fffffff, p=q->head;
    for (int k=0; k<q->len; ++k, p=(p+1)%q->cap) {
        int i = q->idx[p];
        if (procs[i].priority < best_pr) { best_pr = procs[i].priority; best = i; }
    }
    if (best >= 0) remove_entry_unlocked(q, best);
    pthread_mutex_unlock(&q->mu);
    return best;
}
