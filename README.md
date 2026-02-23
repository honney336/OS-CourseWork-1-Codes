# OS Coursework 1: Kernel Mechanisms & CPU Scheduling Analysis

A comprehensive implementation and comparative study of Operating System kernels, focusing on **Inter-Process Communication (IPC)**, **Process Synchronization**, and **CPU Scheduling Algorithms** on both Linux and Windows.

## 📂 Project Structure

### 🐧 Linux (POSIX)
- **IPC.c**: Shared memory & semaphore implementation.
- **process_sync.c**: Demonstration of race conditions and mutex/semaphore solutions.
- **Scheduling Algorithms**: 
  - `linfcfs.c` (First-Come, First-Served)
  - `linsjf.c` (Shortest Job First)
  - `linrr.c` (Round Robin)
  - `linps.c` (Priority Scheduling)

### 🪟 Windows (Win32 API)
- **winIPC.c**: Win32 File Mapping and Mutex implementation.
- **process_sync_prob.c / solu.c**: Windows-specific synchronization primitives.
- **Scheduling Algorithms**:
  - `winFCFS.c`, `winSJF.c`, `winRR.c`, `winPS.c` (Win32 thread-based scheduling simulations).

---

## 🚀 Technical Highlights

### 🔄 CPU Scheduling Comparison
This project simulates how different OS kernels manage process queues:
* **FCFS**: Non-preemptive scheduling based on arrival time.
* **SJF**: Optimal waiting time simulation by picking the shortest burst.
* **Round Robin**: Time-quantum based preemptive scheduling.
* **Priority**: Importance-based execution logic.



### 📡 Inter-Process Communication
Comparing **System V / POSIX** logic against **Win32** logic:
* **Linux**: Uses `shmget`, `shmat`, and `sem_open`. High efficiency via `fork()` copy-on-write memory.
* **Windows**: Uses `CreateFileMapping` and `CreateMutex`. Robust handles-based security model.

### 📊 Performance Analytics
Every executable produces a kernel-level report including:
* **Turnaround & Waiting Times** (for scheduling).
* **Context Switches** (Voluntary vs Involuntary).
* **Memory Pressure** (Minor/Major Page Faults).

---

## 🛠️ Compilation Guide

### Linux (GCC)
```bash
# Example for Scheduling
gcc linrr.c -o ./executables/linrr
# Example for IPC (requires pthread)
gcc IPC.c -o ./executables/IPC -pthread
```

### Windows (MinGW)
```
# Example for Scheduling
gcc winRR.c -o ./executables/winRR.exe
# Example for IPC (requires psapi for kernel metrics)
gcc winIPC.c -o ./executables/IPC.exe -lpsapi
```

