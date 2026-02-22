#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>

#define STUDENT_THREADS 6
#define SUBMISSIONS_PER_STUDENT 100000
#define BUFFER_SIZE 4

// SHARED RESOURCES
volatile int exam_database = 0;
volatile int exam_buffer[BUFFER_SIZE];
volatile int buffer_index = 0;

// Deadlock Resources
pthread_mutex_t resourceA;
pthread_mutex_t resourceB;

// Metrics Collection
volatile int buffer_overwrites = 0;
volatile int buffer_underflows = 0;
volatile int context_switches = 0;
volatile int starvation_events = 0;

// SWAPPING METRICS
volatile int total_swaps = 0;
double swap_penalty = 0;
double scheduling_latency = 0;

int db_before[STUDENT_THREADS];
int db_after[STUDENT_THREADS];


double measure_hardware_swap()
{
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);

    for (int i = 0; i < 5; i++)
        fwrite(buffer, 1, sizeof(buffer), fp);

    fflush(fp);
    fclose(fp);

    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("disk_test.bin");

    double disk_speed =
        5.0 / ((e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9);

    double mem_usage = 10.0;

    FILE *fmem = fopen("/proc/self/status", "r");
    if (fmem)
    {
        char line[256];
        long kb = 0;

        while (fgets(line, sizeof(line), fmem))
            if (sscanf(line, "VmRSS: %ld kB", &kb) == 1)
                break;

        fclose(fmem);
        mem_usage = kb / 1024.0;
    }

    clock_gettime(CLOCK_MONOTONIC, &s);

    fp = fopen("lat.txt", "w");
    if (fp)
    {
        fputc('A', fp);
        fclose(fp);
    }

    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");

    double latency =
        (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;

    return 2 * (latency + (mem_usage / disk_speed));
}

void* StudentProducer(void* arg)
{
    int id = *(int*)arg;

    db_before[id] = exam_database;

    for (int i = 0; i < SUBMISSIONS_PER_STUDENT; i++)
    {
        // PRODUCER CONSUMER PROBLEM
        if (buffer_index >= BUFFER_SIZE)
        {
            buffer_overwrites++;
            buffer_index = 0;
        }

        exam_buffer[buffer_index] = id;

        // RACE CONDITION
        int temp = exam_database;

        if (i % 1000 == 0)
        {
            context_switches++;
            total_swaps++;        
            usleep(0);
        }

        exam_database = temp + 1;

        buffer_index++;
    }

    db_after[id] = exam_database;

    return NULL;
}

void* DatabaseConsumer(void* arg)
{
    int total = STUDENT_THREADS * SUBMISSIONS_PER_STUDENT;

    for (int i = 0; i < total; i++)
    {
        if (buffer_index <= 0)
        {
            buffer_underflows++;
        }
        else
        {
            buffer_index--;
        }
    }

    return NULL;
}

void* DeadlockProcess1(void* arg)
{
    pthread_mutex_lock(&resourceA);
    usleep(100);

    pthread_mutex_lock(&resourceB); 
    pthread_mutex_unlock(&resourceB);
    pthread_mutex_unlock(&resourceA);

    return NULL;
}

void* DeadlockProcess2(void* arg)
{
    pthread_mutex_lock(&resourceB);
    usleep(100);

    pthread_mutex_lock(&resourceA);
    pthread_mutex_unlock(&resourceA);
    pthread_mutex_unlock(&resourceB);

    return NULL;
}

void* StarvationThread(void* arg)
{
    for (int i = 0; i < 5; i++)
    {
        starvation_events++;
        sleep(1);
    }

    return NULL;
}

double get_cpu_usage()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    return (usage.ru_utime.tv_sec +
           usage.ru_utime.tv_usec / 1e6);
}

