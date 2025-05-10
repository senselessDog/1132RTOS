/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                  MUTUAL EXCLUSION SEMAPHORE MANAGEMENT
*
*                              (c) Copyright 1992-2007, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_MUTEX.C
* By      : Jean J. Labrosse
* Version : V2.86
*
* LICENSING TERMS:
* ---------------
*   uC/OS-II is provided in source form for FREE evaluation, for educational use or for peaceful research.  
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micri痠 to properly license 
* its use in your product. We provide ALL the source code for your convenience and to help you experience 
* uC/OS-II.   The fact that the  source is provided does  NOT  mean that you can use it without  paying a 
* licensing fee.
*********************************************************************************************************
*/

#ifndef  OS_MASTER_FILE
#include <ucos_ii.h>
#endif

extern char MsgBuffer[100];
#define MSG_BUF_SIZE 100
#if OS_MUTEX_EN > 0
/*
*********************************************************************************************************
*                                            LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  OS_MUTEX_KEEP_LOWER_8   ((INT16U)0x00FFu)
#define  OS_MUTEX_KEEP_UPPER_8   ((INT16U)0xFF00u)

#define  OS_MUTEX_AVAILABLE      ((INT16U)0x00FFu)

/*
*********************************************************************************************************
*                                            LOCAL CONSTANTS
*********************************************************************************************************
*/

static  void  OSMutex_RdyAtPrio(OS_TCB *ptcb, INT8U prio);

/*$PAGE*/
/*
*********************************************************************************************************
*                                   ACCEPT MUTUAL EXCLUSION SEMAPHORE
*
* Description: This  function checks the mutual exclusion semaphore to see if a resource is available.
*              Unlike OSMutexPend(), OSMutexAccept() does not suspend the calling task if the resource is
*              not available or the event did not occur.
*
* Arguments  : pevent     is a pointer to the event control block
*
*              perr       is a pointer to an error code which will be returned to your application:
*                            OS_ERR_NONE         if the call was successful.
*                            OS_ERR_EVENT_TYPE   if 'pevent' is not a pointer to a mutex
*                            OS_ERR_PEVENT_NULL  'pevent' is a NULL pointer
*                            OS_ERR_PEND_ISR     if you called this function from an ISR
*                            OS_ERR_PIP_LOWER    If the priority of the task that owns the Mutex is
*                                                HIGHER (i.e. a lower number) than the PIP.  This error
*                                                indicates that you did not set the PIP higher (lower
*                                                number) than ALL the tasks that compete for the Mutex.
*                                                Unfortunately, this is something that could not be
*                                                detected when the Mutex is created because we don't know
*                                                what tasks will be using the Mutex.
*
* Returns    : == OS_TRUE    if the resource is available, the mutual exclusion semaphore is acquired
*              == OS_FALSE   a) if the resource is not available
*                            b) you didn't pass a pointer to a mutual exclusion semaphore
*                            c) you called this function from an ISR
*
* Warning(s) : This function CANNOT be called from an ISR because mutual exclusion semaphores are
*              intended to be used by tasks only.
*********************************************************************************************************
*/

