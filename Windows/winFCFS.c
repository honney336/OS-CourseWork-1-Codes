#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct process {
    int pid;
    double at, bt, st, ft, wt, tat, rt;
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

    // Windows Memory Usage Approximation
    double mem_usage = 10.0;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        mem_usage = (double)statex.ullTotalPhys / (1024 * 1024 * 1024); // GB for scaling
    }

    double lat_start = get_time_seconds();
    fp = fopen("lat.txt", "w");
    if (fp) {
        fputc('A', fp);
        fclose(fp);
    }
    double lat_end = get_time_seconds();
    remove("lat.txt");

    double latency = lat_end - lat_start;

    return 2 * (latency + (mem_usage / disk_speed));
}

int main() {
    int n = 10;
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect FCFS Scheduler (Windows)\n");

    int static_pids[] = {3, 6, 8, 7, 1, 4, 5, 10, 2, 9};
    double static_at[] = {1.0, 2.0, 2.0, 3.0, 4.0, 4.0, 4.0, 4.0, 5.0, 5.0};
    double static_bt[] = {8.0, 6.0, 8.0, 8.0, 2.0, 9.0, 3.0, 7.0, 9.0, 7.0};

    for (int i = 0; i < n; i++) {
        p[i].pid = static_pids[i];
        p[i].at = static_at[i];
        p[i].bt = static_bt[i];
    }

    // FCFS sorting (based on Arrival Time)
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p[j].at > p[j + 1].at) {
                struct process tmp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = tmp;
            }
        }
    }

    double swap_time = measure_hardware_swap();
    double start_execution = get_time_seconds();

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    double sched_latency_total = 0.0;

    printf("\n+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| PID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        double l_start = get_time_seconds();

        if (current_time < p[i].at) current_time = p[i].at;

        // Logic Check: Swap Metric
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

        double l_end = get_time_seconds();
        sched_latency_total += (l_end - l_start);

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

    double end_execution = get_time_seconds();
    double exec_time = end_execution - start_execution;

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
    printf("Total Swapped Processes    : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.6f seconds\n", exec_time);
    printf("Scheduling Latency         : %.6f seconds (avg)\n", sched_latency_total / n);
    printf("Average Process Latency    : %.2f units\n", total_wt / n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.0f units\n", max_wt);

    return 0;
}