int main()
{
    pthread_t producers[STUDENT_THREADS];
    pthread_t consumer, d1, d2, starve;

    int ids[STUDENT_THREADS];

    struct timespec start, end, ls, le;

    pthread_mutex_init(&resourceA, NULL);
    pthread_mutex_init(&resourceB, NULL);

    printf("\nCampusConnect: Process Sync Failure \n");

    swap_penalty = measure_hardware_swap();

    clock_gettime(CLOCK_MONOTONIC, &start);

    clock_gettime(CLOCK_MONOTONIC, &ls);

    pthread_create(&consumer, NULL, DatabaseConsumer, NULL);

    for (int i = 0; i < STUDENT_THREADS; i++)
    {
        ids[i] = i;
        pthread_create(&producers[i], NULL, StudentProducer, &ids[i]);
    }

    pthread_create(&d1, NULL, DeadlockProcess1, NULL);
    pthread_create(&d2, NULL, DeadlockProcess2, NULL);

    pthread_create(&starve, NULL, StarvationThread, NULL);

    for (int i = 0; i < STUDENT_THREADS; i++)
        pthread_join(producers[i], NULL);

    pthread_join(consumer, NULL);

    clock_gettime(CLOCK_MONOTONIC, &le);

    scheduling_latency =
        (le.tv_sec - ls.tv_sec) +
        (le.tv_nsec - ls.tv_nsec) / 1e9;

    clock_gettime(CLOCK_MONOTONIC, &end);

    double exec_time =
        (end.tv_sec - start.tv_sec) +
        (end.tv_nsec - start.tv_nsec) / 1e9;

    long expected = (long)STUDENT_THREADS * SUBMISSIONS_PER_STUDENT;

    printf("\n=====================================================\n");
    printf("        CAMPUSCONNECT CONCURRENCY FAILURE REPORT      \n");
    printf("=====================================================\n");

    printf("\n[ DATA INTEGRITY MATRIX ]\n");
    printf("+---------+-----------+-----------+\n");
    printf("| Student | Before DB | After DB  |\n");
    printf("+---------+-----------+-----------+\n");

    for (int i = 0; i < STUDENT_THREADS; i++)
    {
        printf("|   %d     | %9d | %9d |\n",
            i + 1, db_before[i], db_after[i]);
    }

    printf("+---------+-----------+-----------+\n");

    printf("\nExpected Submissions      : %ld\n", expected);
    printf("Actual Recorded           : %d\n", exam_database);
    printf("Lost Records (Race)       : %ld\n", expected - exam_database);
    printf("Data Integrity Rate       : %.2f%%\n",
        ((double)exam_database / expected) * 100);

    printf("\n[ PRODUCER-CONSUMER MATRIX ]\n");
    printf("Buffer Size               : %d slots\n", BUFFER_SIZE);
    printf("Buffer Overwrites         : %d\n", buffer_overwrites);
    printf("Buffer Underflows         : %d\n", buffer_underflows);

    printf("\n[ SWAPPING MATRIX ]\n");
    printf("Swap Penalty (Measured)   : %.6f units\n", swap_penalty);
    printf("Total Swap Events         : %d\n", total_swaps);
    printf("Total Swap Overhead       : %.6f units\n",
           total_swaps * swap_penalty);

    printf("\n[ SCHEDULING & STARVATION ]\n");
    printf("Scheduling Latency        : %.6f sec\n", scheduling_latency);
    printf("Forced Context Switches   : %d\n", context_switches);
    printf("Starvation Events         : %d\n", starvation_events);

    printf("\n[ PERFORMANCE METRICS ]\n");
    printf("Total Execution Time      : %.6f seconds\n", exec_time);
    printf("CPU Time Used             : %.6f seconds\n", get_cpu_usage());
    printf("Throughput                : %.2f ops/sec\n",
           expected / exec_time);

    printf("\n[ PROBLEMS ]\n");
    printf("Race Condition            : YES\n");
    printf("Critical Section          : Violated\n");
    printf("Producer-Consumer         : YES\n");
    printf("Deadlock                  : YES\n");
    printf("Starvation                : YES\n");
    printf("Data Inconsistency        : YES\n");

    printf("=====================================================\n");

    return 0;
}
