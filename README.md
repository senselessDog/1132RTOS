# Lab 4: Earliest Deadline First (EDF) Scheduler for FreeRTOS

This project adapts the FreeRTOS kernel to implement a dynamic Earliest Deadline First (EDF) scheduler, replacing its native fixed-priority, preemptive scheduling algorithm.

## Introduction

FreeRTOS, like most commercial RTOSs, is built around a scheduler that prioritizes tasks based on a static priority level assigned at creation. This lab aims to fundamentally change this behavior by introducing an EDF scheduling policy. With EDF, the task with the most urgent (i.e., the earliest) deadline is always chosen to run, regardless of any pre-assigned priority.

This modification requires a deep dive into the FreeRTOS source code to identify and alter the core scheduling logic.

## FreeRTOS Kernel Modifications

The key to implementing EDF in FreeRTOS is to modify the mechanism that selects the next task to run.

### 1. TCB Extension (`tasks.c`)

First, we need to add deadline information to the Task Control Block (`TCB_t` structure, typically defined in `tasks.c`). A new member, `xDeadline`, is added to store the absolute deadline of the task's current job.

```c
// Inside the TCB_t structure in tasks.c
typedef struct tskTaskControlBlock
{
    // ... existing members ...
    TickType_t xDeadline; // Absolute deadline for the task
    // ... existing members ...
} tskTCB;
```

When a task is created using `xTaskCreate`, this `xDeadline` member should be initialized, typically to the task's first deadline.

### 2. Core Scheduling Logic (`tasks.c`)

The heart of the FreeRTOS scheduler is the `vTaskSwitchContext()` function. This function is responsible for selecting the highest-priority task that is ready to run. The default implementation uses a series of ready lists (`pxReadyTasksLists`) indexed by priority. We must bypass this and implement our own selection logic.

#### **New Scheduling Logic: Linear Search for Earliest Deadline**

Instead of using the ready lists, we will iterate through the list of all created tasks to find the one that is ready and has the earliest deadline. FreeRTOS maintains lists of tasks in various states (ready, blocked, suspended). We need to find the appropriate list(s) that contain all ready tasks.

The new logic, which should be placed at the beginning of `vTaskSwitchContext()`, will:
1.  Temporarily remove all tasks from the standard ready lists to prevent the original scheduler logic from interfering.
2.  Perform a linear search through all tasks. For each task, check if it's in the ready state.
3.  Keep track of the ready task with the minimum `xDeadline`.
4.  Once the task with the earliest deadline is found, place *only that task* back into its corresponding ready list.
5.  Allow the rest of `vTaskSwitchContext()` to execute. It will find only one task in the ready lists and "schedule" it to run.
6.  Finally, ensure all other ready tasks (that were not selected) are placed back into the ready lists so they are not lost.

This approach, while not the most efficient, effectively forces the FreeRTOS scheduler to follow the EDF policy.

A conceptual implementation:
```c
// At the beginning of vTaskSwitchContext() in tasks.c

// 1. Find the ready task with the earliest deadline (p_edf_task)
//    by iterating through all tasks.
//    (This requires knowledge of FreeRTOS internal list structures)

// 2. Set pxCurrentTCB to point to the selected EDF task.
//    pxCurrentTCB = p_edf_task;
```
*Note: A more direct modification would be to replace the `taskSELECT_HIGHEST_PRIORITY_TASK()` macro with a custom function that performs the linear search for the earliest deadline.*

## Application Implementation

The application code will create a set of periodic tasks. The logic for each task is similar to the previous labs.

### Periodic Task Logic

Each periodic task will execute in a loop:

1.  **Perform Work**: Simulate computation by spinning in a loop for a specified duration.
2.  **Update Deadline**: Before delaying, the task must calculate and update its next absolute deadline.
    ```c
    // Inside the periodic task's while(1) loop
    TickType_t xNextDeadline;
    xNextDeadline = xTaskGetTickCount() + xPeriodInTicks;
    // A custom function or direct TCB access would be needed to set the deadline
    vTaskSetDeadline(xTaskGetCurrentTaskHandle(), xNextDeadline);
    ```
3.  **Delay**: Use `vTaskDelayUntil()` to wait for the start of the next period. This function is ideal for maintaining a fixed execution frequency.

### Test Cases

The implementation should be tested with the following task sets to observe the EDF scheduling behavior.

* **Task Set 1**: `{ t1(1, 3), t2(3, 5) }`
* **Task Set 2**: `{ t1(1, 4), t2(2, 5), t3(2, 10) }`

## Output Format

The console output should log the "complete" and "preempt" events to trace the scheduler's decisions. The task names (`T1`, `T2`, etc.) should be used for clarity.

**Example for Task Set 1:**
```
1 complete T1 T2
4 complete T2 T1
5 complete T1 T2
6 preempt T2 T1
7 complete T1 T2
...
```
This output will allow for analysis of the EDF schedule and comparison with theoretical results.
