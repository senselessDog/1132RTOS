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
 * NOTE 1:  This project provides two demo applications.  A simple blinky
 * style project, and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting in main.c is used to select
 * between the two.  See the notes on using mainCREATE_SIMPLE_BLINKY_DEMO_ONLY
 * in main.c.  This file implements the simply blinky style version.
 *
 * NOTE 2:  This file only contains the source code that is specific to the
 * basic demo.  Generic functions, such FreeRTOS hook functions, and functions
 * required to configure the hardware are defined in main.c.
 ******************************************************************************
 *
 * main_blinky() creates one queue, and two tasks.  It then starts the
 * scheduler.
 *
 * The Queue Send Task:
 * The queue send task is implemented by the prvQueueSendTask() function in
 * this file.  prvQueueSendTask() sits in a loop that causes it to repeatedly
 * block for 200 milliseconds, before sending the value 100 to the queue that
 * was created within main_blinky().  Once the value is sent, the task loops
 * back around to block for another 200 milliseconds...and so on.
 *
 * The Queue Receive Task:
 * The queue receive task is implemented by the prvQueueReceiveTask() function
 * in this file.  prvQueueReceiveTask() sits in a loop where it repeatedly
 * blocks on attempts to read data from the queue that was created within
 * main_blinky().  When data is received, the task checks the value of the
 * data, and if the value equals the expected 100, toggles an LED.  The 'block
 * time' parameter passed to the queue receive function specifies that the
 * task should be held in the Blocked state indefinitely to wait for data to
 * be available on the queue.  The queue receive task will only leave the
 * Blocked state when the queue send task writes to the queue.  As the queue
 * send task writes to the queue every 200 milliseconds, the queue receive
 * task leaves the Blocked state every 200 milliseconds, and therefore toggles
 * the LED every 200 milliseconds.
 */

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
 #include <stdio.h>
 #include <stdlib.h>
/* Standard demo includes. */
#include "partest.h"

#define TASK_SET_1 1
#define TASK_SET_2 2
#define CURRENT_TASK_SET TASK_SET_1 // 修改此處以選擇不同的任務集

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	mainQUEUE_SEND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue.  The 200ms value is converted
to ticks using the portTICK_PERIOD_MS constant. */
#define mainQUEUE_SEND_FREQUENCY_MS			( pdMS_TO_TICKS( 200 ) )

/* The number of items the queue can hold.  This is 1 as the receive task
will remove items as they are added, meaning the send task should always find
the queue empty. */
#define mainQUEUE_LENGTH					( 1 )

/* The LED toggled by the Rx task. */
#define mainTASK_LED						( 0 )

/*-----------------------------------------------------------*/

/*
 * Called by main when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1 in
 * main.c.
 */
void main_blinky( void );

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvQueueReceiveTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/*-----------------------------------------------------------*/


void main_blinky( void )
{
	/* Create the queue. */
	xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint32_t ) );

	if( xQueue != NULL )
	{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvQueueReceiveTask,				/* The function that implements the task. */
					"Rx", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					NULL, 								/* The parameter passed to the task - not used in this case. */
					mainQUEUE_RECEIVE_TASK_PRIORITY, 	/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */

		xTaskCreate( prvQueueSendTask, "TX", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY, NULL );

		/* Start the tasks and timer running. */
		vTaskStartScheduler();
	}

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the Idle and/or
	timer tasks to be created.  See the memory management section on the
	FreeRTOS web site for more details on the FreeRTOS heap
	http://www.freertos.org/a00111.html. */
	for( ;; );
}
/*-----------------------------------------------------------*/

static void prvQueueSendTask( void *pvParameters )
{
TickType_t xNextWakeTime;
const unsigned long ulValueToSend = 100UL;

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again. */
		vTaskDelayUntil( &xNextWakeTime, mainQUEUE_SEND_FREQUENCY_MS );

		/* Send to the queue - causing the queue receive task to unblock and
		toggle the LED.  0 is used as the block time so the sending operation
		will not block - it shouldn't need to block as the queue should always
		be empty at this point in the code. */
		xQueueSend( xQueue, &ulValueToSend, 0U );
	}
}
/*-----------------------------------------------------------*/

