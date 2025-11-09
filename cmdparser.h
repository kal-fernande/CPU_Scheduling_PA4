#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <stdbool.h>

// Enumeration for supported scheduling algorithms
typedef enum {
    SCHED_NONE,
    SCHED_FCFS,
    SCHED_SJF,
    SCHED_RR,
    SCHED_PRIORITY
} scheduler_t;

// Structure holding parsed command-line options
typedef struct {
    scheduler_t scheduler;
    char input_file[256];
    int quantum;
    bool show_help;
} cmd_options_t;

// Function prototypes
cmd_options_t parse_arguments(int argc, char *argv[]);
void print_usage(const char *prog_name);

#endif