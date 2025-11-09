// scheduler_wiring.c
#include "scheduler_wiring.h"
#include <stdint.h>
#include <stdlib.h>
#ifdef SCHED_RR
#undef SCHED_RR
#endif
#ifdef SCHED_FIFO
#undef SCHED_FIFO
#endif
#ifdef SCHED_OTHER
#undef SCHED_OTHER
#endif

// Globals for worker access (kept internal to this module)
static proc_t *G_PROCS = NULL;
static int     G_NOW   = 0;
static sem_t   G_TICK_DONE; // worker posts at end of its 1-tick slice

static void *worker(void *arg) {
    int idx = (int)(intptr_t)arg;
    proc_t *p = &G_PROCS[idx];
    for (;;) {
        sem_wait(&p->run_sem);
        if (p->done) break;
        if (p->started_time < 0) { p->started_time = G_NOW; p->response_time = G_NOW - p->arrival; }
        p->remaining -= 1; // consume exactly one CPU tick
        sem_post(&G_TICK_DONE); // notify scheduler
    }
    return NULL;
}

int run_scheduler(proc_t *procs, int nprocs, scheduler_t alg, int quantum,
                  int **out_timeline, int *out_tl_len) {
    // init globals and barrier
    G_PROCS = procs; G_NOW = 0;
    sem_init(&G_TICK_DONE, 0, 0);

    // ready queue and semaphores
    readyq_t rq; rq_init(&rq, nprocs * 2);
    proc_sems_init(procs, nprocs);

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
        sem_post(&procs[chosen].run_sem);
        sem_wait(&G_TICK_DONE);

        // completion check
        if (procs[chosen].remaining == 0 && !procs[chosen].done) {
            procs[chosen].done = true;
            procs[chosen].finish_time = G_NOW + 1;
            finished++;
            running_idx = -1;
            if (alg == SCHED_RR) rr_budget = quantum;
            sem_post(&procs[chosen].run_sem); // let worker exit
        } else {
            running_idx = chosen;
            if (alg == SCHED_RR) rr_budget--;
        }

        G_NOW++;
    }

    // join & cleanup
    for (int i=0;i<nprocs;++i) pthread_join(ths[i], NULL);
    free(ths);
    proc_sems_destroy(procs, nprocs);
    sem_destroy(&G_TICK_DONE);
    rq_destroy(&rq);

    if (out_timeline && out_tl_len) {
        *out_timeline = timeline;
        *out_tl_len = tl_len;
    } else if (timeline) {
        free(timeline);
    }

    return G_NOW; // makespan
}
