/*
  * MODIFICATIONS TO KERNEL FILES
  * 
  * 1. Add to OS_TCB in ucos_ii.h:
  *    INT8U  compTime;   // Remaining computation time
  *    INT8U  period;     // Task period
  *
  * 2. Add to OSTimeTick() in os_core.c:
  *    // Decrement compTime for the current task
  *    if (OSTCBCur->compTime > 0) {
  *        OSTCBCur->compTime--;
  *    }
  *
  * 3. Add to OSIntExit() in os_core.c where task switching occurs:
  *    if (OSPrioHighRdy != OSPrioCur) {
  *        sprintf(MsgBuffer, "%5d Preempt     %3d      %3d", 
  *                (int)OSTimeGet(), (int)OSPrioCur, (int)OSPrioHighRdy);
  *        MsgReady = 1;
  *    }
  *
  * 4. Add to OS_Sched() in os_core.c where task switching occurs:
  *    if (OSPrioHighRdy != OSPrioCur) {
  *        sprintf(MsgBuffer, "%5d Complete    %3d      %3d", 
  *                (int)OSTimeGet(), (int)OSPrioCur, (int)OSPrioHighRdy);
  *        MsgReady = 1;
  *    }
  */