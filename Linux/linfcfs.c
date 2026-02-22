#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct process {
    int pid;
    double at, bt, st, ft, wt, tat, rt;
};

// ================= REAL OS METRICS FUNCTIONS =================

double measure_hardware_swap() {
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (int i = 0; i < 5; i++) fwrite(buffer, 1, sizeof(buffer), fp);
    fflush(fp);
    fclose(fp);
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("disk_test.bin");

    double disk_speed =
        5.0 / ((e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9);

    double mem_usage = 10.0;
    FILE *fmem = fopen("/proc/self/status", "r");
    if (fmem) {
        char line[256];
        long kb = 0;
        while (fgets(line, sizeof(line), fmem))
            if (sscanf(line, "VmRSS: %ld kB", &kb) == 1) break;
        fclose(fmem);
        mem_usage = kb / 1024.0;
    }

    clock_gettime(CLOCK_MONOTONIC, &s);
    fp = fopen("lat.txt", "w");
    if (fp) {
        fputc('A', fp);
        fclose(fp);
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");

    double latency =
        (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;

    return 2 * (latency + (mem_usage / disk_speed));
}

int main() {
    int n, choice;
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect FCFS Scheduler (Linux)\n");
    printf("1. Manual Input\n2. Automated Input\nEnter choice: ");
    scanf("%d", &choice);
    printf("Enter number of processes: ");
    scanf("%d", &n);

    srand(time(NULL));

    for (int i = 0; i < n; i++) {
        p[i].pid = i + 1;
        if (choice == 1) {
            printf("Enter AT and BT for P%d: ", p[i].pid);
            scanf("%lf %lf", &p[i].at, &p[i].bt);
        } else {
            p[i].at = (rand() % 5) + 1;
            p[i].bt = (rand() % 8) + 2;
        }
    }

    // FCFS sorting
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (p[j].at > p[j + 1].at) {
                struct process tmp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = tmp;
            }

    double swap_time = measure_hardware_swap();

    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;

    /* REAL SCHEDULING LATENCY */
    double sched_latency_total = 0.0;

    printf("\n+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| PID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {

        struct timespec ls, le;
        clock_gettime(CLOCK_MONOTONIC, &ls);

        if (current_time < p[i].at) current_time = p[i].at;

        if ((current_time - p[i].at) > 5) {
            current_time += swap_time;
            total_swaps++;
        }

        p[i].st = current_time;
        p[i].rt = p[i].st - p[i].at;
        current_time += p[i].bt;
        p[i].ft = current_time;
        p[i].tat = p[i].ft - p[i].at;
        p[i].wt = p[i].tat - p[i].bt;

        clock_gettime(CLOCK_MONOTONIC, &le);
        sched_latency_total +=
            (le.tv_sec - ls.tv_sec) +
            (le.tv_nsec - ls.tv_nsec) / 1e9;

        total_wt += p[i].wt;
        total_tat += p[i].tat;
        total_rt += p[i].rt;
        total_bt += p[i].bt;

        if (p[i].wt > max_wt) max_wt = p[i].wt;
        if (p[i].wt < min_wt) min_wt = p[i].wt;
        if (p[i].tat > max_tat) max_tat = p[i].tat;
        if (p[i].tat < min_tat) min_tat = p[i].tat;

        printf("| %-4d | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[i].pid, p[i].at, p[i].bt, p[i].st, p[i].ft,
               p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double exec_time =
        (end_t.tv_sec - start_t.tv_sec) +
        (end_t.tv_nsec - start_t.tv_nsec) / 1e9;

    /* ---- METRICS ---- */

    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.0f units\n", max_wt);
    printf("Minimum Waiting Time       : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.0f units\n", min_tat);
    printf("Throughput                 : %.4f processes/unit\n", (double)n / p[n - 1].ft);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / p[n - 1].ft) * 100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (per process)    : %.6f units\n", swap_time);
    printf("Total Swapped Processes   : %d\n", total_swaps);
    printf("Total Swapping Overhead   : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.6f seconds\n", exec_time);
    printf("Scheduling Latency         : %.6f seconds (avg)\n", sched_latency_total / n);
    printf("Average Process Latency    : %.2f units\n", total_wt / n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.0f units\n", max_wt);

    return 0;
}

