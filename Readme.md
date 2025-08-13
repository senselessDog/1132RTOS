# Periodic Task Emulation in uC/OS-II

This project demonstrates how to emulate and observe the behavior of periodic tasks under the Rate Monotonic (RM) scheduling algorithm in the uC/OS-II real-time kernel.

## Introduction

The primary goal of this lab is to implement periodic tasks and monitor their scheduling behaviors, such as context switches and deadline violations. Since uC/OS-II does not natively support periodic tasks, this project involves modifying the kernel to simulate this functionality. This is achieved by adding custom fields to the Task Control Block (TCB) and instrumenting key kernel functions to log scheduling events.

## Kernel Modifications

To support periodic task emulation, the following changes were made to the uC/OS-II kernel files.

### 1. `ucos_ii.h`

The `OS_TCB` structure was extended with three new fields to manage task periodicity and execution time:

```c
typedef struct os_tcb {
    // ... existing fields ...

    INT8U  compTime;   /* Remaining computation time in ticks */
    INT8U  period;     /* Task period in ticks */
    INT32U deadline;   /* Next absolute deadline */
} OS_TCB;
```

### 2. `os_core.c`

Several kernel functions were modified to handle the new TCB fields and to log scheduling events.

#### **OSTimeTick()**

This function is called on every system tick. A small piece of code was added to decrement the `compTime` of the currently running task, simulating the consumption of one CPU tick.

```c
// At the end of OSTimeTick()
OS_ENTER_CRITICAL();
if (OSTCBCur->compTime > 0) {
    OSTCBCur->compTime--;
}
OS_EXIT_CRITICAL();
```

#### **OSIntExit()**

This function is called when an Interrupt Service Routine (ISR) completes. It handles involuntary context switches (preemptions). Code was added to log a "Preempt" event when a higher-priority task becomes ready to run.

```c
// Inside OSIntExit(), before OSIntCtxSw() is called
if (OSPrioHighRdy != OSPrioCur) {
    char tempBuf[MSG_BUF_SIZE];
    sprintf(tempBuf, "%5d Preempt     %3d      %3d\n",
            (int)OSTimeGet(), (int)OSPrioCur, (int)OSPrioHighRdy);
    AddMessageToQueue(tempBuf); // Custom function to buffer messages
    // ...
}
```

#### **OS_Sched()**

This function is called for voluntary context switches, typically when a task completes its current job and yields the CPU. Code was added to log a "Complete" event.

```c
// Inside OS_Sched(), before OS_TASK_SW() is called
if (OSPrioHighRdy != OSPrioCur) {
    char tempBuf[MSG_BUF_SIZE];
    sprintf(tempBuf, "%5d Complete    %3d      %3d\n",
            (int)OSTimeGet(), (int)OSPrioCur, (int)OSPrioHighRdy);
    AddMessageToQueue(tempBuf);
    // ...
}
```

## Implementation (`lab1.c`)

The main application file `lab1.c` sets up and runs the periodic tasks.

### Task Creation

Tasks are created using `OSTaskCreateExt`. Each periodic task is an instance of the `PeriodicTask` function. A `TASK_PARAM` struct containing the computation time (`c`) and period (`p`) is passed as an argument.

* **Task Set 1**: `{ t1(1,3), t2(3,6) }`
* **Task Set 2**: `{ t1(1,3), t2(3,6), t3(4,9) }`

Priorities are assigned based on the Rate Monotonic (RM) principle: shorter period means higher priority.

### Periodic Execution Logic

The `PeriodicTask` function implements the core logic for periodic execution:

1.  **Initialization**: At the start, each task initializes its `compTime`, `period`, and initial `deadline` in its own TCB.
2.  **Execution Loop**: The task enters an infinite loop. Inside the loop, it spins (consumes CPU) until its `compTime` for the current period is decremented to zero by `OSTimeTick`.
3.  **Delay and Replenish**: After the computation is "done", the task calculates the time remaining until its next period begins (`toDelay`). It then calls `OSTimeDly()` to sleep for that duration.
4.  **Deadline Check**: Before sleeping, it checks if a deadline violation has occurred. If `toDelay` is negative, it means the task took longer than its period to complete.
5.  **Next Period**: After the delay, it replenishes its `compTime` to its initial value and calculates the next deadline, starting a new period.

### Output Logging

Since `printf` is not safe to call from within an ISR or critical section, all event messages are stored in a shared message queue (`MsgQueue`). In this implementation, the printing is handled within the periodic task's loop after its computation phase to ensure thread safety. The output format is as follows:

```
Time  Event       [From]   [To]
---------------------------------
   1 Preempt        63        6
   2 Complete        6        7
...
```

## How to Run

1.  Apply the kernel modifications as described above.
2.  Compile the project with the `lab1.c` file.
3.  Load and run the application on the target hardware (e.g., Altera NIOS II) or simulator.
4.  Observe the console output for the scheduling event trace. To switch between Task Set 1 and Task Set 2, comment or uncomment the creation of `Task3` in the `TaskStart` function in `lab1.c`.