#if OS_MUTEX_ACCEPT_EN > 0
BOOLEAN  OSMutexAccept (OS_EVENT *pevent, INT8U *perr)
{
    INT8U      pip;                                    /* Priority Inheritance Priority (PIP)          */
#if OS_CRITICAL_METHOD == 3                            /* Allocate storage for CPU status register     */
    OS_CPU_SR  cpu_sr = 0;
#endif



#if OS_ARG_CHK_EN > 0
    if (perr == (INT8U *)0) {                          /* Validate 'perr'                              */
        return (OS_FALSE);
    }
    if (pevent == (OS_EVENT *)0) {                     /* Validate 'pevent'                            */
        *perr = OS_ERR_PEVENT_NULL;
        return (OS_FALSE);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {  /* Validate event block type                    */
        *perr = OS_ERR_EVENT_TYPE;
        return (OS_FALSE);
    }
    if (OSIntNesting > 0) {                            /* Make sure it's not called from an ISR        */
        *perr = OS_ERR_PEND_ISR;
        return (OS_FALSE);
    }
    OS_ENTER_CRITICAL();                               /* Get value (0 or 1) of Mutex                  */
    pip = (INT8U)(pevent->OSEventCnt >> 8);            /* Get PIP from mutex                           */
    if ((pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;   /*      Mask off LSByte (Acquire Mutex)         */
        pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;     /*      Save current task priority in LSByte    */
        pevent->OSEventPtr  = (void *)OSTCBCur;        /*      Link TCB of task owning Mutex           */
        if (OSTCBCur->OSTCBPrio <= pip) {              /*      PIP 'must' have a SMALLER prio ...      */
            OS_EXIT_CRITICAL();                        /*      ... than current task!                  */
            *perr = OS_ERR_PIP_LOWER;
        } else {
            OS_EXIT_CRITICAL();
            *perr = OS_ERR_NONE;
        }
        return (OS_TRUE);
    }
    OS_EXIT_CRITICAL();
    *perr = OS_ERR_NONE;
    return (OS_FALSE);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                  CREATE A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function creates a mutual exclusion semaphore.
*
* Arguments  : prio          is the priority to use when accessing the mutual exclusion semaphore.  In
*                            other words, when the semaphore is acquired and a higher priority task
*                            attempts to obtain the semaphore then the priority of the task owning the
*                            semaphore is raised to this priority.  It is assumed that you will specify
*                            a priority that is LOWER in value than ANY of the tasks competing for the
*                            mutex.
*
*              perr          is a pointer to an error code which will be returned to your application:
*                               OS_ERR_NONE         if the call was successful.
*                               OS_ERR_CREATE_ISR   if you attempted to create a MUTEX from an ISR
*                               OS_ERR_PRIO_EXIST   if a task at the priority inheritance priority
*                                                   already exist.
*                               OS_ERR_PEVENT_NULL  No more event control blocks available.
*                               OS_ERR_PRIO_INVALID if the priority you specify is higher that the
*                                                   maximum allowed (i.e. > OS_LOWEST_PRIO)
*
* Returns    : != (void *)0  is a pointer to the event control clock (OS_EVENT) associated with the
*                            created mutex.
*              == (void *)0  if an error is detected.
*
* Note(s)    : 1) The LEAST significant 8 bits of '.OSEventCnt' are used to hold the priority number
*                 of the task owning the mutex or 0xFF if no task owns the mutex.
*
*              2) The MOST  significant 8 bits of '.OSEventCnt' are used to hold the priority number
*                 to use to reduce priority inversion.
*********************************************************************************************************
*/

OS_EVENT  *OSMutexCreate (INT8U prio, INT8U *perr)
{
    OS_EVENT  *pevent;
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr = 0;
#endif



#if OS_ARG_CHK_EN > 0
    if (perr == (INT8U *)0) {                              /* Validate 'perr'                          */
        return ((OS_EVENT *)0);
    }
    if (prio >= OS_LOWEST_PRIO) {                          /* Validate PIP                             */
        *perr = OS_ERR_PRIO_INVALID;
        return ((OS_EVENT *)0);
    }
#endif
    if (OSIntNesting > 0) {                                /* See if called from ISR ...               */
        *perr = OS_ERR_CREATE_ISR;                         /* ... can't CREATE mutex from an ISR       */
        return ((OS_EVENT *)0);
    }
    OS_ENTER_CRITICAL();
    if (OSTCBPrioTbl[prio] != (OS_TCB *)0) {               /* Mutex priority must not already exist    */
        OS_EXIT_CRITICAL();                                /* Task already exist at priority ...       */
        *perr = OS_ERR_PRIO_EXIST;                         /* ... inheritance priority                 */
        return ((OS_EVENT *)0);
    }
    OSTCBPrioTbl[prio] = OS_TCB_RESERVED;                  /* Reserve the table entry                  */
    pevent             = OSEventFreeList;                  /* Get next free event control block        */
    if (pevent == (OS_EVENT *)0) {                         /* See if an ECB was available              */
        OSTCBPrioTbl[prio] = (OS_TCB *)0;                  /* No, Release the table entry              */
        OS_EXIT_CRITICAL();
        *perr              = OS_ERR_PEVENT_NULL;           /* No more event control blocks             */
        return (pevent);
    }
    OSEventFreeList        = (OS_EVENT *)OSEventFreeList->OSEventPtr;   /* Adjust the free list        */
    OS_EXIT_CRITICAL();
    pevent->OSEventType    = OS_EVENT_TYPE_MUTEX;
    pevent->OSEventCnt     = (INT16U)((INT16U)prio << 8) | OS_MUTEX_AVAILABLE; /* Resource is avail.   */
    pevent->OSEventPtr     = (void *)0;                                 /* No task owning the mutex    */
#if OS_EVENT_NAME_SIZE > 1
    pevent->OSEventName[0] = '?';
    pevent->OSEventName[1] = OS_ASCII_NUL;
#endif
    OS_EventWaitListInit(pevent);
    *perr                  = OS_ERR_NONE;
    return (pevent);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          DELETE A MUTEX
*
* Description: This function deletes a mutual exclusion semaphore and readies all tasks pending on the it.
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mutex.
*
*              opt           determines delete options as follows:
*                            opt == OS_DEL_NO_PEND   Delete mutex ONLY if no task pending
*                            opt == OS_DEL_ALWAYS    Deletes the mutex even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE             The call was successful and the mutex was deleted
*                            OS_ERR_DEL_ISR          If you attempted to delete the MUTEX from an ISR
*                            OS_ERR_INVALID_OPT      An invalid option was specified
*                            OS_ERR_TASK_WAITING     One or more tasks were waiting on the mutex
*                            OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mutex
*                            OS_ERR_PEVENT_NULL      If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error
*              (OS_EVENT *)0 if the mutex was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the mutex MUST check the return code of OSMutexPend().
*
*              2) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the mutex.
*
*              3) Because ALL tasks pending on the mutex will be readied, you MUST be careful because the
*                 resource(s) will no longer be guarded by the mutex.
*
*              4) IMPORTANT: In the 'OS_DEL_ALWAYS' case, we assume that the owner of the Mutex (if there
*                            is one) is ready-to-run and is thus NOT pending on another kernel object or
*                            has delayed itself.  In other words, if a task owns the mutex being deleted,
*                            that task will be made ready-to-run at its original priority.
*********************************************************************************************************
*/

