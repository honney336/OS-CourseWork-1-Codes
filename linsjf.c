#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct process {
    int pid;
    double at, bt, st, ft, wt, tat, rt;
    int completed;
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
    double disk_speed = 5.0 / ((e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9);
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
    if (fp) { fputc('A', fp); fclose(fp); }
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");
    double latency = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    return 2 * (latency + (mem_usage / disk_speed));
}

int main() {
    int n, choice;
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect SJF Scheduler (Linux)\n");
    printf("1. Manual Input\n2. Automated Input\nEnter choice: ");
    scanf("%d", &choice);
    printf("Enter number of processes: ");
    scanf("%d", &n);

    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        p[i].pid = i + 1;
        p[i].completed = 0;
        if (choice == 1) {
            printf("Enter AT and BT for P%d: ", p[i].pid);
            scanf("%lf %lf", &p[i].at, &p[i].bt);
        } else {
            p[i].at = (rand() % 5) + 1;
            p[i].bt = (rand() % 8) + 2;
        }
    }

    double swap_time = measure_hardware_swap();
    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    double sched_latency_total = 0.0;
    double max_ft = 0;

    printf("\n+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| PID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        int idx = -1;
        double min_bt = 1e9;

        
        for (int j = 0; j < n; j++) {
            if (!p[j].completed && p[j].at <= current_time) {
                if (p[j].bt < min_bt) {
                    min_bt = p[j].bt;
                    idx = j;
                }
            }
        }

       if (idx == -1) {
            double earliest = 1e9;
            for (int j = 0; j < n; j++) {
                if (!p[j].completed && p[j].at < earliest) earliest = p[j].at;
            }
            current_time = earliest;
            i--; // Repeat this iteration
            continue;
        }

        struct timespec ls, le;
        clock_gettime(CLOCK_MONOTONIC, &ls);

        if ((current_time - p[idx].at) > 5) {
            current_time += swap_time;
            total_swaps++;
        }

        p[idx].st = current_time;
        p[idx].rt = p[idx].st - p[idx].at;
        current_time += p[idx].bt;
        p[idx].ft = current_time;
        p[idx].tat = p[idx].ft - p[idx].at;
        p[idx].wt = p[idx].tat - p[idx].bt;
        p[idx].completed = 1;
        if(p[idx].ft > max_ft) max_ft = p[idx].ft;

        clock_gettime(CLOCK_MONOTONIC, &le);
        sched_latency_total += (le.tv_sec - ls.tv_sec) + (le.tv_nsec - ls.tv_nsec) / 1e9;

        total_wt += p[idx].wt;
        total_tat += p[idx].tat;
        total_rt += p[idx].rt;
        total_bt += p[idx].bt;

        if (p[idx].wt > max_wt) max_wt = p[idx].wt;
        if (p[idx].wt < min_wt) min_wt = p[idx].wt;
        if (p[idx].tat > max_tat) max_tat = p[idx].tat;
        if (p[idx].tat < min_tat) min_tat = p[idx].tat;

        printf("| %-4d | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[idx].pid, p[idx].at, p[idx].bt, p[idx].st, p[idx].ft,
               p[idx].wt, p[idx].tat, p[idx].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double exec_time = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_nsec - start_t.tv_nsec) / 1e9;

    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.2f units\n", max_wt);
    printf("Minimum Waiting Time       : %.2f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.2f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.2f units\n", min_tat);
    printf("Throughput                 : %.4f processes/unit\n", (double)n / max_ft);
    printf("CPU Utilization            : %.2f%%\n", (total_bt / max_ft) * 100);

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
    printf("Worst-Case Latency         : %.2f units\n", max_wt);

    return 0;
}