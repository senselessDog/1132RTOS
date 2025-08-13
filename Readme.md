# Lab 3: Ceiling Priority Protocol (CPP) for uC/OS-II Mutexes

This project implements the Ceiling Priority Protocol (CPP) to enhance the mutex (mutual exclusion semaphore) mechanism in uC/OS-II. The goal is to overcome the limitations of the default Priority Inheritance Protocol (PIP), such as the potential for multiple blocking and deadlocks.

## Introduction

uC/OS-II's default resource management protocol for mutexes is the Priority Inheritance Protocol (PIP). While PIP prevents uncontrolled priority inversion, it does not prevent a high-priority task from being blocked multiple times by different lower-priority tasks, nor does it prevent deadlocks.

The Ceiling Priority Protocol (CPP) is a more advanced protocol that addresses these issues. In CPP, each mutex is assigned a "ceiling priority," which is equal to the highest priority of any task that can lock it. When a task acquires a mutex, its own priority is immediately raised to the mutex's ceiling priority. This prevents other tasks, even those with a base priority higher than the current task but lower than the ceiling, from preempting it. This mechanism effectively prevents multiple blocking and deadlocks.

This lab modifies the standard uC/OS-II mutex functions to implement CPP.

## Kernel Modifications

The core of this lab is to modify how mutexes handle task priorities. The focus is on the `OSMutexPend()` and `OSMutexPost()` functions in `os_mutex.c`. We will also need to add a new field to the Task Control Block to store the task's original priority.

### 1. `ucos_ii.h` - TCB Extension

A new field, `OSTCBOriginalPrio`, is added to the `OS_TCB` structure to save a task's base priority before it is elevated by CPP. This ensures that the priority can be correctly restored when the mutex is released.

```c
typedef struct os_tcb {
    // ... existing fields ...
    INT8U  OSTCBOriginalPrio; // Stores the original priority before elevation
    // ... other custom fields from previous labs ...
} OS_TCB;
```
When a task is created, its `OSTCBOriginalPrio` should be initialized to its base priority (`prio`).

### 2. `os_mutex.c` - Implementing CPP Logic

#### **`OSMutexPend()` - Acquiring a Mutex**

This function is modified to implement the core priority ceiling logic.

1.  **Check Mutex Availability**: When a task calls `OSMutexPend()`, it first checks if the mutex is available (owned by no one).
2.  **Priority Elevation**: If the mutex is available:
    * The calling task (`OSTCBCur`) acquires the lock.
    * The task's original priority (`OSTCBCur->OSTCBPrio`) is saved in `OSTCBCur->OSTCBOriginalPrio`.
    * The task's current priority is immediately elevated to the mutex's ceiling priority (`pevent->OSEventCnt`). The ceiling priority is set when the mutex is created with `OSMutexCreate()`.
    * The task's position in the ready list must be updated to reflect its new, higher priority. This involves clearing its bit in the old priority slot and setting it in the new one. **This must be done manually without calling `OSTaskChangePrio()`**, as that function calls the scheduler and can cause unintended side effects.
3.  **Blocking**: If the mutex is not available, the task blocks as usual.

#### **`OSMutexPost()` - Releasing a Mutex**

This function is modified to restore the task's original priority.

1.  **Release Lock**: The calling task releases the mutex.
2.  **Priority Restoration**:
    * The task's current priority (`OSTCBCur->OSTCBPrio`) is restored from the saved value in `OSTCBCur->OSTCBOriginalPrio`.
    * Similar to the elevation process, the task's position in the ready list must be manually updated to reflect its original, lower priority.
3.  **Wake Up Waiting Task**: If any other tasks are waiting for the mutex, the highest priority waiting task is made ready, and the scheduler is called.

**Important Note on Manual Priority Change:**
Changing a task's priority involves directly manipulating the `OSRdyGrp` and `OSRdyTbl` bitmaps. You must calculate the new and old `x`, `y`, `BitX`, and `BitY` values and update the tables accordingly.

```c
// Simplified example of manual priority change
// In OSMutexPend():
// 1. Save original priority: ptcb->OSTCBOriginalPrio = ptcb->OSTCBPrio;
// 2. Clear from old ready list slot: OSRdyTbl[old_y] &= ~old_bitx;
// 3. Set new priority: ptcb->OSTCBPrio = new_prio;
// 4. Update TCB priority fields (OSTCBX, OSTCBY, etc.)
// 5. Add to new ready list slot: OSRdyTbl[new_y] |= new_bitx;
```

## Application and Scenarios

The application code should simulate tasks with different priorities competing for shared resources (represented by mutexes). The key is to reproduce the scenarios where PIP fails and demonstrate how CPP resolves them.

* **Scenario 1 (Multiple Blocking)**: A high-priority task is blocked by two different lower-priority tasks in succession because they hold different locks that the high-priority task needs. CPP should prevent this by ensuring the first lower-priority task's priority is raised high enough to prevent the second lower-priority task from running.
* **Scenario 2 (Deadlock)**: Two tasks attempt to acquire locks in a reverse order, leading to a deadly embrace. CPP prevents this by design, as a task holding a lock cannot be preempted by another task that would attempt to lock a resource with a lower ceiling.

The application tasks should use `OSTimeDly()` to simulate arrival times and a spinning loop (`while (compTime > 0)`) to simulate execution, just as in previous labs.

## Output Format

The output should clearly show the locking and unlocking events, including the priority changes.

```
Time  Event          From/Task   To/Prio    Comments
------------------------------------------------------------------
  20  lock R1        Task3(Prio=5)  Prio=1    (Priority elevated to 1)
  90  unlock R1      Task3(Prio=1)  Prio=5    (Priority restored to 5)
...
```
This detailed logging will make it possible to trace the execution flow and verify that CPP is working correctly to prevent multiple blocking and deadlocks.