#if OS_MUTEX_DEL_EN
OS_EVENT  *OSMutexDel (OS_EVENT *pevent, INT8U opt, INT8U *perr)
{
    BOOLEAN    tasks_waiting;
    OS_EVENT  *pevent_return;
    INT8U      pip;                                        /* Priority inheritance priority            */
    INT8U      prio;
    OS_TCB    *ptcb;
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr = 0;
#endif



#if OS_ARG_CHK_EN > 0
    if (perr == (INT8U *)0) {                              /* Validate 'perr'                          */
        return (pevent);
    }
    if (pevent == (OS_EVENT *)0) {                         /* Validate 'pevent'                        */
        *perr = OS_ERR_PEVENT_NULL;
        return (pevent);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {      /* Validate event block type                */
        *perr = OS_ERR_EVENT_TYPE;
        return (pevent);
    }
    if (OSIntNesting > 0) {                                /* See if called from ISR ...               */
        *perr = OS_ERR_DEL_ISR;                             /* ... can't DELETE from an ISR             */
        return (pevent);
    }
    OS_ENTER_CRITICAL();
    if (pevent->OSEventGrp != 0) {                         /* See if any tasks waiting on mutex        */
        tasks_waiting = OS_TRUE;                           /* Yes                                      */
    } else {
        tasks_waiting = OS_FALSE;                          /* No                                       */
    }
    switch (opt) {
        case OS_DEL_NO_PEND:                               /* DELETE MUTEX ONLY IF NO TASK WAITING --- */
             if (tasks_waiting == OS_FALSE) {
#if OS_EVENT_NAME_SIZE > 1
                 pevent->OSEventName[0] = '?';             /* Unknown name                             */
                 pevent->OSEventName[1] = OS_ASCII_NUL;
#endif
                 pip                 = (INT8U)(pevent->OSEventCnt >> 8);
                 OSTCBPrioTbl[pip]   = (OS_TCB *)0;        /* Free up the PIP                          */
                 pevent->OSEventType = OS_EVENT_TYPE_UNUSED;
                 pevent->OSEventPtr  = OSEventFreeList;    /* Return Event Control Block to free list  */
                 pevent->OSEventCnt  = 0;
                 OSEventFreeList     = pevent;
                 OS_EXIT_CRITICAL();
                 *perr               = OS_ERR_NONE;
                 pevent_return       = (OS_EVENT *)0;      /* Mutex has been deleted                   */
             } else {
                 OS_EXIT_CRITICAL();
                 *perr               = OS_ERR_TASK_WAITING;
                 pevent_return       = pevent;
             }
             break;

        case OS_DEL_ALWAYS:                                /* ALWAYS DELETE THE MUTEX ---------------- */
             pip  = (INT8U)(pevent->OSEventCnt >> 8);                     /* Get PIP of mutex          */
             prio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);  /* Get owner's original prio */
             ptcb = (OS_TCB *)pevent->OSEventPtr;
             if (ptcb != (OS_TCB *)0) {                    /* See if any task owns the mutex           */
                 if (ptcb->OSTCBPrio == pip) {             /* See if original prio was changed         */
                     OSMutex_RdyAtPrio(ptcb, prio);        /* Yes, Restore the task's original prio    */
                 }
             }
             while (pevent->OSEventGrp != 0) {             /* Ready ALL tasks waiting for mutex        */
                 (void)OS_EventTaskRdy(pevent, (void *)0, OS_STAT_MUTEX, OS_STAT_PEND_OK);
             }
#if OS_EVENT_NAME_SIZE > 1
             pevent->OSEventName[0] = '?';                 /* Unknown name                             */
             pevent->OSEventName[1] = OS_ASCII_NUL;
#endif
             pip                 = (INT8U)(pevent->OSEventCnt >> 8);
             OSTCBPrioTbl[pip]   = (OS_TCB *)0;            /* Free up the PIP                          */
             pevent->OSEventType = OS_EVENT_TYPE_UNUSED;
             pevent->OSEventPtr  = OSEventFreeList;        /* Return Event Control Block to free list  */
             pevent->OSEventCnt  = 0;
             OSEventFreeList     = pevent;                 /* Get next free event control block        */
             OS_EXIT_CRITICAL();
             if (tasks_waiting == OS_TRUE) {               /* Reschedule only if task(s) were waiting  */
                 OS_Sched();                               /* Find highest priority task ready to run  */
             }
             *perr         = OS_ERR_NONE;
             pevent_return = (OS_EVENT *)0;                /* Mutex has been deleted                   */
             break;

        default:
             OS_EXIT_CRITICAL();
             *perr         = OS_ERR_INVALID_OPT;
             pevent_return = pevent;
             break;
    }
    return (pevent_return);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                  PEND ON MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function waits for a mutual exclusion semaphore.
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            mutex.
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for the resource up to the amount of time specified by this argument.
*                            If you specify 0, however, your task will wait forever at the specified
*                            mutex or, until the resource becomes available.
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*                               OS_ERR_NONE        The call was successful and your task owns the mutex
*                               OS_ERR_TIMEOUT     The mutex was not available within the specified 'timeout'.
*                               OS_ERR_PEND_ABORT  The wait on the mutex was aborted.
*                               OS_ERR_EVENT_TYPE  If you didn't pass a pointer to a mutex
*                               OS_ERR_PEVENT_NULL 'pevent' is a NULL pointer
*                               OS_ERR_PEND_ISR    If you called this function from an ISR and the result
*                                                  would lead to a suspension.
*                               OS_ERR_PIP_LOWER   If the priority of the task that owns the Mutex is
*                                                  HIGHER (i.e. a lower number) than the PIP.  This error
*                                                  indicates that you did not set the PIP higher (lower
*                                                  number) than ALL the tasks that compete for the Mutex.
*                                                  Unfortunately, this is something that could not be
*                                                  detected when the Mutex is created because we don't know
*                                                  what tasks will be using the Mutex.
*                               OS_ERR_PEND_LOCKED If you called this function when the scheduler is locked
*
* Returns    : none
*
* Note(s)    : 1) The task that owns the Mutex MUST NOT pend on any other event while it owns the mutex.
*
*              2) You MUST NOT change the priority of the task that owns the mutex
*********************************************************************************************************
*/

