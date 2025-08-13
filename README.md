# Lab 5: Checkpointing in FreeRTOS for Power-Fail Recovery

This project implements a checkpointing mechanism in FreeRTOS to ensure system state can be recovered after a simulated power failure. This is critical for embedded systems that need to maintain progress and data integrity despite unexpected power interruptions.

## Introduction

The goal of this lab is to build a resilient system that can survive power loss. This is achieved by periodically saving a "checkpoint" of the entire system state to non-volatile memory (FRAM). If power is lost, the system can restore itself from the last valid checkpoint upon rebooting, effectively resuming its operation from a known-good state.

The process involves two main operations:
1.  **Commit**: Periodically backing up the critical system state.
2.  **Restore**: On startup, detecting if a power failure occurred and, if so, restoring the system from the backup.

## System Architecture

### Memory Layout

A dedicated section of Ferroelectric RAM (FRAM) is reserved for storing checkpoint data. FRAM is used because it is non-volatile (like Flash) but offers fast write speeds (like SRAM). The memory mapping must be configured (e.g., via a linker command file `.cmd`) to create this backup space. To handle potential corruption during a write operation, two backup slots are often used, with an index to point to the last successfully written copy.

### Core Components to Backup

A complete system checkpoint must include all data necessary to fully restore the system's state. In this FreeRTOS-based system, this includes:

1.  **CPU Registers**: The entire CPU context, including the Program Counter (PC), Stack Pointer (SP), and all general-purpose registers. This is the most critical part, as it captures the exact execution state of the current task.
2.  **SRAM**: The entire contents of the SRAM, which includes:
    * `.data` section: Initialized global and static variables.
    * `.bss` section: Uninitialized global and static variables.
    * The system stack used by interrupts and function calls.
3.  **FreeRTOS Heap (`ucHeap`)**: The memory region used by FreeRTOS for dynamic allocations, such as creating tasks, queues, and semaphores. The state of all TCBs and other kernel objects resides here.

The backup must be performed in a specific order (e.g., Heap, SRAM, Registers) to ensure consistency.

## Implementation Details

### Checkpoint Commit

The `commit()` function is responsible for saving the system state.

1.  **Triggering**: In the main test task, `commit()` is called periodically (e.g., every 10 iterations of a loop).
2.  **Backup Process**:
    * The function copies the entire `ucHeap` array to the reserved FRAM space.
    * It then copies the entire SRAM (from its start to end address) to FRAM.
    * Finally, it saves the current CPU registers. This is the trickiest part and is highly architecture-dependent. It often involves inline assembly to push all registers onto the stack and then copying that stack frame to FRAM.

### Power Failure Simulation and Restoration

#### **Simulating Power Failure**
A power failure is simulated randomly within the main test task. When a random condition is met, the program enters a low-power mode (`LPM4.5`) or a simple infinite loop to halt execution, mimicking a power-down state. The system is then manually reset by pressing the reset button on the board.

#### **Restoration Logic**
The `restore()` function is called once at the very beginning of `main()`, right after hardware initialization.

1.  **Detection**: The first step is to determine if the boot is a "cold start" or a recovery from a power failure. This is typically done by checking a "magic number" or a validity flag in a specific location in FRAM. If the flag is not set, it's a first-time boot, and the restoration is skipped.
2.  **Restore Process**: If recovery is needed, the `restore()` function performs the backup process in reverse:
    * It copies the saved `ucHeap` from FRAM back to its original location in RAM.
    * It copies the saved SRAM data from FRAM back to RAM.
    * It restores the CPU registers from the saved context in FRAM. This will cause the program to jump back to the exact point where the checkpoint was taken, effectively resuming the `commit()` function from where it left off.

## Application Flow

The `main()` function is structured as follows:

```c
void main(void)
{
    // 1. Initialize hardware (e.g., I/O, clock)
    prvSetupHardware();

    // 2. Attempt to restore from a checkpoint
    restore(); // This function will either restore and resume, or return if it's a cold boot.

    // 3. If it's a cold boot, create tasks and start the scheduler
    xTaskCreate(vTestTask, ...);
    vTaskStartScheduler(); // The program should not return from here
}

void vTestTask(void *pvParameters)
{
    int i = 0;
    while(1)
    {
        // Simulate work
        i++;
        printf("%d\n", i);

        // Randomly simulate a power failure
        if (should_power_fail()) {
            power_off(); // Enter low-power mode
        }

        // Periodically commit a checkpoint
        if ((i % 10) == 0) {
            commit();
        }
    }
}
```
When the system is restored, it will resume execution inside the `commit()` function, return to the `vTestTask`, and continue the loop, printing the next value of `i`. The console output will demonstrate that the system did not restart from 0, but rather continued from its last saved state.
