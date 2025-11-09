// main.c — minimal driver using your CLI + our scheduler wiring
#include <stdio.h>
#include <stdlib.h>
#include "cmdparser.h"         // your CLI: parse_arguments, print_usage, scheduler_t
#include "scheduler_wiring.h"  // run_scheduler(...) + proc_t typedef

// If you already have a CSV loader, declare it here and link it.
// Expected signature:
int load_csv(const char *path, proc_t **out_procs, int *out_nprocs);

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

    // 3) Run the scheduler loop (spawns threads, uses semaphores, returns makespan/timeline)
    int *timeline = NULL, tl_len = 0;
    int makespan = run_scheduler(procs, nprocs, opts.scheduler, opts.quantum, &timeline, &tl_len);

    // 4) (Optional) minimal results printing — keep or replace with your friend's output code
    //    All per-process metrics are in procs[i].started_time / finish_time / waiting_time / response_time.
    printf("Finished in %d ticks. Processes: %d\n", makespan, nprocs);
    // for (int i = 0; i < nprocs; ++i) {
    //     int turn = procs[i].finish_time - procs[i].arrival;
    //     printf("%s: start=%d finish=%d wait=%d resp=%d turn=%d\n",
    //         procs[i].pid, procs[i].started_time, procs[i].finish_time,
    //         procs[i].waiting_time, procs[i].response_time, turn);
    // }

    // 5) Cleanup
    free(timeline);
    free(procs);
    return 0;
}