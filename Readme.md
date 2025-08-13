# Lab 2: Earliest Deadline First (EDF) Scheduler for uC/OS-II

This project involves modifying the uC/OS-II real-time kernel to replace its default fixed-priority scheduler with a dynamic-priority Earliest Deadline First (EDF) scheduler.

## Introduction

Unlike the Rate Monotonic (RM) scheduler from Lab 1, which uses static priorities, the EDF scheduler dynamically assigns priorities based on job deadlines. The core principle of EDF is simple: **the job with the earliest absolute deadline has the highest priority**. This lab focuses on implementing this dynamic scheduling logic within the uC/OS-II kernel.

## Kernel Modifications

To transform the uC/OS-II scheduler into an EDF scheduler, significant changes must be made to the kernel's core scheduling logic. The original priority-based bitmap (`OSRdyGrp` and `OSRdyTbl`) will be bypassed in favor of a new mechanism that finds the task with the earliest deadline.

### 1. `ucos_ii.h` - TCB Extension

The Task Control Block (`OS_TCB`) structure must contain deadline information. We will reuse and adapt the structure from Lab 1. The `deadline` field is crucial for the EDF scheduling decisions.

```c
typedef struct os_tcb {
    // ... existing fields ...

    INT8U  compTime;   /* Remaining computation time in ticks */
    INT8U  period;     /* Task period in ticks */
    INT32U deadline;   /* Absolute deadline for the current job */
} OS_TCB;
```

### 2. `os_core.c` - Modifying Scheduling Points

The primary scheduling decisions in uC/OS-II occur in three functions: `OSStart()`, `OSIntExit()`, and `OS_Sched()`. The logic within these functions needs to be altered to implement EDF.

#### **New Scheduling Logic: `OS_SchedNew_EDF()`**

A new function, `OS_SchedNew_EDF()`, should be created to find the next task to run. This function will perform a **linear search** through the linked list of all tasks (`OSTCBList`).

The logic is as follows:
1.  Initialize a pointer `p_edf_tcb` to null and a variable `min_deadline` to a very large value.
2.  Iterate through `OSTCBList` from the head.
3.  For each task control block (`ptcb`) in the list:
    * Check if the task is ready to run (i.e., `(ptcb->OSTCBStat & OS_STAT_RDY) != 0`).
    * If the task is ready, compare its `ptcb->deadline` with `min_deadline`.
    * If `ptcb->deadline` is smaller, update `min_deadline` to this new value and set `p_edf_tcb` to point to the current `ptcb`.
4.  After the search is complete, `p_edf_tcb` will point to the ready task with the earliest deadline. The priority of this task (`p_edf_tcb->OSTCBPrio`) becomes the new `OSPrioHighRdy`.

An example implementation snippet:
```c
static void OS_SchedNew_EDF(void) {
    OS_TCB *ptcb;
    OS_TCB *p_edf_tcb = (OS_TCB *)0;
    INT32U min_deadline = 0xFFFFFFFF;

    ptcb = OSTCBList;
    while (ptcb != (OS_TCB *)0) {
        if ((ptcb->OSTCBStat & OS_STAT_RDY) != 0) { // Check if task is ready
            if (ptcb->deadline < min_deadline) {
                min_deadline = ptcb->deadline;
                p_edf_tcb = ptcb;
            }
        }
        ptcb = ptcb->OSTCBNext;
    }

    if (p_edf_tcb != (OS_TCB *)0) {
        OSPrioHighRdy = p_edf_tcb->OSTCBPrio;
        OSTCBHighRdy = p_edf_tcb;
    } else {
        // If no ready task is found, default to idle task
        OSPrioHighRdy = OS_TASK_IDLE_PRIO;
        OSTCBHighRdy = OSTCBPrioTbl[OS_TASK_IDLE_PRIO];
    }
}
```

#### **Integrating `OS_SchedNew_EDF()`**

You must replace the calls to the original `OS_SchedNew()` with your new `OS_SchedNew_EDF()` inside `OSStart()`, `OSIntExit()`, and `OS_Sched()`. This ensures that every time the scheduler runs, it uses the EDF policy.

## Application Implementation (`lab2.c`)

The periodic task logic from Lab 1 can be largely reused. The most critical change is how a task's deadline is managed.

### Deadline Management

When a periodic task completes its computation for the current period and is about to delay itself until the next period, its deadline must be advanced.

```c
// Inside the periodic task's main loop, before calling OSTimeDly()

// ... computation completes ...

OS_ENTER_CRITICAL();
// Advance the deadline for the next period
OSTCBCur->deadline = OSTCBCur->deadline + (INT32U)OSTCBCur->period;
OS_EXIT_CRITICAL();

// Call OSTimeDly() to wait for the next period
OSTimeDly(toDelay);
```

This ensures that when the task becomes ready again, it competes for the CPU with an updated deadline.

## Test Cases

This project requires testing the EDF scheduler with the following two sets of periodic tasks.

* **Task Set 1**: `{ t1(1, 3), t2(3, 5) }`
* **Task Set 2**: `{ t1(1, 4), t2(2, 5), t3(2, 10) }`

## Output Format

The output format for logging scheduling events ("Preempt", "Complete") should remain the same as in Lab 1, allowing for a direct comparison of the scheduling behavior.

```
Time  Event       [From]   [To]
---------------------------------
   0 Preempt        63        1
   1 Complete        1        2
...
