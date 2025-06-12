/*
 * FreeRTOS V202107.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/******************************************************************************
 * This project provides two demo applications.  A simple blinky style project,
 * and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting (defined in this file) is used to
 * select between the two.  The simply blinky demo is implemented and described
 * in main_blinky.c.  The more comprehensive test and demo application is
 * implemented and described in main_full.c.
 *
 * This file implements the code that is not demo specific, including the
 * hardware setup and standard FreeRTOS hook functions.
 *
 * ENSURE TO READ THE DOCUMENTATION PAGE FOR THIS PORT AND DEMO APPLICATION ON
 * THE http://www.FreeRTOS.org WEB SITE FOR FULL INFORMATION ON USING THIS DEMO
 * APPLICATION, AND ITS ASSOCIATE FreeRTOS ARCHITECTURE PORT!
 *
 */

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
functionality in an interrupt. */
#include "EventGroupsDemo.h"
#include "TaskNotify.h"
//#include "ParTest.h" /* LEDs - a historic name for "Parallel Port". */

/* TI includes. */
#include "driverlib.h"

/* Set mainCREATE_SIMPLE_BLINKY_DEMO_ONLY to one to run the simple blinky demo,
or 0 to run the more comprehensive test and demo application. */
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY  1

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/
/* HERE STARTS THE LAB 5 CHECKPOINT CODE           */
/*-----------------------------------------------------------*/

// Linker-defined symbols for SRAM sections.
// Ensure your linker command file (.cmd) exposes these symbols.
extern char __bsp_sram_data_start__;
extern char __bsp_sram_data_end__;
extern char __bsp_sram_bss_start__;
extern char __bsp_sram_bss_end__;

#define SRAM_DATA_START     ((void * const)&__bsp_sram_data_start__)
#define SRAM_DATA_SIZE      ((size_t)(&__bsp_sram_data_end__ - &__bsp_sram_data_start__))
#define SRAM_BSS_START      ((void * const)&__bsp_sram_bss_start__)
#define SRAM_BSS_SIZE       ((size_t)(&__bsp_sram_bss_end__ - &__bsp_sram_bss_start__))

// Status flags for checkpointing
#define STATUS_CLEAN_START            0x00
#define STATUS_CHECKPOINT_A_VALID     0x01
#define STATUS_CHECKPOINT_B_VALID     0x02
#define SRAM_DATA_BACKUP_SIZE   512     // 預設 256 bytes 給 .data
#define SRAM_BSS_BACKUP_SIZE   1024    // 預設 2KB 給 .bss
#define NUM_REGISTERS_TO_BACKUP 15

// The structure that holds our backup data
typedef struct {
    uint8_t heap_bak[configTOTAL_HEAP_SIZE];
    uint8_t data_bak[SRAM_DATA_BACKUP_SIZE];
    uint8_t bss_bak[SRAM_BSS_BACKUP_SIZE];
    // NOTE: CPU registers are part of the task context, which is saved
    // within the task's stack. The stack itself is in SRAM (.bss or heap),
    // so backing up SRAM/heap effectively backs up the registers.
    uint16_t registers_bak[NUM_REGISTERS_TO_BACKUP];  // R1-R15
} Checkpoint_t;
#pragma PERSISTENT(checkpoint_status)
uint8_t checkpoint_status = STATUS_CLEAN_START;
// Reserve space for two checkpoint copies in persistent FRAM memory
#pragma PERSISTENT(checkpoint_a)
Checkpoint_t checkpoint_a = {0};

#pragma PERSISTENT(checkpoint_b)
Checkpoint_t checkpoint_b = {0};