void OSMutexPend(OS_EVENT *pevent, INT16U timeout, INT8U *perr)
{
    INT8U      ceiling_prio;                             /* Ceiling Priority                          */
    INT8U      original_prio;                            /* Task's original priority                  */
    OS_TCB    *ptcb;
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr = 0;
#endif

    /* Initial validation checks */
#if OS_ARG_CHK_EN > 0
    if (perr == (INT8U *)0) {
        return;
    }
    if (pevent == (OS_EVENT *)0) {
        *perr = OS_ERR_PEVENT_NULL;
        return;
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {
        *perr = OS_ERR_EVENT_TYPE;
        return;
    }
    if (OSIntNesting > 0) {
        *perr = OS_ERR_PEND_ISR;
        return;
    }
    if (OSLockNesting > 0) {
        *perr = OS_ERR_PEND_LOCKED;
        return;
    }

    OS_ENTER_CRITICAL();
    ceiling_prio = (INT8U)(pevent->OSEventCnt >> 8);    /* Get ceiling priority from mutex           */

    /* Check if mutex is available */
    if ((INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {


    	/* Mutex is available, acquire it */
        original_prio = OSTCBCur->OSTCBPrio;            /* Store original priority                   */
        // find the highest priority
        int highest_ceiling_prio;
        if (original_prio<ceiling_prio){
        	highest_ceiling_prio=original_prio;
        }else{
        	highest_ceiling_prio=ceiling_prio;
        }
        char tempBuf[MSG_BUF_SIZE];
		sprintf(tempBuf, "%5d   lock  R%1d  (Prio=%3d change to=%3d)\n",
				(int)OSTimeGet(), (int)ceiling_prio,(int)original_prio, // 當前任務的優先級
				(int)highest_ceiling_prio); // 此 Mutex 的天花板優先級);
		AddMessageToQueue(tempBuf);
		if (ceiling_prio > OSPrioCur){
			OSPrioCur = ceiling_prio;
		}

        /* Store original priority in lower 8 bits for restoration later */
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;    /* Clear lower 8 bits                        */
        pevent->OSEventCnt |= original_prio;            /* Save task's original priority             */

        pevent->OSEventPtr = (void *)OSTCBCur;          /* Point to owning task's TCB                */

        /* In CPP, immediately elevate task to ceiling priority */
        if (OSTCBCur->OSTCBPrio > ceiling_prio) {       /* Only elevate if ceiling is higher priority*/
            /* Remove task from ready list at current priority */
            OSRdyTbl[OSTCBCur->OSTCBY] &= ~OSTCBCur->OSTCBBitX;
            if (OSRdyTbl[OSTCBCur->OSTCBY] == 0) {
                OSRdyGrp &= ~OSTCBCur->OSTCBBitY;
            }


            /* Change task priority to ceiling priority */
            OSTCBCur->OSTCBPrio = ceiling_prio;

            /* Update task's priority bit position variables */
#if OS_LOWEST_PRIO <= 63
            OSTCBCur->OSTCBY    = (INT8U)(OSTCBCur->OSTCBPrio >> 3);
            OSTCBCur->OSTCBX    = (INT8U)(OSTCBCur->OSTCBPrio & 0x07);
            OSTCBCur->OSTCBBitY = (INT8U)(1 << OSTCBCur->OSTCBY);
            OSTCBCur->OSTCBBitX = (INT8U)(1 << OSTCBCur->OSTCBX);
#else
            OSTCBCur->OSTCBY    = (INT8U)((OSTCBCur->OSTCBPrio >> 4) & 0xFF);
            OSTCBCur->OSTCBX    = (INT8U)(OSTCBCur->OSTCBPrio & 0x0F);
            OSTCBCur->OSTCBBitY = (INT16U)(1 << OSTCBCur->OSTCBY);
            OSTCBCur->OSTCBBitX = (INT16U)(1 << OSTCBCur->OSTCBX);
#endif

            /* Add task back to ready list at ceiling priority */
            OSRdyGrp               |= OSTCBCur->OSTCBBitY;
            OSRdyTbl[OSTCBCur->OSTCBY] |= OSTCBCur->OSTCBBitX;

            /* Update priority table */
            OSTCBPrioTbl[ceiling_prio] = OSTCBCur;
            OSTCBPrioTbl[original_prio] = (OS_TCB *)0;
        }

        OS_EXIT_CRITICAL();
        *perr = OS_ERR_NONE;
        return;
    }

    /* Mutex is not available, task must wait */
    OSTCBCur->OSTCBStat     |= OS_STAT_MUTEX;           /* Flag task as pending on mutex             */
    OSTCBCur->OSTCBStatPend  = OS_STAT_PEND_OK;
    OSTCBCur->OSTCBDly       = timeout;                 /* Store timeout in TCB                      */

    /* Add task to event wait list */
    OS_EventTaskWait(pevent);
    OS_EXIT_CRITICAL();
    OS_Sched();                                         /* Find highest priority task ready           */

    /* Task resumes here when mutex obtained or timeout */
    OS_ENTER_CRITICAL();

    /* Check why task was resumed */
    switch (OSTCBCur->OSTCBStatPend) {
        case OS_STAT_PEND_OK:
             *perr = OS_ERR_NONE;
             break;

        case OS_STAT_PEND_ABORT:
             *perr = OS_ERR_PEND_ABORT;
             break;

        case OS_STAT_PEND_TO:
        default:
             OS_EventTaskRemove(OSTCBCur, pevent);
             *perr = OS_ERR_TIMEOUT;
             break;
    }

    /* Clear status flags */
    OSTCBCur->OSTCBStat          = OS_STAT_RDY;
    OSTCBCur->OSTCBStatPend      = OS_STAT_PEND_OK;
    OSTCBCur->OSTCBEventPtr      = (OS_EVENT *)0;
#if (OS_EVENT_MULTI_EN > 0)
    OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **)0;
#endif

    OS_EXIT_CRITICAL();
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                  POST TO A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function signals a mutual exclusion semaphore
*
* Arguments  : pevent              is a pointer to the event control block associated with the desired
*                                  mutex.
*
* Returns    : OS_ERR_NONE             The call was successful and the mutex was signaled.
*              OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mutex
*              OS_ERR_PEVENT_NULL      'pevent' is a NULL pointer
*              OS_ERR_POST_ISR         Attempted to post from an ISR (not valid for MUTEXes)
*              OS_ERR_NOT_MUTEX_OWNER  The task that did the post is NOT the owner of the MUTEX.
*              OS_ERR_PIP_LOWER        If the priority of the new task that owns the Mutex is
*                                      HIGHER (i.e. a lower number) than the PIP.  This error
*                                      indicates that you did not set the PIP higher (lower
*                                      number) than ALL the tasks that compete for the Mutex.
*                                      Unfortunately, this is something that could not be
*                                      detected when the Mutex is created because we don't know
*                                      what tasks will be using the Mutex.
*********************************************************************************************************
*/

INT8U OSMutexPost(OS_EVENT *pevent)
{
    INT8U      ceiling_prio;                           /* Ceiling priority                           */
    INT8U      original_prio;                          /* Original priority of task                  */
    INT8U      prio;                                   /* Priority of task made ready                */
    OS_TCB    *ptcb;                                   /* Pointer to TCB                             */
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr = 0;
#endif

    /* Initial validation */
    if (OSIntNesting > 0) {
        return (OS_ERR_POST_ISR);
    }
#if OS_ARG_CHK_EN > 0
    if (pevent == (OS_EVENT *)0) {
        return (OS_ERR_PEVENT_NULL);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {
        return (OS_ERR_EVENT_TYPE);
    }

    OS_ENTER_CRITICAL();

    if (OSTCBCur != (OS_TCB *)pevent->OSEventPtr) {    /* Verify current task owns the mutex         */
        OS_EXIT_CRITICAL();
        return (OS_ERR_NOT_MUTEX_OWNER);
    }

    ceiling_prio = (INT8U)(pevent->OSEventCnt >> 8);   /* Get ceiling priority                       */
    original_prio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8); /* Get original priority    */
    int highest_ceiling_prio;
	if (original_prio<ceiling_prio){
		highest_ceiling_prio=original_prio;
	}else{
		highest_ceiling_prio=ceiling_prio;
	}
    char tempBuf[MSG_BUF_SIZE];
	sprintf(tempBuf, "%5d unlock  R%1d  (Prio=%3d change to=%3d)\n",
			(int)OSTimeGet(), (int)ceiling_prio,(int)highest_ceiling_prio, // 當前任務的優先級
			(int)original_prio); // 此 Mutex 的天花板優先級);
	AddMessageToQueue(tempBuf);
    /* If task priority was elevated, restore it */
    if (OSTCBCur->OSTCBPrio != original_prio) {
        /* Remove from ready list at ceiling priority */
        OSRdyTbl[OSTCBCur->OSTCBY] &= ~OSTCBCur->OSTCBBitX;
        if (OSRdyTbl[OSTCBCur->OSTCBY] == 0) {
            OSRdyGrp &= ~OSTCBCur->OSTCBBitY;
        }
        //update OSPrioCur
        OSPrioCur=original_prio;
        /* Restore original priority */
        OSTCBCur->OSTCBPrio = original_prio;

        /* Update priority bit position variables */
#if OS_LOWEST_PRIO <= 63
        OSTCBCur->OSTCBY    = (INT8U)(OSTCBCur->OSTCBPrio >> 3);
        OSTCBCur->OSTCBX    = (INT8U)(OSTCBCur->OSTCBPrio & 0x07);
        OSTCBCur->OSTCBBitY = (INT8U)(1 << OSTCBCur->OSTCBY);
        OSTCBCur->OSTCBBitX = (INT8U)(1 << OSTCBCur->OSTCBX);
#else
        OSTCBCur->OSTCBY    = (INT8U)((OSTCBCur->OSTCBPrio >> 4) & 0xFF);
        OSTCBCur->OSTCBX    = (INT8U)(OSTCBCur->OSTCBPrio & 0x0F);
        OSTCBCur->OSTCBBitY = (INT16U)(1 << OSTCBCur->OSTCBY);
        OSTCBCur->OSTCBBitX = (INT16U)(1 << OSTCBCur->OSTCBX);
#endif

        /* Add back to ready list at original priority */
        OSRdyGrp                |= OSTCBCur->OSTCBBitY;
        OSRdyTbl[OSTCBCur->OSTCBY] |= OSTCBCur->OSTCBBitX;

        /* Update priority table */
        OSTCBPrioTbl[original_prio] = OSTCBCur;
        OSTCBPrioTbl[ceiling_prio] = (OS_TCB *)0;
    }

    /* Check if any tasks are waiting for the mutex */
    if (pevent->OSEventGrp != 0) {
        /* Make highest priority waiting task ready */
        prio = OS_EventTaskRdy(pevent, (void *)0, OS_STAT_MUTEX, OS_STAT_PEND_OK);
        ptcb = OSTCBPrioTbl[prio];

        /* Store original priority in lower 8 bits */
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;
        pevent->OSEventCnt |= prio;

        /* Update mutex owner */
        pevent->OSEventPtr = ptcb;

        /* Immediately elevate new owner's priority to ceiling priority */
        if (prio > ceiling_prio) {
            /* Remove from ready list at current priority */
            OSRdyTbl[ptcb->OSTCBY] &= ~ptcb->OSTCBBitX;
            if (OSRdyTbl[ptcb->OSTCBY] == 0) {
                OSRdyGrp &= ~ptcb->OSTCBBitY;
            }

            /* Change priority to ceiling priority */
            ptcb->OSTCBPrio = ceiling_prio;

            /* Update bit position variables */
#if OS_LOWEST_PRIO <= 63
            ptcb->OSTCBY    = (INT8U)(ptcb->OSTCBPrio >> 3);
            ptcb->OSTCBX    = (INT8U)(ptcb->OSTCBPrio & 0x07);
            ptcb->OSTCBBitY = (INT8U)(1 << ptcb->OSTCBY);
            ptcb->OSTCBBitX = (INT8U)(1 << ptcb->OSTCBX);
#else
            ptcb->OSTCBY    = (INT8U)((ptcb->OSTCBPrio >> 4) & 0xFF);
            ptcb->OSTCBX    = (INT8U)(ptcb->OSTCBPrio & 0x0F);
            ptcb->OSTCBBitY = (INT16U)(1 << ptcb->OSTCBY);
            ptcb->OSTCBBitX = (INT16U)(1 << ptcb->OSTCBX);
#endif

            /* Add to ready list at ceiling priority */
            OSRdyGrp               |= ptcb->OSTCBBitY;
            OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;

            /* Update priority table */
            OSTCBPrioTbl[ceiling_prio] = ptcb;
            OSTCBPrioTbl[prio] = (OS_TCB *)0;
        }


        OS_EXIT_CRITICAL();
        OS_Sched();                                   /* Find highest priority task ready to run     */
        return (OS_ERR_NONE);
    }

    /* No tasks waiting, make mutex available */
    pevent->OSEventCnt |= OS_MUTEX_AVAILABLE;
    pevent->OSEventPtr  = (void *)0;

    OS_EXIT_CRITICAL();
    return (OS_ERR_NONE);
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                     QUERY A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function obtains information about a mutex
*
* Arguments  : pevent          is a pointer to the event control block associated with the desired mutex
*
*              p_mutex_data    is a pointer to a structure that will contain information about the mutex
*
* Returns    : OS_ERR_NONE          The call was successful and the message was sent
*              OS_ERR_QUERY_ISR     If you called this function from an ISR
*              OS_ERR_PEVENT_NULL   If 'pevent'       is a NULL pointer
*              OS_ERR_PDATA_NULL    If 'p_mutex_data' is a NULL pointer
*              OS_ERR_EVENT_TYPE    If you are attempting to obtain data from a non mutex.
*********************************************************************************************************
*/

#if OS_MUTEX_QUERY_EN > 0
INT8U  OSMutexQuery (OS_EVENT *pevent, OS_MUTEX_DATA *p_mutex_data)
{
    INT8U      i;
#if OS_LOWEST_PRIO <= 63
    INT8U     *psrc;
    INT8U     *pdest;
#else
    INT16U    *psrc;
    INT16U    *pdest;
#endif
#if OS_CRITICAL_METHOD == 3                      /* Allocate storage for CPU status register           */
    OS_CPU_SR  cpu_sr = 0;
#endif



    if (OSIntNesting > 0) {                                /* See if called from ISR ...               */
        return (OS_ERR_QUERY_ISR);                         /* ... can't QUERY mutex from an ISR        */
    }
#if OS_ARG_CHK_EN > 0
    if (pevent == (OS_EVENT *)0) {                         /* Validate 'pevent'                        */
        return (OS_ERR_PEVENT_NULL);
    }
    if (p_mutex_data == (OS_MUTEX_DATA *)0) {              /* Validate 'p_mutex_data'                  */
        return (OS_ERR_PDATA_NULL);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {      /* Validate event block type                */
        return (OS_ERR_EVENT_TYPE);
    }
    OS_ENTER_CRITICAL();
    p_mutex_data->OSMutexPIP  = (INT8U)(pevent->OSEventCnt >> 8);
    p_mutex_data->OSOwnerPrio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);
    if (p_mutex_data->OSOwnerPrio == 0xFF) {
        p_mutex_data->OSValue = OS_TRUE;
    } else {
        p_mutex_data->OSValue = OS_FALSE;
    }
    p_mutex_data->OSEventGrp  = pevent->OSEventGrp;        /* Copy wait list                           */
    psrc                      = &pevent->OSEventTbl[0];
    pdest                     = &p_mutex_data->OSEventTbl[0];
    for (i = 0; i < OS_EVENT_TBL_SIZE; i++) {
        *pdest++ = *psrc++;
    }
    OS_EXIT_CRITICAL();
    return (OS_ERR_NONE);
}
#endif                                                     /* OS_MUTEX_QUERY_EN                        */

/*$PAGE*/
/*
*********************************************************************************************************
*                                RESTORE A TASK BACK TO ITS ORIGINAL PRIORITY
*
* Description: This function makes a task ready at the specified priority
*
* Arguments  : ptcb            is a pointer to OS_TCB of the task to make ready
*
*              prio            is the desired priority
*
* Returns    : none
*********************************************************************************************************
*/

static  void  OSMutex_RdyAtPrio (OS_TCB *ptcb, INT8U prio)
{
    INT8U   y;


    y            =  ptcb->OSTCBY;                          /* Remove owner from ready list at 'pip'    */
    OSRdyTbl[y] &= ~ptcb->OSTCBBitX;
    if (OSRdyTbl[y] == 0) {
        OSRdyGrp &= ~ptcb->OSTCBBitY;
    }
    ptcb->OSTCBPrio         = prio;
#if OS_LOWEST_PRIO <= 63
    ptcb->OSTCBY            = (INT8U)((prio >> (INT8U)3) & (INT8U)0x07);
    ptcb->OSTCBX            = (INT8U) (prio & (INT8U)0x07);
    ptcb->OSTCBBitY         = (INT8U)(1 << ptcb->OSTCBY);
    ptcb->OSTCBBitX         = (INT8U)(1 << ptcb->OSTCBX);
#else
    ptcb->OSTCBY            = (INT8U)((prio >> (INT8U)4) & (INT8U)0x0F);
    ptcb->OSTCBX            = (INT8U) (prio & (INT8U)0x0F);
    ptcb->OSTCBBitY         = (INT16U)(1 << ptcb->OSTCBY);
    ptcb->OSTCBBitX         = (INT16U)(1 << ptcb->OSTCBX);
#endif
    OSRdyGrp               |= ptcb->OSTCBBitY;             /* Make task ready at original priority     */
    OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
    OSTCBPrioTbl[prio]      = ptcb;
}


#endif                                                     /* OS_MUTEX_EN                              */
