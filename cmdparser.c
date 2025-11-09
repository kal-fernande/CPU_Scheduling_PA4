#include "cmdparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

cmd_options_t parse_arguments(int argc, char *argv[]) {
    cmd_options_t opts = {
        .scheduler = SCHED_NONE,
        .quantum = 0,
        .show_help = false
    };

    static struct option long_opts[] = {
        {"fcfs",     no_argument,       0, 'f'},
        {"sjf",      no_argument,       0, 's'},
        {"rr",       no_argument,       0, 'r'},
        {"priority", no_argument,       0, 'p'},
        {"input",    required_argument, 0, 'i'},
        {"quantum",  required_argument, 0, 'q'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int opt_index = 0;

    while ((opt = getopt_long(argc, argv, "fsrpi:q:h", long_opts, &opt_index)) != -1) {
        switch (opt) {
            case 'f': opts.scheduler = SCHED_FCFS; break;
            case 's': opts.scheduler = SCHED_SJF; break;
            case 'r': opts.scheduler = SCHED_RR; break;
            case 'p': opts.scheduler = SCHED_PRIORITY; break;
            case 'i': strncpy(opts.input_file, optarg, sizeof(opts.input_file) - 1); break;
            case 'q': opts.quantum = atoi(optarg); break;
            case 'h': opts.show_help = true; break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Validation
    if (!opts.show_help) {
        if (opts.scheduler == SCHED_NONE) {
            fprintf(stderr, "Error: must specify a scheduling algorithm (--fcfs, --sjf, --rr, or --priority)\n");
            exit(EXIT_FAILURE);
        }
        if (strlen(opts.input_file) == 0) {
            fprintf(stderr, "Error: must specify an input file with -i or --input <file>\n");
            exit(EXIT_FAILURE);
        }
        if (opts.scheduler == SCHED_RR && opts.quantum <= 0) {
            fprintf(stderr, "Error: Round Robin requires a valid time quantum (--quantum <n>)\n");
            exit(EXIT_FAILURE);
        }
    }

    return opts;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("  -f, --fcfs           Use First Come First Served scheduling\n");
    printf("  -s, --sjf            Use Shortest Job First scheduling\n");
    printf("  -r, --rr             Use Round Robin scheduling (requires --quantum)\n");
    printf("  -p, --priority       Use Priority scheduling\n");
    printf("  -i, --input <file>   Input CSV workload file\n");
    printf("  -q, --quantum <n>    Time quantum for Round Robin\n");
    printf("  -h, --help           Show this help message\n\n");
}