// The heap itself, must be persistent
#pragma PERSISTENT(ucHeap)
uint8_t ucHeap[configTOTAL_HEAP_SIZE] = {0};
// Function to backup CPU registers R1-R15
// Function to backup CPU registers R1-R15
void backupRegisters(uint16_t *backup_register) {
//    __asm(" mov.w  r1,  0(r12)");    // backup_register[0] = R1
    __asm(" mov.w  r2,  2(r12)");    // backup_register[1] = R2 (SP)
    __asm(" mov.w  r3,  4(r12)");    // backup_register[2] = R3
    __asm(" mov.w  r4,  6(r12)");    // backup_register[3] = R4
    __asm(" mov.w  r5,  8(r12)");    // backup_register[4] = R5
    __asm(" mov.w  r6,  10(r12)");   // backup_register[5] = R6
    __asm(" mov.w  r7,  12(r12)");   // backup_register[6] = R7
    __asm(" mov.w  r8,  14(r12)");   // backup_register[7] = R8
    __asm(" mov.w  r9,  16(r12)");   // backup_register[8] = R9
    __asm(" mov.w  r10, 18(r12)");   // backup_register[9] = R10
    __asm(" mov.w  r11, 20(r12)");   // backup_register[10] = R11
    __asm(" mov.w  r12, 22(r12)");   // backup_register[11] = R12
    __asm(" mov.w  r13, 24(r12)");   // backup_register[12] = R13
    __asm(" mov.w  r14, 26(r12)");   // backup_register[13] = R14
    __asm(" mov.w  r15, 28(r12)");   // backup_register[14] = R15
}

// Function to restore CPU registers R1-R15
void restoreRegisters(uint16_t *backup_register) {
//    __asm(" mov.w  0(r12),  r1");    // R1 = backup_register[0]
    __asm(" mov.w  2(r12),  r2");    // R2 = backup_register[1] (SP)
    __asm(" mov.w  4(r12),  r3");    // R3 = backup_register[2]
    __asm(" mov.w  6(r12),  r4");    // R4 = backup_register[3]
    __asm(" mov.w  8(r12),  r5");    // R5 = backup_register[4]
    __asm(" mov.w  10(r12), r6");    // R6 = backup_register[5]
    __asm(" mov.w  12(r12), r7");    // R7 = backup_register[6]
    __asm(" mov.w  14(r12), r8");    // R8 = backup_register[7]
    __asm(" mov.w  16(r12), r9");    // R9 = backup_register[8]
    __asm(" mov.w  18(r12), r10");   // R10 = backup_register[9]
    __asm(" mov.w  20(r12), r11");   // R11 = backup_register[10]
    __asm(" mov.w  22(r12), r12");   // R12 = backup_register[11]
    __asm(" mov.w  24(r12), r13");   // R13 = backup_register[12]
    __asm(" mov.w  26(r12), r14");   // R14 = backup_register[13]
    __asm(" mov.w  28(r12), r15");   // R15 = backup_register[14]
}
void commit(void) {
    Checkpoint_t *backup_target;
    printf("SRAM_DATA_SIZE: %d, BACKUP_SIZE: %d\n",
               (int)SRAM_DATA_SIZE, SRAM_DATA_BACKUP_SIZE);
    printf("SRAM_BSS_SIZE: %d, BACKUP_SIZE: %d\n",
           (int)SRAM_BSS_SIZE, SRAM_BSS_BACKUP_SIZE);

    // Make sure we're not overflowing
    if (SRAM_DATA_SIZE > SRAM_DATA_BACKUP_SIZE) {
        printf("WARNING: SRAM_DATA_SIZE exceeds backup size!\n");
    }
    if (SRAM_BSS_SIZE > SRAM_BSS_BACKUP_SIZE) {
        printf("WARNING: SRAM_BSS_SIZE exceeds backup size!\n");
    }
    // Determine the backup target (ping-pong buffering)
    if (checkpoint_status == STATUS_CHECKPOINT_A_VALID || checkpoint_status == STATUS_CLEAN_START) {
        backup_target = &checkpoint_b;
        printf("b ");
    } else {
        backup_target = &checkpoint_a;
        printf("a ");
    }
    if (backup_target == &checkpoint_a) {
        checkpoint_status = STATUS_CHECKPOINT_A_VALID;
    } else {
        checkpoint_status = STATUS_CHECKPOINT_B_VALID;
    }
    printf("commit\n");
    // --- Perform backup ---

    // 1. Backup ucHeap
    memcpy(backup_target->heap_bak, ucHeap, configTOTAL_HEAP_SIZE);

    // 2. Backup SRAM (.data and .bss)
    memcpy(backup_target->data_bak, SRAM_DATA_START, SRAM_DATA_SIZE);
    memcpy(backup_target->bss_bak, SRAM_BSS_START, SRAM_BSS_SIZE);
    backupRegisters(backup_target->registers_bak);

    // --- Update status ---
    // After a successful backup, update the status to validate the new checkpoint

}