static void prvQueueReceiveTask( void *pvParameters )
{
unsigned long ulReceivedValue;
const unsigned long ulExpectedValue = 100UL;

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

	for( ;; )
	{
		/* Wait until something arrives in the queue - this task will block
		indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
		FreeRTOSConfig.h. */
		xQueueReceive( xQueue, &ulReceivedValue, portMAX_DELAY );

		/*  To get here something must have been received from the queue, but
		is it the expected value?  If it is, toggle the LED. */
		if( ulReceivedValue == ulExpectedValue )
		{
			vParTestToggleLED( mainTASK_LED );
			ulReceivedValue = 0U;
		}
	}
}
typedef struct tskTaskControlBlock       /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
    volatile StackType_t * pxTopOfStack; /*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */

    #if ( portUSING_MPU_WRAPPERS == 1 )
        xMPU_SETTINGS xMPUSettings; /*< The MPU settings are defined as part of the port layer.  THIS MUST BE THE SECOND MEMBER OF THE TCB STRUCT. */
    #endif

    ListItem_t xStateListItem;                  /*< The list that the state list item of a task is reference from denotes the state of that task (Ready, Blocked, Suspended ). */
    ListItem_t xEventListItem;                  /*< Used to reference a task from an event list. */
    UBaseType_t uxPriority;                     /*< The priority of the task.  0 is the lowest priority. */
    StackType_t * pxStack;                      /*< Points to the start of the stack. */
    char pcTaskName[ configMAX_TASK_NAME_LEN ]; /*< Descriptive name given to the task when created.  Facilitates debugging only. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

    #if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
        StackType_t * pxEndOfStack; /*< Points to the highest valid address for the stack. */
    #endif

    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
        UBaseType_t uxCriticalNesting; /*< Holds the critical section nesting depth for ports that do not maintain their own count in the port layer. */
    #endif

    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTCBNumber;  /*< Stores a number that increments each time a TCB is created.  It allows debuggers to determine when a task has been deleted and then recreated. */
        UBaseType_t uxTaskNumber; /*< Stores a number specifically for use by third party trace code. */
    #endif

    #if ( configUSE_MUTEXES == 1 )
        UBaseType_t uxBasePriority; /*< The priority last assigned to the task - used by the priority inheritance mechanism. */
        UBaseType_t uxMutexesHeld;
    #endif

    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
        TaskHookFunction_t pxTaskTag;
    #endif

    #if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
        void * pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
    #endif

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
        uint32_t ulRunTimeCounter; /*< Stores the amount of time the task has spent in the Running state. */
    #endif

    #if ( configUSE_NEWLIB_REENTRANT == 1 )
        /* Allocate a Newlib reent structure that is specific to this task.
         * Note Newlib support has been included by popular demand, but is not
         * used by the FreeRTOS maintainers themselves.  FreeRTOS is not
         * responsible for resulting newlib operation.  User must be familiar with
         * newlib and must provide system-wide implementations of the necessary
         * stubs. Be warned that (at the time of writing) the current newlib design
         * implements a system-wide malloc() that must be provided with locks.
         *
         * See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
         * for additional information. */
        struct  _reent xNewLib_reent;
    #endif

    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
        volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
    #endif

    /* See the comments in FreeRTOS.h with the definition of
     * tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
    #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 Macro has been consolidated for readability reasons. */
        uint8_t ucStaticallyAllocated;                     /*< Set to pdTRUE if the task is a statically allocated to ensure no attempt is made to free the memory. */
    #endif

    #if ( INCLUDE_xTaskAbortDelay == 1 )
        uint8_t ucDelayAborted;
    #endif

    #if ( configUSE_POSIX_ERRNO == 1 )
        int iTaskErrno;
    #endif
    /* ---- EDF Scheduler Additions ---- */
    TickType_t xAbsoluteDeadline;   /*< 任務的絕對截止時間 */
    TickType_t xPeriod;             /*< 任務的週期 (用於更新截止時間) */
    TickType_t xExecutionTime;      /*< 任務的執行時間 (用於記錄或未來擴展) */
    TickType_t xRemainingExecutionTime;
    /* ---- End of EDF Scheduler Additions ---- */
} tskTCB;
typedef tskTCB TCB_t; // 現在 main_blinky.c 知道了 TCB_t 的結構
extern TCB_t * volatile pxCurrentTCB;
#define MAX_MSG_QUEUE 20
#define MSG_BUF_SIZE 100

