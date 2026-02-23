#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STUDENT_THREADS 6
#define SUBMISSIONS_PER_STUDENT 100000
#define BUFFER_SIZE 4

// SHARED RESOURCES
volatile LONG exam_database = 0;
volatile int exam_buffer[BUFFER_SIZE];
volatile LONG buffer_index = 0;

// DEADLOCK RESOURCES
CRITICAL_SECTION resourceA;
CRITICAL_SECTION resourceB;

// METRICS
volatile LONG buffer_overwrites = 0;
volatile LONG buffer_underflows = 0;
volatile LONG context_switches = 0;
volatile LONG starvation_events = 0;
volatile LONG total_swaps = 0;

double swap_penalty = 0;
double scheduling_latency = 0;

int db_before[STUDENT_THREADS];
int db_after[STUDENT_THREADS];

double now()
{
    LARGE_INTEGER freq, t;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / freq.QuadPart;
}

double measure_hardware_swap()
{
    double start = now();
    FILE* f = fopen("disk_test.bin", "wb");
    char buf[1024 * 1024] = { 0 };

    for (int i = 0; i < 5; i++)
        fwrite(buf, 1, sizeof(buf), f);

    fclose(f);
    remove("disk_test.bin");

    double latency = now() - start;
    return latency * 2;
}

DWORD WINAPI StudentProducer(LPVOID arg)
{
    int id = *(int*)arg;
    db_before[id] = exam_database;

    for (int i = 0; i < SUBMISSIONS_PER_STUDENT; i++)
    {
        if (buffer_index >= BUFFER_SIZE)
        {
            InterlockedIncrement(&buffer_overwrites);
            buffer_index = 0;
        }

        exam_buffer[buffer_index] = id;

        int temp = exam_database;

        if (i % 1000 == 0)
        {
            InterlockedIncrement(&context_switches);
            InterlockedIncrement(&total_swaps);
            Sleep(0);
        }

        exam_database = temp + 1;
        buffer_index++;
    }

    db_after[id] = exam_database;
    return 0;
}

DWORD WINAPI DatabaseConsumer(LPVOID arg)
{
    int total = STUDENT_THREADS * SUBMISSIONS_PER_STUDENT;

    for (int i = 0; i < total; i++)
    {
        if (buffer_index <= 0)
            InterlockedIncrement(&buffer_underflows);
        else
            buffer_index--;
    }
    return 0;
}

DWORD WINAPI DeadlockProcess1(LPVOID arg)
{
    EnterCriticalSection(&resourceA);
    Sleep(100);
    EnterCriticalSection(&resourceB);
    LeaveCriticalSection(&resourceB);
    LeaveCriticalSection(&resourceA);
    return 0;
}

DWORD WINAPI DeadlockProcess2(LPVOID arg)
{
    EnterCriticalSection(&resourceB);
    Sleep(100);
    EnterCriticalSection(&resourceA);
    LeaveCriticalSection(&resourceA);
    LeaveCriticalSection(&resourceB);
    return 0;
}

DWORD WINAPI StarvationThread(LPVOID arg)
{
    for (int i = 0; i < 1; i++)
    {
        InterlockedIncrement(&starvation_events);
        Sleep(1000);
    }
    return 0;
}

int main()
{
    HANDLE producers[STUDENT_THREADS];
    HANDLE consumer, d1, d2, starve;
    int ids[STUDENT_THREADS];

    InitializeCriticalSection(&resourceA);
    InitializeCriticalSection(&resourceB);

    printf("\nCampusConnect: Process Sync Failure \n");

    swap_penalty = measure_hardware_swap();
    double start = now();
    double ls = now();

    consumer = CreateThread(NULL, 0, DatabaseConsumer, NULL, 0, NULL);

    for (int i = 0; i < STUDENT_THREADS; i++)
    {
        ids[i] = i;
        producers[i] = CreateThread(NULL, 0, StudentProducer, &ids[i], 0, NULL);
    }

    d1 = CreateThread(NULL, 0, DeadlockProcess1, NULL, 0, NULL);
    d2 = CreateThread(NULL, 0, DeadlockProcess2, NULL, 0, NULL);
    starve = CreateThread(NULL, 0, StarvationThread, NULL, 0, NULL);

    WaitForMultipleObjects(STUDENT_THREADS, producers, TRUE, INFINITE);
    WaitForSingleObject(consumer, INFINITE);

    double le = now();
    scheduling_latency = le - ls;
    double end = now();

    long expected = STUDENT_THREADS * SUBMISSIONS_PER_STUDENT;

    printf("\n=====================================================\n");
    printf("        CAMPUSCONNECT CONCURRENCY FAILURE REPORT      \n");
    printf("=====================================================\n");

    printf("\n[ DATA INTEGRITY MATRIX ]\n");
    printf("+---------+-----------+-----------+\n");
    printf("| Student | Before DB | After DB  |\n");
    printf("+---------+-----------+-----------+\n");

    for (int i = 0; i < STUDENT_THREADS; i++)
        printf("|   %d     | %9d | %9d |\n", i + 1, db_before[i], db_after[i]);

    printf("+---------+-----------+-----------+\n");

    printf("\nExpected Submissions      : %ld\n", expected);
    printf("Actual Recorded           : %ld\n", exam_database);
    printf("Lost Records (Race)       : %ld\n", expected - exam_database);
    printf("Data Integrity Rate       : %.2f%%\n",
        ((double)exam_database / expected) * 100);

    printf("\n[ PRODUCER-CONSUMER MATRIX ]\n");
    printf("Buffer Size               : %d slots\n", BUFFER_SIZE);
    printf("Buffer Overwrites         : %ld\n", buffer_overwrites);
    printf("Buffer Underflows         : %ld\n", buffer_underflows);

    printf("\n[ SWAPPING MATRIX ]\n");
    printf("Swap Penalty (Measured)   : %.6f units\n", swap_penalty);
    printf("Total Swap Events         : %ld\n", total_swaps);
    printf("Total Swap Overhead       : %.6f units\n", total_swaps * swap_penalty);

    printf("\n[ SCHEDULING & STARVATION ]\n");
    printf("Scheduling Latency        : %.6f sec\n", scheduling_latency);
    printf("Forced Context Switches   : %ld\n", context_switches);
    printf("Starvation Events         : %ld\n", starvation_events);

    printf("\n[ PERFORMANCE METRICS ]\n");
    printf("Total Execution Time      : %.6f seconds\n", end - start);
    printf("Throughput                : %.2f ops/sec\n",
        expected / (end - start));

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
