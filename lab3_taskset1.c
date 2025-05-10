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
  #define TASK_START_PRIO 0
  #define R1_PRIO         1
  #define R2_PRIO         2
  #define TASK1_PRIO      3   /* Highest priority - shortest period */
  #define TASK2_PRIO      4
  #define TASK3_PRIO      5   /* For task set 2 */
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
  //Task set for S1
  TASK_PARAM Task1Param = {6, 100};  /* t1(1,3) */
  TASK_PARAM Task2Param = {6, 150};  /* t2(3,6) */
  TASK_PARAM Task3Param = {9, 200};  /* t3(4,9) */
//  //Task set for S2
//  TASK_PARAM Task1Param = {11, 100};  /* t1(1,3) */
//  TASK_PARAM Task2Param = {12, 150};  /* t2(3,6) */
  INT32U TaskStartTime;
  /* Function prototypes */
  void TaskStart(void *pdata);
  void PeriodicTask1(void *pdata);
  void PeriodicTask2(void *pdata);
  void PeriodicTask3(void *pdata);
  OS_EVENT *R1_Mutex;
  OS_EVENT *R2_Mutex;
  INT8U    err;

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

      R1_Mutex = OSMutexCreate(1, &err);
	  if (err != OS_ERR_NONE) {
		  // 處理錯誤，例如印出錯誤訊息
		  printf("Error creating R1_Mutex: %d\n", err);
	  } else {
		  printf("R1_Mutex created with ceiling priority 1\n");
	  }

	  // 創建 R2 的 Mutex，設定其天花板優先級為 2
	  R2_Mutex = OSMutexCreate(2, &err);
	  if (err != OS_ERR_NONE) {
		  // 處理錯誤
		  printf("Error creating R2_Mutex: %d\n", err);
	  } else {
		  printf("R2_Mutex created with ceiling priority 2\n");
	  }
      /* Create application tasks (Task Set 1) */
      OSTaskCreateExt(PeriodicTask1,
                      (void *)&Task1Param,
                      (void *)&Task1Stk[TASK_STACKSIZE-1],
                      TASK1_PRIO,
                      TASK1_PRIO,
                      Task1Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);

      OSTaskCreateExt(PeriodicTask2,
                      (void *)&Task2Param,
                      (void *)&Task2Stk[TASK_STACKSIZE-1],
                      TASK2_PRIO,
                      TASK2_PRIO,
                      Task2Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);

      /* comment for Task Set 2 */

      OSTaskCreateExt(PeriodicTask3,
                      (void *)&Task3Param,
                      (void *)&Task3Stk[TASK_STACKSIZE-1],
                      TASK3_PRIO,
                      TASK3_PRIO,
                      Task3Stk,
                      TASK_STACKSIZE,
                      NULL,
                      0);

      /* Display header */
      printf("\nTime  Event       [From]   [To]\n");
      printf("---------------------------------\n");

      TaskStartTime=OSTimeGet();
      /* Reset system time */
      /* Delete self */
      OSTimeSet(0);
      OSTaskDel(OS_PRIO_SELF);
  }

  /* Periodic task implementation */

  void PeriodicTask1(void *pdata)
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
      OSTimeDly(8);
      int should_Run_R1=1;
      int should_Run_R2=1;
      while(1) {
          /* Consume CPU for c ticks */
          //printf("Task %d: high proity %3d\n ",(int)OSTCBCur->OSTCBPrio,(int)OSPrioHighRdy);
          while(((int)((OSTCBCur->period) - (OSTimeGet() - start))>0)&& OSTCBCur->compTime > 0 ) {
              //printf("[hello] Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
              /* Do nothing, just consume CPU time */
        	  if (OSTCBCur->compTime == 4 && should_Run_R1){
        		  OSMutexPend(R1_Mutex,1000,&err);
        		  should_Run_R1=0;
        	  }else if (OSTCBCur->compTime == 2 && should_Run_R2){
        		  OSMutexPend(R2_Mutex,1000,&err);
        		  should_Run_R2=0;
        	  }
          }
          //Finish resource
          OSMutexPost(R2_Mutex);
          OSMutexPost(R1_Mutex);

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

  void PeriodicTask2(void *pdata)
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
      OSTimeDly(4);
      int should_Run_R2=1;
      while(1) {
          /* Consume CPU for c ticks */
          //printf("Task %d: high proity %3d\n ",(int)OSTCBCur->OSTCBPrio,(int)OSPrioHighRdy);
          while(((int)((OSTCBCur->period) - (OSTimeGet() - start))>0)&& OSTCBCur->compTime > 0 ) {
              //printf("[hello] Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
              /* Do nothing, just consume CPU time */
        	  if (OSTCBCur->compTime == 4 && should_Run_R2){
        		  OSMutexPend(R2_Mutex,1000,&err);
				  should_Run_R2=0;
			  }
          }
          OSMutexPost(R2_Mutex);

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


  void PeriodicTask3(void *pdata)
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
      OSTimeSet(0);
      while(1) {
          /* Consume CPU for c ticks */
          //printf("Task %d: high proity %3d\n ",(int)OSTCBCur->OSTCBPrio,(int)OSPrioHighRdy);
    	  int should_Run_R1=1;
          while(((int)((OSTCBCur->period) - (OSTimeGet() - start))>0)&& OSTCBCur->compTime > 0 ) {
              //printf("[hello] Task %d: compTime = %d\n", (int)OSTCBCur->OSTCBPrio, (int)OSTCBCur->compTime);
              /* Do nothing, just consume CPU time */
        	  if (OSTCBCur->compTime == 7 && should_Run_R1 ){
        		//   printf("%5d Task3 Enter & Lock R1 Mutex\n",(int)OSTimeGet());
        		  OSMutexPend(R1_Mutex,1000,&err);
				  should_Run_R1=0;
        	  }
          }
		  OSMutexPost(R1_Mutex);
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


