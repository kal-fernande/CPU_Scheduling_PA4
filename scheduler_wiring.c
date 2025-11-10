#include "scheduler_wiring.h"
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

// Globals for worker access (kept internal to this module)
static proc_t  *G_PROCS = NULL;
static int      G_NOW   = 0;
static gate_t   G_TICK_DONE;   // worker posts at end of its 1-tick slice
static readyq_t *G_RQ   = NULL; // to lock around procs[] updates in worker

static void *worker(void *arg) {
    int idx = (int)(intptr_t)arg;
    proc_t *p = &G_PROCS[idx];

    for (;;) {
        gate_wait(&p->run_gate);  // wait for 1 tick or exit signal

        // Check/modify process fields under the ready-queue mutex (race-free)
        pthread_mutex_lock(&G_RQ->mu);

        if (p->done) { // scheduler signaled exit
            pthread_mutex_unlock(&G_RQ->mu);
            break;
        }

        if (p->started_time < 0) {
            p->started_time = G_NOW;
            p->response_time = G_NOW - p->arrival;
        }

        p->remaining -= 1; // consume exactly one CPU tick

        pthread_mutex_unlock(&G_RQ->mu);

        // notify scheduler that the tick completed
        gate_post(&G_TICK_DONE);
    }
    return NULL;
}

int run_scheduler(proc_t *procs, int nprocs, scheduler_t alg, int quantum,
                  int **out_timeline, int *out_tl_len) {
    // init globals and barrier
    G_PROCS = procs; G_NOW = 0;
    gate_init(&G_TICK_DONE, 0);

    // ready queue and per-proc gates
    readyq_t rq; rq_init(&rq, nprocs * 2);
    G_RQ = &rq;
    for (int i=0;i<nprocs;++i) gate_init(&procs[i].run_gate, 0);

    // spawn workers
    pthread_t *ths = (pthread_t*)malloc(sizeof(pthread_t)*nprocs);
    for (int i=0;i<nprocs;++i) pthread_create(&ths[i], NULL, worker, (void*)(intptr_t)i);

    // timeline (optional)
    int *timeline = NULL, tl_cap = 0, tl_len = 0;
    if (out_timeline && out_tl_len) {
        tl_cap = 2048;
        timeline = (int*)malloc(sizeof(int)*tl_cap);
    }

    int finished = 0, running_idx = -1;
    int rr_budget = (alg == SCHED_RR ? quantum : 0);

    // main loop
    while (finished < nprocs) {
        // grow timeline if asked for
        if (timeline && tl_len >= tl_cap) {
            tl_cap *= 2;
            timeline = (int*)realloc(timeline, sizeof(int)*tl_cap);
        }

        admit_arrivals(procs, nprocs, &rq, G_NOW);

        int chosen = -1;
        switch (alg) {
            case SCHED_NONE:     /* fallthrough, treat as FCFS */
            case SCHED_FCFS:     chosen = pick_next_fcfs(&rq, procs, running_idx); break;
            case SCHED_SJF:      chosen = pick_next_sjf (&rq, procs, running_idx); break;
            case SCHED_RR:       chosen = pick_next_rr  (&rq, procs, running_idx, &rr_budget, quantum); break;
            case SCHED_PRIORITY: chosen = pick_next_priority(&rq, procs, running_idx); break;
            default:             chosen = -1; break;
        }

        if (timeline) timeline[tl_len++] = chosen;

        if (chosen < 0) {
            inc_waiting_all_except(&rq, procs, -1); // idle tick
            G_NOW++;
            continue;
        }

        inc_waiting_all_except(&rq, procs, chosen);

        // grant 1 tick and wait for completion
        gate_post(&procs[chosen].run_gate);
        gate_wait(&G_TICK_DONE);

        // completion check and state updates under rq.mu (rq is a stack var → use dot)
        pthread_mutex_lock(&rq.mu);
        int now_remaining = procs[chosen].remaining;
        bool was_done = procs[chosen].done;

        if (now_remaining == 0 && !was_done) {
            procs[chosen].done = true;
            procs[chosen].finish_time = G_NOW + 1;
            finished++;
            running_idx = -1;
            if (alg == SCHED_RR) rr_budget = quantum;
            pthread_mutex_unlock(&rq.mu);

            // wake worker once more so it can observe done=true and exit
            gate_post(&procs[chosen].run_gate);
        } else {
            running_idx = chosen;
            if (alg == SCHED_RR) rr_budget--;
            pthread_mutex_unlock(&rq.mu);
        }

        G_NOW++;
    }

    // join & cleanup
    for (int i=0;i<nprocs;++i) pthread_join(ths[i], NULL);
    free(ths);
    for (int i=0;i<nprocs;++i) gate_destroy(&procs[i].run_gate);
    gate_destroy(&G_TICK_DONE);
    rq_destroy(&rq);

    if (out_timeline && out_tl_len) {
        *out_timeline = timeline;
        *out_tl_len = tl_len;
    } else if (timeline) {
        free(timeline);
    }

    return G_NOW; // makespan
}