char MsgQueue[MAX_MSG_QUEUE][MSG_BUF_SIZE];  // 消息隊列
uint8_t  MsgQueueIn = 0;                       // 寫入位置
uint8_t  MsgQueueOut = 0;                      // 讀取位置
uint8_t  MsgCount = 0;                         // 當前消息數量
void AddMessageToQueue(const char *msg) {
    if (MsgCount < MAX_MSG_QUEUE) {
         strcpy(MsgQueue[MsgQueueIn], msg);
         MsgQueueIn = (MsgQueueIn + 1) % MAX_MSG_QUEUE;
         MsgCount++;
    }
}
typedef struct {
    char pcTaskName[configMAX_TASK_NAME_LEN];
    TickType_t xExecutionTime; // 執行時間 (C)
    TickType_t xPeriod;        // 週期 (P) / 相對截止時間
    TickType_t xLastWakeTime;
} EDFTaskParameters_t;
/*-----------------------------------------------------------*/
//TickType_t xLastWakeTime1 = 0;
//TickType_t xLastWakeTime2 = 0;
static void prvEDFTask(void *pvParameters) {
    EDFTaskParameters_t *pxTaskParams = (EDFTaskParameters_t *)pvParameters;
//    TickType_t xLastWakeTime;                 // 用於 vTaskDelayUntil，標記週期的開始
    TickType_t xStartOfComputationTick;       // 記錄本週期計算階段開始時的 xTickCount
    TickType_t xEndOfComputationTick;
//    pxCurrentTCB->pcTaskName=pxTaskParams->pcTaskName;
    pxCurrentTCB->xAbsoluteDeadline=pxTaskParams->xPeriod;
//    xLastWakeTime = xTaskGetTickCount();

    // 初始的絕對截止時間應由修改後的 xTaskCreate 設置到 TCB 中

    for (;;) {
        // 記錄本週期計算階段開始的系統時間
        taskENTER_CRITICAL();
        pxCurrentTCB->xRemainingExecutionTime=pxTaskParams->xExecutionTime;
        xStartOfComputationTick = xTaskGetTickCount();
        printf("%d start %s\n", (unsigned int)xStartOfComputationTick, pxTaskParams->pcTaskName);
//        printf("%s xRemainingxAbsoluteDeadline %d\n",  pxTaskParams->pcTaskName,(int)pxCurrentTCB->xAbsoluteDeadline);
        taskEXIT_CRITICAL();
        // 1. 使用 while 迴圈模擬執行 C 個 tick 的計算量
        //    任務在此迴圈中保持運行 (除非被更高優先權的 EDF 任務搶佔)，
        //    直到 xTickCount 相對於 xStartOfComputationTick 前進了 C 個單位。
        //    如果 xExecutionTime 為 0，此迴圈不會執行。
        while (pxCurrentTCB->xRemainingExecutionTime > 0) {
//            taskENTER_CRITICAL();
//            printf("%s xRemainingExecutionTime2 %d\n",  pxTaskParams->pcTaskName,(unsigned int)pxCurrentTCB->xRemainingExecutionTime);
//            taskEXIT_CRITICAL();
            // 這裡的內容可以是：
            // a) 空迴圈 (純粹的忙等待，消耗 CPU 時間)
            // b) 執行任務的實際工作單元
            // c) 為了確保系統在極端情況下不死鎖（例如 C 非常大且沒有搶佔），
            //    可以考慮加入一個非常短暫的 yield，但這會稍微改變純粹 "忙等C個tick" 的語義。
            //    但在一個搶佔式 EDF 排程器下，這個 yield 不是必需的，因為排程器會負責搶佔。
            //    我們這裡假設它是忙等待或執行實際工作。
        }
        taskENTER_CRITICAL();
        // 2. 打印完成訊息
        //    此時的 xTickCount 理論上應該是 xStartOfComputationTick + pxTaskParams->xExecutionTime
        //    (如果沒有被長時間搶佔，且 C > 0)
        printf("%d complete %s\n", (unsigned int)xTaskGetTickCount(), pxTaskParams->pcTaskName);
        // 3. 更新下一個絕對截止時間 (EDF 關鍵步驟)
        xEndOfComputationTick = xTaskGetTickCount();
//        printf("help\n");
        while (MsgCount > 0) {
            printf(MsgQueue[MsgQueueOut]);
//            printf("help2\n");
            MsgQueueOut = (MsgQueueOut + 1) % MAX_MSG_QUEUE;
            MsgCount--;
        }
        taskEXIT_CRITICAL();

        // 4. 延遲到下一個週期開始
        pxCurrentTCB->xAbsoluteDeadline+=pxTaskParams->xPeriod;
        if (pxTaskParams->xPeriod-(xEndOfComputationTick-xStartOfComputationTick)>0){
            xTaskDelayUntil(&pxTaskParams->xLastWakeTime, pxTaskParams->xPeriod);
        }
    }
}
void main_lab(void) {
    printf("EDF Lab - Main Application - Task Set %d\n", CURRENT_TASK_SET);

#if CURRENT_TASK_SET == TASK_SET_1
    // 任務集 1: t1(1,3), t2(3,5) [cite: 4]
    static EDFTaskParameters_t xTask1Params = { .pcTaskName = "T1", .xExecutionTime = 1, .xPeriod = 3 ,.xLastWakeTime=0};
    static EDFTaskParameters_t xTask2Params = { .pcTaskName = "T2", .xExecutionTime = 3, .xPeriod = 5 ,.xLastWakeTime=0 };

    xTaskCreate(prvEDFTask, xTask1Params.pcTaskName, configMINIMAL_STACK_SIZE, &xTask1Params, tskIDLE_PRIORITY + 2,NULL);
    xTaskCreate(prvEDFTask, xTask2Params.pcTaskName, configMINIMAL_STACK_SIZE, &xTask2Params, tskIDLE_PRIORITY + 1,NULL);

#elif CURRENT_TASK_SET == TASK_SET_2
    // 任務集 2: t1(1,4), t2(2,5), t3(2,10) [cite: 4]
    static EDFTaskParameters_t xTask1Params_set2 = { .pcTaskName = "T1", .xExecutionTime = 1, .xPeriod = 4 };
    static EDFTaskParameters_t xTask2Params_set2 = { .pcTaskName = "T2", .xExecutionTime = 2, .xPeriod = 5 };
    static EDFTaskParameters_t xTask3Params_set2 = { .pcTaskName = "T3", .xExecutionTime = 2, .xPeriod = 10 };

    xTaskCreate(prvEDFTask, xTask1Params_set2.pcTaskName, configMINIMAL_STACK_SIZE, &xTask1Params_set2, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(prvEDFTask, xTask2Params_set2.pcTaskName, configMINIMAL_STACK_SIZE, &xTask2Params_set2, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(prvEDFTask, xTask3Params_set2.pcTaskName, configMINIMAL_STACK_SIZE, &xTask3Params_set2, tskIDLE_PRIORITY + 1, NULL);
#endif

    vTaskStartScheduler();

    for (;;); // 程式不應執行到此處
}