/**
 * @brief Restores the system state from a checkpoint if one is valid.
 */
static int i = 1;
static int next_power_fail_count=0;
static int random_interval=0;
void restore(void) {
    Checkpoint_t *backup_source;

    // Determine which backup is valid
    if (checkpoint_status == STATUS_CHECKPOINT_A_VALID) {
        backup_source = &checkpoint_a;
        printf("A\n");
    } else if (checkpoint_status == STATUS_CHECKPOINT_B_VALID) {
        backup_source = &checkpoint_b;
        printf("B\n");
    } else {
        printf("No\n");
        return; // No valid checkpoint, perform a clean start
    }

    // --- Perform restore ---
    // 1. Restore ucHeap
    memcpy(ucHeap, backup_source->heap_bak, configTOTAL_HEAP_SIZE);

    // 2. Restore SRAM (.data and .bss)
    memcpy(SRAM_DATA_START, backup_source->data_bak, SRAM_DATA_SIZE);
    memcpy(SRAM_BSS_START, backup_source->bss_bak, SRAM_BSS_SIZE);
    //update timeout
    next_power_fail_count = i + random_interval;
    printf("--- System start/resume. Current count: %d. New failure target: %d ---\n", i, next_power_fail_count);
    //backup register
    restoreRegisters(backup_source->registers_bak);
    vTaskStartScheduler();
}


