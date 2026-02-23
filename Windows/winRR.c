#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct process {
    int pid;
    double at, bt, rt_bt; 
    double st, ft, wt, tat, rt;
    int started; 
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
    int tq = 4; // Fixed Time Quantum
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect Round Robin Scheduler (Windows)\n");
    // STATIC DATA FROM YOUR LINUX RR OUTPUT
    int static_pids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    double static_at[] = {1.0, 5.0, 1.0, 5.0, 1.0, 4.0, 4.0, 5.0, 3.0, 4.0};
    double static_bt[] = {7.0, 5.0, 4.0, 3.0, 3.0, 3.0, 7.0, 8.0, 3.0, 7.0};

    for (int i = 0; i < n; i++) {
        p[i].pid = static_pids[i];
        p[i].at = static_at[i];
        p[i].bt = static_bt[i];
        p[i].rt_bt = p[i].bt;
        p[i].started = 0;
    }

    double swap_time = measure_hardware_swap();
    double start_execution = get_time_seconds();

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    double sched_latency_total = 0.0;
    double max_finish_time = 0;

    int completed = 0;
    int queue[2000]; // Ready Queue
    int front = 0, rear = 0;

    printf("\nStep-by-Step Execution (Time Quantum = %d):\n", tq);
    printf("============================================\n");

    // Initial push of arrived processes
    for (int i = 0; i < n; i++) {
        if (p[i].at <= current_time)
            queue[rear++] = i;
    }

    while (completed < n) {
        if (front == rear) { // Advance time if queue is empty
            current_time += 1.0;
            for (int i = 0; i < n; i++) {
                if (p[i].at <= current_time && p[i].rt_bt > 0) {
                    int already = 0;
                    for (int j = front; j < rear; j++) if (queue[j] == i) already = 1;
                    if (!already) queue[rear++] = i;
                }
            }
            continue;
        }

        int idx = queue[front++];
        double l_start = get_time_seconds();

        if (!p[idx].started) {
            p[idx].st = (current_time > p[idx].at) ? current_time : p[idx].at;
            if (current_time < p[idx].at) current_time = p[idx].at;
            p[idx].rt = current_time - p[idx].at;
            p[idx].started = 1;
        }

        double slice = (p[idx].rt_bt > tq) ? tq : p[idx].rt_bt;

        // Logic: Swap Metric
        if ((current_time - p[idx].at) > 5) {
            current_time += swap_time;
            total_swaps++;
        }

        printf("Time %.2f: PID %d runs for %.2f units\n", current_time, p[idx].pid, slice);
        current_time += slice;
        p[idx].rt_bt -= slice;

        double l_end = get_time_seconds();
        sched_latency_total += (l_end - l_start);

        // Push new arrivals
        for (int i = 0; i < n; i++) {
            if (p[i].at <= current_time && p[i].rt_bt > 0 && i != idx) {
                int already = 0;
                for (int j = front; j < rear; j++) if (queue[j] == i) { already = 1; break; }
                if (!already) queue[rear++] = i;
            }
        }

        if (p[idx].rt_bt > 0) {
            queue[rear++] = idx; // Re-queue
        } else {
            p[idx].ft = current_time;
            if (p[idx].ft > max_finish_time) max_finish_time = p[idx].ft;
            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;
            
            total_wt += p[idx].wt;
            total_tat += p[idx].tat;
            total_rt += p[idx].rt;
            total_bt += p[idx].bt;
            completed++;

            if (p[idx].wt > max_wt) max_wt = p[idx].wt;
            if (p[idx].wt < min_wt) min_wt = p[idx].wt;
            if (p[idx].tat > max_tat) max_tat = p[idx].tat;
            if (p[idx].tat < min_tat) min_tat = p[idx].tat;
        }
    }

    printf("============================================\n");
    double end_execution = get_time_seconds();
    double exec_time = end_execution - start_execution;

    // ================= FINAL TABLE =================
    printf("\n+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| PID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    for (int i = 0; i < n; i++) {
        printf("| %-4d | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[i].pid, p[i].at, p[i].bt, p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.2f units\n", max_wt);
    printf("Minimum Waiting Time       : %.2f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.2f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.2f units\n", min_tat);
    printf("Throughput                 : %.4f processes/unit\n", (double)n / max_finish_time);
    printf("CPU Utilization            : %.2f%%\n", (total_bt / max_finish_time) * 100);

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