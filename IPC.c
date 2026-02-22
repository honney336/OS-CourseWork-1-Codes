#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#define STUDENTS 10
#define SUBMISSIONS 50000

typedef struct {
    int student_db[STUDENTS];
    int total_records;
    // Real Kernel Metrics
    long vol_ctx_switches[STUDENTS];
    long invol_ctx_switches[STUDENTS];
    long page_faults_major[STUDENTS];
    long page_faults_minor[STUDENTS];
    double start_time[STUDENTS];
    double end_time[STUDENTS];
} SharedData;

double get_time_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

double get_total_cpu_time() {
    struct rusage u;
    getrusage(RUSAGE_CHILDREN, &u); 
    return (u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1e6) +
           (u.ru_stime.tv_sec + u.ru_stime.tv_usec / 1e6);
}

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    key_t key = ftok("/tmp", 65);
    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmid < 0) { perror("shmget"); exit(1); }

    SharedData *data = (SharedData *) shmat(shmid, NULL, 0);
    memset(data, 0, sizeof(SharedData));

    sem_t *sem = sem_open("/campus_sem", O_CREAT, 0666, 1);
    sem_unlink("/campus_sem"); 

    printf("\nCampusConnect: Real-Time Kernel IPC Analysis\n");

    for(int i = 0; i < STUDENTS; i++) {
        data->start_time[i] = get_time_now();
        pid_t pid = fork();

        if(pid < 0) { perror("fork"); exit(1); }

        if(pid == 0) { // Child Process (Student)
            for(int j = 0; j < SUBMISSIONS; j++) {
                sem_wait(sem);
                data->student_db[i]++;
                data->total_records++;
                sem_post(sem);
            }
            
            struct rusage usage;
            getrusage(RUSAGE_SELF, &usage);
            data->page_faults_major[i] = usage.ru_majflt;
            data->page_faults_minor[i] = usage.ru_minflt;
            data->vol_ctx_switches[i] = usage.ru_nvcsw;   
            data->invol_ctx_switches[i] = usage.ru_nivcsw; 
            data->end_time[i] = get_time_now();
            
            shmdt(data);
            exit(0);
        }
    }

    for(int i = 0; i < STUDENTS; i++) wait(NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double wall_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double cpu_time = get_total_cpu_time();
    
    double cpu_util = (cpu_time / (wall_time * num_cores)) * 100.0;
    if (cpu_util > 100.0) cpu_util = 100.0; 

    printf("\n=====================================================\n");
    printf("         CAMPUSCONNECT IPC CONCURRENCY REPORT         \n");
    printf("=====================================================\n");

    printf("\n[ DATA INTEGRITY MATRIX ]\n");
    printf("+---------+--------------+\n");
    printf("| Student | Submissions  |\n");
    printf("+---------+--------------+\n");
    for(int i=0; i<STUDENTS; i++) {
        printf("|   %2d    |   %8d   |\n", i+1, data->student_db[i]);
    }
    printf("+---------+--------------+\n");

    printf("\n[ KERNEL SCHEDULING MATRIX ]\n");
    printf("+---------+------------+------------+-------------+\n");
    printf("| Student | Vol Ctx Sw | Inv Ctx Sw | Duration(s) |\n");
    printf("+---------+------------+------------+-------------+\n");
    for(int i=0; i<STUDENTS; i++) {
        printf("|   %2d    |   %8ld |   %8ld |   %8.6f  |\n", 
               i+1, data->vol_ctx_switches[i], data->invol_ctx_switches[i], 
               data->end_time[i]-data->start_time[i]);
    }
    printf("+---------+------------+------------+-------------+\n");

    printf("\n[ SWAPPING MATRIX (PAGE FAULTS) ]\n");
    printf("+---------+-------------+-------------+\n");
    printf("| Student | Minor (Soft)| Major (Hard)|\n");
    printf("+---------+-------------+-------------+\n");
    for(int i=0; i<STUDENTS; i++) {
        printf("|   %2d    |   %9ld |   %9ld |\n", 
               i+1, data->page_faults_minor[i], data->page_faults_major[i]);
    }
    printf("+---------+-------------+-------------+\n");

    printf("\n[ PERFORMANCE METRICS ]\n");
    printf("Detected CPU Cores    : %ld\n", num_cores);
    printf("Total Wall Time       : %.6f sec\n", wall_time);
    printf("Total CPU Time        : %.6f sec\n", cpu_time);
    printf("System Utilization    : %.2f%% \n", cpu_util);
    printf("Throughput            : %.2f ops/sec\n", (double)data->total_records / wall_time);

    printf("\n[ IPC SYNCHRONIZATION METRICS ]\n");
    printf("Total Submissions     : %d\n", data->total_records);
    printf("Data Integrity Rate   : %.2f%%\n", ((double)data->total_records / (STUDENTS * SUBMISSIONS)) * 100);
    printf("=====================================================\n");

    sem_close(sem);
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}