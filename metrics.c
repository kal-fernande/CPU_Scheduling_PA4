#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metrics.h"

static void print_gantt(const int *tl, int n, proc_t *procs) {
    if (!tl || n <= 0) return;

    // Compress consecutive slots into segments [start,end) of the same proc
    printf("\nTimeline (Gantt):\n");
    int start = 0, cur = tl[0];
    for (int t = 1; t <= n; ++t) {
        if (t == n || tl[t] != cur) {
            // print segment [start, t) labeled by PID or IDLE
            if (cur < 0) {
                printf("[%4d..%4d): IDLE\n", start, t);
            } else {
                printf("[%4d..%4d): %s\n", start, t, procs[cur].pid);
            }
            if (t < n) { start = t; cur = tl[t]; }
        }
    }
    puts("");
}

metrics_t compute_and_print_metrics(proc_t *procs, int nprocs, int makespan, const int *timeline, int tl_len) {
    metrics_t M = {0};
    long sum_wait = 0, sum_resp = 0, sum_turn = 0;

    // CPU busy ticks from timeline (if present), else infer from bursts
    long cpu_busy = 0;
    if (timeline && tl_len > 0) {
        for (int i = 0; i < tl_len; ++i) {
            if (timeline[i] >= 0){
                cpu_busy++;
            }
        }
    } else {
        // fallback: sum original bursts
        for (int i = 0; i < nprocs; ++i){
            cpu_busy += procs[i].burst;
        }
    }

    // Gantt
    print_gantt(timeline, tl_len, procs);

    // Table header
    printf("-------------------------------------\n");
    printf("PID       Arr  Burst  Start  Finish  Wait  Resp  Turn\n");

    for (int i = 0; i < nprocs; ++i) {
        int turn = procs[i].finish_time - procs[i].arrival;
        sum_wait += procs[i].waiting_time;
        sum_resp += procs[i].response_time;
        sum_turn += turn;

        printf("%-8s %4d  %5d  %5d  %6d  %4d  %4d  %4d\n",
               procs[i].pid,
               procs[i].arrival,
               procs[i].burst,
               procs[i].started_time,
               procs[i].finish_time,
               procs[i].waiting_time,
               procs[i].response_time,
               turn);
    }
    printf("-------------------------------------\n");

    // Globals
    M.avg_wait = (double)sum_wait / nprocs;
    M.avg_resp = (double)sum_resp / nprocs;
    M.avg_turn = (double)sum_turn / nprocs;
    M.throughput = (makespan > 0) ? ((double)nprocs / (double)makespan) : 0.0;
    M.cpu_utilization = (makespan > 0) ? (100.0 * (double)cpu_busy / (double)makespan) : 0.0;

    printf("Avg Wait = %.2f\n", M.avg_wait);
    printf("Avg Resp = %.2f\n", M.avg_resp);
    printf("Avg Turn = %.2f\n", M.avg_turn);
    printf("Throughput = %.3f jobs/unit time\n", M.throughput);
    printf("CPU Utilization = %.1f%%\n", M.cpu_utilization);

    return M;
}