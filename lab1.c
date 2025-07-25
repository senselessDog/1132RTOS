/*
 * Lab 1 for uC/OS-II: Periodic Task Emulation
 * Implements periodic tasks and displays context switches
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include "includes.h"
 
  /* Task stack sizes */
  #define TASK_STACKSIZE 4096
 
  /* Task priorities (using Rate Monotonic Scheduling: shorter period = higher priority) */
  #define TASK_START_PRIO 5
  #define TASK1_PRIO      6   /* Highest priority - shortest period */
  #define TASK2_PRIO      7
  #define TASK3_PRIO      8   /* For task set 2 */
  #define PRINT_TASK_PRIO 10  /* Lowest priority for print task */
 
  /* Global variables */
  OS_STK TaskStartStk[TASK_STACKSIZE];
  OS_STK Task1Stk[TASK_STACKSIZE];
  OS_STK Task2Stk[TASK_STACKSIZE];
  OS_STK Task3Stk[TASK_STACKSIZE];
  OS_STK PrintTaskStk[TASK_STACKSIZE];
 
  /* Message buffer for event output */
 // #define MSG_BUF_SIZE 100
 // char MsgBuffer[MSG_BUF_SIZE];
 // INT8U MsgReady = 0;
  //way 2
 #define MAX_MSG_QUEUE 100
 #define MSG_BUF_SIZE 100
 
 char MsgQueue[MAX_MSG_QUEUE][MSG_BUF_SIZE];  // 消息隊列
 INT8U MsgQueueIn = 0;                       // 寫入位置
 INT8U MsgQueueOut = 0;                      // 讀取位置
 INT8U MsgCount = 0;                         // 當前消息數量
 
  /* Task parameters (computation time, period) */
  typedef struct {
      INT8U c;    /* Computation time in ticks */
      INT8U p;    /* Period in ticks */
  } TASK_PARAM;
 
  TASK_PARAM Task1Param = {1, 3};  /* t1(1,3) */
  TASK_PARAM Task2Param = {3, 6};  /* t2(3,6) */
  TASK_PARAM Task3Param = {4, 9};  /* t3(4,9) */
  INT32U TaskStartTime;
  /* Function prototypes */
  void TaskStart(void *pdata);
  void PeriodicTask(void *pdata);
  void PrintTask(void *pdata);
 
  /* Main function */
  int main(void)
  {
      OSInit();                                  /* Initialize uC/OS-II */
 
      /* Create the startup task */
      OSTaskCreateExt(TaskStart,
                      NULL,
                      (void *)&TaskStartStk[TASK_STACKSIZE-1],
                      TASK_START_PRIO,
                      TASK_START_PRIO,
                      TaskStartStk,
                      TASK_STACKSIZE,
                      NULL,
                      0);
 
      OSStart();                                /* Start multitasking */
      return 0;
  }
  void AddMessageToQueue(const char *msg) {
     #if OS_CRITICAL_METHOD == 3  /* 為 CPU 狀態寄存器分配存儲空間 */
     OS_CPU_SR cpu_sr = 0;
     #endif
      OS_ENTER_CRITICAL();
      if (MsgCount < MAX_MSG_QUEUE) {
           strcpy(MsgQueue[MsgQueueIn], msg);
           MsgQueueIn = (MsgQueueIn + 1) % MAX_MSG_QUEUE;
           MsgCount++;
      }
      OS_EXIT_CRITICAL();
  }
  /* Startup task */
  void TaskStart(void *pdata)
  {
      /* Initialize system */
      OSTimeSet(0);                            /* Reset system time */
 
      /* Create application tasks (Task Set 1) */
      OSTaskCreateExt(PeriodicTask,
                      (void *)&Task1Param,
                      (void *)&Task1Stk[TASK_STACKSIZE-1],
                      TASK1_PRIO,
                      TASK1_PRIO,
                      Task1Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);
 
      OSTaskCreateExt(PeriodicTask,
                      (void *)&Task2Param,
                      (void *)&Task2Stk[TASK_STACKSIZE-1],
                      TASK2_PRIO,
                      TASK2_PRIO,
                      Task2Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);
 
      /* Uncomment for Task Set 2 */
 
      OSTaskCreateExt(PeriodicTask,
                      (void *)&Task3Param,
                      (void *)&Task3Stk[TASK_STACKSIZE-1],
                      TASK3_PRIO,
                      TASK3_PRIO,
                      Task3Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);
 
 //     /* Create print task */
 //     OSTaskCreateExt(PrintTask,
 //                     NULL,
 //                     (void *)&PrintTaskStk[TASK_STACKSIZE-1],
 //                     PRINT_TASK_PRIO,
 //                     PRINT_TASK_PRIO,
 //                     PrintTaskStk,
 //                     TASK_STACKSIZE,
 //                     NULL,
 //                     0);
 
      /* Display header */
      printf("\nTime  Event       [From]   [To]\n");
      printf("---------------------------------\n");
      OSTimeSet(0);
      TaskStartTime=OSTimeGet();
      /* Reset system time */
      /* Delete self */
      OSTaskDel(OS_PRIO_SELF);
  }
 
  /* Periodic task implementation */
  void PeriodicTask(void *pdata)
  {
      INT32U start;
      INT32U end;
      INT32U toDelay;
      TASK_PARAM *param;
 
      #if OS_CRITICAL_METHOD == 3
      OS_CPU_SR cpu_sr;
      #endif
 
 
      param = (TASK_PARAM *)pdata;
 
      /* Initialize task's compTime and period */
      OS_ENTER_CRITICAL();
      OSTCBCur->compTime = param->c;
      OSTCBCur->period = param->p;
      OS_EXIT_CRITICAL();
 
      start = (INT32U)((OSTimeGet() / OSTCBCur->period) * OSTCBCur->period+TaskStartTime);
      OS_ENTER_CRITICAL();
      OSTCBCur->deadline = (INT32U)(start + OSTCBCur->period);
      OS_EXIT_CRITICAL();
 
      while(1) {
          /* Consume CPU for c ticks */
          //printf("Task %d: high proity %3d\n ",(int)OSTCBCur->OSTCBPrio,(int)OSPrioHighRdy);
          while(((int)((OSTCBCur->period) - (OSTimeGet() - start))>0)&& OSTCBCur->compTime > 0 ) {
              //printf("[hello] Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
              /* Do nothing, just consume CPU time */
          }
 
          /* Calculate end time and delay for next period */
          end = OSTimeGet();
          toDelay = (OSTCBCur->period) - (end - start);
 
          /* Calculate start of next period */
          start = start + (OSTCBCur->period);
          //printf("Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
          /* Reset computation time counter */
          OS_ENTER_CRITICAL();
          OSTCBCur->compTime = param->c;
 
 //         char tempBuf[MSG_BUF_SIZE];
 //         sprintf(tempBuf,"Task %d: deadline = %5d\n",  (int)OSTCBCur->OSTCBPrio,(int)OSTCBCur->deadline);
 //		 AddMessageToQueue(tempBuf);
          OS_EXIT_CRITICAL();
 
          OS_ENTER_CRITICAL();
          while (MsgCount > 0) {
              printf("%s", MsgQueue[MsgQueueOut]);
              MsgQueueOut = (MsgQueueOut + 1) % MAX_MSG_QUEUE;
              MsgCount--;
          }
          OS_EXIT_CRITICAL();
 //		 if ((int)OSTCBCur->OSTCBPrio==8){
 //			 //printf("Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
 //			 char tempBuf[MSG_BUF_SIZE];
 //			 //sprintf(tempBuf,"Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
 //			 sprintf(tempBuf,"Task %d: dly = %d\n",  (int)OSTCBCur->OSTCBPrio,(int)toDelay);
 //			 AddMessageToQueue(tempBuf);
 //		  }
 
          /* Delay until next period */
          OSTimeSet(end);
          if (((int)toDelay) > 0) {
              OS_ENTER_CRITICAL();
              OSTCBCur->deadline += (INT32U)OSTCBCur->period;
              OS_EXIT_CRITICAL();
              OSTimeDly(toDelay);
 
          } else {
              /* Deadline violation occurred */
              //printf("%5d Deadline Violation for Task %d\n", (int)OSTimeGet(), (int)OSTCBCur->OSTCBPrio);
 //             char tempBuf[MSG_BUF_SIZE];
 //			 sprintf(tempBuf, "%5d Deadline Violation for Task %d\n", (int)OSTimeGet(), (int)OSTCBCur->OSTCBPrio);
 //			 AddMessageToQueue(tempBuf);
 
              /* Reset for next period */
              // 如果出現截止期違例，重新調整週期計數
              int periodCount = OSTimeGet() / OSTCBCur->period;
              start = periodCount * OSTCBCur->period;
          }
      }
  }
 
 // /* Print task implementation */
 // void PrintTask(void *pdata)
 // {
 //	while(1) {
 //		#if OS_CRITICAL_METHOD == 3
 //		OS_CPU_SR cpu_sr = 0;
 //		#endif
 //		OS_ENTER_CRITICAL();
 //		if (MsgCount > 0) {
 //			printf("%s\n", MsgQueue[MsgQueueOut]);
 //			MsgQueueOut = (MsgQueueOut + 1) % MAX_MSG_QUEUE;
 //			MsgCount--;
 //		}
 ////		if (MsgReady==1){
 ////			 printf("%s\n", MsgBuffer);
 ////			 MsgReady = 0;
 ////		}
 //		OS_EXIT_CRITICAL();
 //		//OSTimeDly(1);
 //	}
 // }
 
 