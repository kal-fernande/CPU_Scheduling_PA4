// csv_loader.c — simple loader for: pid,arrival,burst,priority
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_wiring.h"   // for proc_t

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n' || s[n-1]=='\r' || s[n-1]==' ' || s[n-1]=='\t')) s[--n] = 0;
    while (*s && (*s==' ' || *s=='\t')) memmove(s, s+1, strlen(s));
}

int load_csv(const char *path, proc_t **out_procs, int *out_nprocs) {
    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen"); return -1; }

    char line[256]; int cnt=0;
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (line[0]=='#' || (int)strlen(line)<3) continue;
        cnt++;
    }
    if (cnt == 0) {
        fclose(f);
        *out_procs = NULL; *out_nprocs = 0;
        return 0;
    }

    rewind(f);
    proc_t *A = (proc_t*)calloc(cnt, sizeof(proc_t));
    if (!A) { fclose(f); return -1; }

    int i = 0;
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (line[0]=='#' || (int)strlen(line)<3) continue;

        char pid[32]; int arr, bur, prio;
        if (sscanf(line, " %31[^,] , %d , %d , %d", pid, &arr, &bur, &prio) != 4) {
            fprintf(stderr, "Bad CSV row: %s\n", line);
            free(A); fclose(f); return -1;
        }

        snprintf(A[i].pid, sizeof(A[i].pid), "%s", pid);
        A[i].arrival = arr;
        A[i].burst   = bur;
        A[i].priority= prio;

        A[i].remaining     = bur;
        A[i].started_time  = -1;
        A[i].finish_time   = -1;
        A[i].response_time = -1;
        A[i].waiting_time  = 0;
        A[i].admitted      = false;
        A[i].done          = false;
        i++;
    }
    fclose(f);

    *out_procs = A;
    *out_nprocs = cnt;
    return 0;
}