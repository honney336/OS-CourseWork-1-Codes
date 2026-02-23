#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct process {
    int pid;
    double at, bt, st, ft, wt, tat, rt;
    int priority;
    int completed; 
};

// Windows-specific high-resolution timer helper
double get_time_seconds() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
}

// ================= REAL OS METRICS FUNCTIONS (WINDOWS PORT) =================

double measure_hardware_swap() {
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    double start = get_time_seconds();
    for (int i = 0; i < 5; i++) fwrite(buffer, 1, sizeof(buffer), fp);
    fflush(fp);
    fclose(fp);
    double end = get_time_seconds();
    remove("disk_test.bin");

    double disk_speed = 5.0 / (end - start);
    if (disk_speed <= 0) disk_speed = 1.0;

    double mem_usage = 10.0;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        // Proxy for system load using physical memory
        mem_usage = (double)statex.ullTotalPhys / (1024 * 1024 * 1024);
    }

    double lat_start = get_time_seconds();
    fp = fopen("lat.txt", "w");
    if (fp) { fputc('A', fp); fclose(fp); }
    double lat_end = get_time_seconds();
    remove("lat.txt");

    double latency = lat_end - lat_start;
    return 2 * (latency + (mem_usage / disk_speed));
}

int main() {
    int n = 10;
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect Priority Scheduler (Windows) - Highest Priority First\n");

    // STATIC DATA FROM YOUR LINUX OUTPUT
    int static_pids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    double static_at[] = {4.0, 5.0, 5.0, 3.0, 1.0, 4.0, 2.0, 2.0, 5.0, 2.0};
    double static_bt[] = {3.0, 2.0, 9.0, 9.0, 3.0, 7.0, 3.0, 8.0, 4.0, 9.0};
    int static_pri[] = {4, 1, 4, 3, 3, 1, 2, 1, 2, 5};

    for (int i = 0; i < n; i++) {
        p[i].pid = static_pids[i];
        p[i].at = static_at[i];
        p[i].bt = static_bt[i];
        p[i].priority = static_pri[i];
        p[i].completed = 0;
    }

    double swap_time = measure_hardware_swap();
    double start_execution = get_time_seconds();

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    double sched_latency_total = 0.0;
    int completed_count = 0;

    printf("\n+------+-------+-------+----------+----------+----------+----------+----------+----------+\n");
    printf("| PID  |   AT  |   BT  | Priority |  Start   |  Finish  |   WT     |   TAT    |   RT     |\n");
    printf("+------+-------+-------+----------+----------+----------+----------+----------+----------+\n");

    while (completed_count < n) {
        int idx = -1;
        int min_priority = INT_MAX;

        // Logic: Find highest priority (lowest number) among arrived processes
        for (int j = 0; j < n; j++) {
            if (!p[j].completed && p[j].at <= current_time) {
                if (p[j].priority < min_priority) {
                    min_priority = p[j].priority;
                    idx = j;
                }
            }
        }

        if (idx == -1) {
            double next_arrival = 1e9;
            for (int j = 0; j < n; j++) {
                if (!p[j].completed && p[j].at < next_arrival) next_arrival = p[j].at;
            }
            current_time = next_arrival;
            continue;
        }

        double l_start = get_time_seconds();

        // Logic: Swap Metric Simulation
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
        completed_count++;

        double l_end = get_time_seconds();
        sched_latency_total += (l_end - l_start);

        total_wt += p[idx].wt;
        total_tat += p[idx].tat;
        total_rt += p[idx].rt;
        total_bt += p[idx].bt;

        if (p[idx].wt > max_wt) max_wt = p[idx].wt;
        if (p[idx].wt < min_wt) min_wt = p[idx].wt;
        if (p[idx].tat > max_tat) max_tat = p[idx].tat;
        if (p[idx].tat < min_tat) min_tat = p[idx].tat;

        printf("| %-4d | %-5.1f | %-5.1f | %-8d | %-8.2f | %-8.2f | %-8.2f | %-8.2f | %-8.2f |\n",
               p[idx].pid, p[idx].at, p[idx].bt, p[idx].priority, p[idx].st, p[idx].ft,
               p[idx].wt, p[idx].tat, p[idx].rt);
    }

    printf("+------+-------+-------+----------+----------+----------+----------+----------+----------+\n");

    double end_execution = get_time_seconds();
    double exec_time = end_execution - start_execution;

    double max_ft = 0;
    for (int i = 0; i < n; i++) if (p[i].ft > max_ft) max_ft = p[i].ft;

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
    printf("Total Swapped Processes    : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.6f seconds\n", exec_time);
    printf("Scheduling Latency         : %.6f seconds (avg)\n", sched_latency_total / n);
    printf("Average Process Latency    : %.2f units\n", total_wt / n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.2f units\n", max_wt);

    return 0;
}