static void test_task(void *pvParameters)
{
    // 這個變數必須是 static，它才會被放在 .data 或 .bss 區段，
    // 我們的 commit() 和 restore() 才能夠正確地備份與還原它。
//    static int i = 0;

    // 這個變數用來儲存下一次斷電時 'i' 的目標計數值。
    // 它可以是函式內的區域變數，因為我們希望每次從斷電中恢復時，
    // 都能重新計算一個新的、不可預測的斷電目標。


    // 只有在系統是全新啟動時 (i=0)，才設定隨機數種子。
    // 如果是從斷點恢復 (i != 0)，我們就不重設種子，
    // 這樣才能在恢復後產生與上次不同的隨機間隔。
    if (i == 0)
    {
        srand(1234);
    }
    random_interval = 11 + (rand() % 19);

    // 設定下一次發生斷電的目標計數值
    next_power_fail_count = i + random_interval;
    printf("--- System start/resume. Current count: %d. New failure target: %d ---\n", i, next_power_fail_count);


    while (1)
    {

        printf("%d\n", i);

        // 每 10 次迴圈 commit 一次
        if (i % 10 == 0)
        {
            commit();
        }

        // 檢查計數器是否已達到預設的斷電目標
        if (i >= next_power_fail_count)
        {
            printf("--- Simulating power failure! ---\n");
            random_interval = i + (rand() % 19);
            // 進入 LPM4.5 低功耗模式，系統將在被喚醒 (例如按下Reset鈕) 後重置
//            PMM_turnOnRegulator();
            PMM_enterLPM45();
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // 延遲 500ms
        i++;
    }
}
void main_lab(void) {
//    printf("%d",Start);
    xTaskCreate(test_task, "TestTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL);

    vTaskStartScheduler();

    for (;;); // 程式不應執行到此處
}
/*-----------------------------------------------------------*/
/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

/*
 * main_blinky() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1.
 * main_full() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 0.
 */
#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
    extern void main_lab( void );
#else
    extern void main_full( void );
#endif /* #if mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 */

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

/* The heap is allocated here so the "persistent" qualifier can be used.  This
requires configAPPLICATION_ALLOCATED_HEAP to be set to 1 in FreeRTOSConfig.h.
See http://www.freertos.org/a00111.html for more information. */
#ifdef __ICC430__
    __persistent                    /* IAR version. */
#else
//  #pragma PERSISTENT( ucHeap )    /* CCS version. */
#endif
//uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] = { 0 };

/*-----------------------------------------------------------*/

int main( void )
{
    /* See http://www.FreeRTOS.org/MSP430FR5969_Free_RTOS_Demo.html */

    /* Configure the hardware ready to run the demo. */
    prvSetupHardware();
    /* Restore a checkpoint if a power failure occurred. */
    restore();
    /* The mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is described at the top
    of this file. */
    #if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
    {
        main_lab();
    }
    #else
    {
        main_full();
    }
    #endif

    return 0;
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected.
    See http://www.freertos.org/Stacks-and-stack-overflow-checking.html */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    __bis_SR_register( LPM4_bits + GIE );
    __no_operation();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    #if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0 )
    {
        /* Call the periodic event group from ISR demo. */
        vPeriodicEventGroupsProcessing();

        /* Call the code that 'gives' a task notification from an ISR. */
        xNotifyTaskFromISR();
    }
    #endif
}
/*-----------------------------------------------------------*/

/* The MSP430X port uses this callback function to configure its tick interrupt.
This allows the application to choose the tick interrupt source.
configTICK_VECTOR must also be set in FreeRTOSConfig.h to the correct
interrupt vector for the chosen tick interrupt source.  This implementation of
vApplicationSetupTimerInterrupt() generates the tick from timer A0, so in this
case configTICK_VECTOR is set to TIMER0_A0_VECTOR. */
void vApplicationSetupTimerInterrupt( void )
{
const unsigned short usACLK_Frequency_Hz = 32768;

    /* Ensure the timer is stopped. */
    TA0CTL = 0;

    /* Run the timer from the ACLK. */
    TA0CTL = TASSEL_1;

    /* Clear everything to start with. */
    TA0CTL |= TACLR;

    /* Set the compare match value according to the tick rate we want. */
    TA0CCR0 = usACLK_Frequency_Hz / configTICK_RATE_HZ;

    /* Enable the interrupts. */
    TA0CCTL0 = CCIE;

    /* Start up clean. */
    TA0CTL |= TACLR;

    /* Up mode. */
    TA0CTL |= MC_1;
}
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__ );

    /* Set all GPIO pins to output and low. */
    GPIO_setOutputLowOnPin( GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setOutputLowOnPin( GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setOutputLowOnPin( GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setOutputLowOnPin( GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setOutputLowOnPin( GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15 );
    GPIO_setAsOutputPin( GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setAsOutputPin( GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setAsOutputPin( GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setAsOutputPin( GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
    GPIO_setAsOutputPin( GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15 );

    /* Configure P2.0 - UCA0TXD and P2.1 - UCA0RXD. */
    GPIO_setOutputLowOnPin( GPIO_PORT_P2, GPIO_PIN0 );
    GPIO_setAsOutputPin( GPIO_PORT_P2, GPIO_PIN0 );
    GPIO_setAsPeripheralModuleFunctionInputPin( GPIO_PORT_P2, GPIO_PIN1, GPIO_SECONDARY_MODULE_FUNCTION );
    GPIO_setAsPeripheralModuleFunctionOutputPin( GPIO_PORT_P2, GPIO_PIN0, GPIO_SECONDARY_MODULE_FUNCTION );

    /* Set PJ.4 and PJ.5 for LFXT. */
    GPIO_setAsPeripheralModuleFunctionInputPin(  GPIO_PORT_PJ, GPIO_PIN4 + GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION  );

    /* Set DCO frequency to 8 MHz. */
    CS_setDCOFreq( CS_DCORSEL_0, CS_DCOFSEL_6 );

    /* Set external clock frequency to 32.768 KHz. */
    CS_setExternalClockSource( 32768, 0 );

    /* Set ACLK = LFXT. */
    CS_initClockSignal( CS_ACLK, CS_LFXTCLK_SELECT, CS_CLOCK_DIVIDER_1 );

    /* Set SMCLK = DCO with frequency divider of 1. */
    CS_initClockSignal( CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1 );

    /* Set MCLK = DCO with frequency divider of 1. */
    CS_initClockSignal( CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1 );

    /* Start XT1 with no time out. */
    CS_turnOnLFXT( CS_LFXT_DRIVE_0 );

    /* Disable the GPIO power-on default high-impedance mode. */
    PMM_unlockLPM5();
}
/*-----------------------------------------------------------*/

int _system_pre_init( void )
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__ );

    /* Return 1 for segments to be initialised. */
    return 1;
}


