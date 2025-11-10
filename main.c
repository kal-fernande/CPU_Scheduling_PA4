// main.c — minimal driver using your CLI + our scheduler wiring
#include <stdio.h>
#include <stdlib.h>
#include "cmdparser.h"         // your CLI: parse_arguments, print_usage, scheduler_t
#include "scheduler_wiring.h"  // run_scheduler(...) + proc_t typedef
#include "metrics.h"
#include <string.h>

// If you already have a CSV loader, declare it here and link it.
// Expected signature:
int load_csv(const char *path, proc_t **out_procs, int *out_nprocs);

// Initialize fields expected by the scheduler core
static void init_proc_fields(proc_t *procs, int n) {
    for (int i = 0; i < n; ++i) {
        procs[i].remaining     = procs[i].burst;
        procs[i].started_time  = -1;
        procs[i].finish_time   = -1;
        procs[i].response_time = 0;
        procs[i].waiting_time  = 0;
        procs[i].admitted      = false;
        procs[i].done          = false;
        // run_sem is initialized in proc_sems_init() inside scheduler_wiring.c
    }
}

int main(int argc, char **argv) {
    // 1) Parse CLI
    cmd_options_t opts = parse_arguments(argc, argv);
    if (opts.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    // 2) Load processes (pid, arrival, burst, priority) -> procs[], nprocs
    proc_t *procs = NULL; 
    int nprocs = 0;
    if (load_csv(opts.input_file, &procs, &nprocs) != 0) {
        fprintf(stderr, "Failed to load input: %s\n", opts.input_file);
        return 1;
    }
    if (nprocs == 0) {
        fprintf(stderr, "No processes found in %s\n", opts.input_file);
        free(procs);
        return 1;
    }

    init_proc_fields(procs, nprocs);

    // 3) Run the scheduler loop (spawns threads, uses semaphores, returns makespan/timeline)
    int *timeline = NULL, tl_len = 0;
    int makespan = run_scheduler(procs, nprocs, opts.scheduler, opts.quantum, &timeline, &tl_len);

    // 4) Metrics & output
    const char *algname =
        (opts.scheduler == SCHED_FCFS)     ? "FCFS" :
        (opts.scheduler == SCHED_SJF)      ? "SJF" :
        (opts.scheduler == SCHED_RR)       ? "RR" :
        (opts.scheduler == SCHED_PRIORITY) ? "PRIORITY" : "UNKNOWN";
    printf("\n===== %s Scheduling =====\n", algname);
    printf("Finished in %d ticks. Processes: %d\n", makespan, nprocs);

    (void)compute_and_print_metrics(procs, nprocs, makespan, timeline, tl_len);
    
    // 5) Cleanup
    free(timeline);
    free(procs);
    return 0;
}