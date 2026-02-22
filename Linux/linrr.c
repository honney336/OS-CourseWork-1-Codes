#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
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

    double disk_speed = 5.0 / ((e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec)/1e9);

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

    double latency = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec)/1e9;

    return 2*(latency + (mem_usage/disk_speed));
}

// ================= ROUND ROBIN SCHEDULER =================
int main() {
    int n, choice, tq;
    struct process p[10];
    int total_swaps = 0;

    printf("CampusConnect Round Robin Scheduler (Linux)\n");
    printf("1. Manual Input\n2. Automated Input\nEnter choice: ");
    scanf("%d", &choice);
    printf("Enter number of processes: ");
    scanf("%d", &n);
    printf("Enter Time Quantum: ");
    scanf("%d", &tq);

    srand(time(NULL));
    for(int i=0;i<n;i++){
        p[i].pid = i+1;
        p[i].started = 0;
        if(choice==1){
            printf("Enter AT and BT for P%d: ", p[i].pid);
            scanf("%lf %lf", &p[i].at, &p[i].bt);
        } else {
            p[i].at = (rand()%5)+1;
            p[i].bt = (rand()%8)+2;
        }
        p[i].rt_bt = p[i].bt; // remaining burst
    }

    double swap_time = measure_hardware_swap();
    struct timespec start_t, end_t;
    clock_gettime(CLOCK_MONOTONIC, &start_t);

    double current_time = 0;
    double total_wt=0, total_tat=0, total_rt=0, total_bt=0;
    double max_wt=0, min_wt=1e9, max_tat=0, min_tat=1e9;
    double sched_latency_total=0.0;
    double max_finish_time = 0;

    int completed=0;
    int queue[1000]; // expanded queue size for safety
    int front=0, rear=0;

    printf("\nStep-by-Step Execution (Time Quantum = %d):\n", tq);
    printf("============================================\n");

    // push arrived processes initially
    for(int i=0;i<n;i++){
        if(p[i].at <= current_time)
            queue[rear++] = i;
    }

    while(completed < n){
        if(front == rear){ // queue empty, advance time
            current_time += 1.0;
            for(int i=0;i<n;i++)
                if(p[i].at <= current_time && p[i].rt_bt>0){
                    int already=0;
                    for(int j=front; j<rear; j++) if(queue[j]==i) already=1;
                    if(!already) queue[rear++] = i;
                }
            continue;
        }

        int idx = queue[front++]; // pop from queue
        struct timespec ls, le;
        clock_gettime(CLOCK_MONOTONIC, &ls);

        if(!p[idx].started){
            p[idx].st = (current_time > p[idx].at) ? current_time : p[idx].at;
            if(current_time < p[idx].at) current_time = p[idx].at;
            p[idx].rt = current_time - p[idx].at;
            p[idx].started = 1;
        }

        double slice = (p[idx].rt_bt > tq) ? tq : p[idx].rt_bt;

        if((current_time - p[idx].at) > 5){ // simulate swap
            current_time += swap_time;
            total_swaps++;
        }

        printf("Time %.2f: PID %d runs for %.2f units\n", current_time, p[idx].pid, slice);
        current_time += slice;
        p[idx].rt_bt -= slice;

        clock_gettime(CLOCK_MONOTONIC, &le);
        sched_latency_total += (le.tv_sec - ls.tv_sec) + (le.tv_nsec - ls.tv_nsec)/1e9;

        // push newly arrived processes to queue
        for(int i=0;i<n;i++){
            if(p[i].at <= current_time && p[i].rt_bt>0 && i != idx){
                int already=0;
                for(int j=front;j<rear;j++)
                    if(queue[j]==i){already=1; break;}
                if(!already) queue[rear++] = i;
            }
        }

        // if process still has remaining burst, add back to queue
        if(p[idx].rt_bt > 0){
            queue[rear++] = idx;
        } else { // finished
            p[idx].ft = current_time;
            if(p[idx].ft > max_finish_time) max_finish_time = p[idx].ft;
            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;
            total_wt += p[idx].wt;
            total_tat += p[idx].tat;
            total_rt += p[idx].rt;
            total_bt += p[idx].bt;
            completed++;
            if(p[idx].wt>max_wt) max_wt = p[idx].wt;
            if(p[idx].wt<min_wt) min_wt = p[idx].wt;
            if(p[idx].tat>max_tat) max_tat = p[idx].tat;
            if(p[idx].tat<min_tat) min_tat = p[idx].tat;
        }
    }

    printf("============================================\n");

    clock_gettime(CLOCK_MONOTONIC, &end_t);
    double exec_time = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_nsec - start_t.tv_nsec)/1e9;

    // ================= FINAL TABLE =================
    printf("\n+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| PID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    for(int i=0;i<n;i++){
        printf("| %-4d | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
            p[i].pid, p[i].at, p[i].bt, p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt/n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat/n);
    printf("Average Response Time      : %.2f units\n", total_rt/n);
    printf("Maximum Waiting Time       : %.2f units\n", max_wt);
    printf("Minimum Waiting Time       : %.2f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.2f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.2f units\n", min_tat);
    printf("Throughput                 : %.4f processes/unit\n", (double)n / max_finish_time);
    printf("CPU Utilization            : %.2f%%\n", (total_bt / max_finish_time) * 100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (per process)    : %.6f units\n", swap_time);
    printf("Total Swapped Processes   : %d\n", total_swaps);
    printf("Total Swapping Overhead   : %.6f units\n", total_swaps*swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.6f seconds\n", exec_time);
    printf("Scheduling Latency         : %.6f seconds (avg)\n", sched_latency_total/n);
    printf("Average Process Latency    : %.2f units\n", total_wt/n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.2f units\n", max_wt);

    return 0;
}