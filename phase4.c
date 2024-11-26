/*
 * Authors: Colton Patch, Ping Tontrasathien
 * phase4.c - 
 * 
 */

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase3_usermode.h>
#include <phase4_usermode.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//
// STRUCTURES
//
struct pcb {
    int pid;
    int sleepEnd; // what time the process should stop sleeping. Used for queueing
    struct pcb *sleepQueueNext;
};

//
// PROTOTYPES
//
void sleepHandler(USLOSS_Sysargs *args);
void termReadHandler(USLOSS_Sysargs *args);
void termWriteHandler(USLOSS_Sysargs *args);
int clockDaemon(void *arg);
static int checkForKernelMode(void);
static void blockOnMbox(int mboxNum);
static void unblockOnMbox(int mboxNum);
static void sleepEnqueue(int pid);
static void sleepDequeue();

//
// GLOBAL VARIABLES
//
static int sleepingProcs = 0; // processes currently sleeping
static struct pcb *sleepQueueHd;
static struct pcb pcbTable[MAXPROC]; // shadow table of PCBs

//
// NON-STATIC FUNCTIONS
//

/*
* void phase4_init(void) - phase 4 initialization function. Fills
*   the system call vector with the handlers defined in phase 4
*/
void phase4_init(void) {
    // make sure in kernel mode
	if (checkForKernelMode() == 0) {
		USLOSS_Trace("ERROR: Someone attempted to call %s while in user mode!\n", __func__);
		USLOSS_Halt(1);
	}

    // fill system call vector with system calls
    systemCallVec[SYS_SLEEP] = sleepHandler;
    systemCallVec[SYS_TERMREAD] = termReadHandler;
    systemCallVec[SYS_TERMWRITE] = termWriteHandler;
}

/*
* void sleepHandler(USLOSS_Sysargs *args) - pauses the current process
*   for the number of seconds specified in args->arg1
*   Input:
*       args->arg1 : Number of seconds to sleep for
*   Output:
*       args->arg4 : -1 if illegal number of seconds, else 0
*/
void sleepHandler(USLOSS_Sysargs *args) {
    int arg1 = (int)(long)args->arg1;
    // check that a negative number of seconds was not passed
    if (arg1 < 0) {
        args->arg4 = (void*)(long)-1;
        return;
    }

    // calculate what time the process should stop sleeping
    int sleepEnd = currentTime() + (arg1 * 1000000);

    // get current process and fill its sleepEnd field
    int curPid = getpid();
    struct pcb *curProc = &pcbTable[curPid % MAXPROC];
    curProc->pid = curPid;
    curProc->sleepEnd = sleepEnd;

    // add to sleep queue and block
    sleepEnqueue(curPid);
    blockMe();

    args->arg4 = (void*)(long)0;
    return;
}

/*
* void termReadHandler(USLOSS_Sysargs *args) - reads a single line
*   from a terminal into a buffer. Will either end with newline or 
*   be MAXLINE characters long.
*   Input:
*       args->arg1 : buffer pointer
*       args->arg2 : length of buffer
*       args->arg3 : number of terminal to read
*   Output:
*       args->arg2 : number of characters read
*       args->arg4 : -1 if illegal input, else 0
*/
void termReadHandler(USLOSS_Sysargs *args) {
    char *buf = (char *)args->arg1;
    int len = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;

    // check for legal input
    if (unit < 0 || unit > 3) {
        args->arg4 = (void*)(long)-1;
        return;
    } else {
        args->arg4 = (void*)(long)0;
    }

    int i = 0;
    while (i < len) {
        // read a character and put into buffer
//        int status; 
//        USLOSS_DeviceInput(USLOSS_TERM_DEV,unit,&status);
//        c = ...get character from terminal status register...
//        buf[i] = c;
        i++;

        // check for the end of line
//        if (c == '\n' || i == MAXLINE) break;
    }
    
    args->arg2 = i;
    return;
}

/*
* void termWriteHandler(USLOSS_Sysargs *args) - Writes characters from
*   a buffer to a terminal
*   Input:
*       args->arg1 : buffer pointer
*       args->arg2 : length of buffer
*       args->arg3 : number of terminal to write to
*   Output:
*       args->arg2 : number of characters written
*       args->arg4 : -1 if illegal input, else 0
*/
void termWriteHandler(USLOSS_Sysargs *args) {
    char *buf = (char *)args->arg1;
    int len = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;

    // check for legal input
    if (unit < 0 || unit > 3) {
        args->arg4 = (void*)(long)-1;
    } else {
        args->arg4 = (void*)(long)0;
    }

    int i = 0;
    while (i < len) {
        // write a character from buffer to terminal
//        char c = buf[i];
//        control = ...build a terminal control register...
//        USLOSS_DeviceOutput(USLOSS_TERM_DEV,unit,control);

        // check for the null terminator
//        if (c == '\0') break;

        i++;
    }
   
    args->arg2 = i;
    return;
}

/*
* int clockDaemon(void *arg) - constantly calls waitDevice on the clock, and
*   checks if the queued up sleeping processes are ready to wake up.
*   arg - unused. Only to avoid warnings so it can be passed as a process'
*   start function.
*/
int clockDaemon(void *arg) {
    (void)arg;
    int zero = 0;
    int *status = &zero;

    while (1) {
        waitDevice(USLOSS_CLOCK_DEV, 0, status);
        if (sleepQueueHd != NULL && currentTime() >= sleepQueueHd->sleepEnd) {
            sleepDequeue();
        } 
    }

    return 0; // to avoid warnings. loop will never terminate
}

void phase4_start_service_processes(void) {
    // spork clock daemon process
    spork("clockDaemon", clockDaemon, NULL, USLOSS_MIN_STACK, 1);
    for (int i = 0; i < 4; i++) {
        char name[16];
        sprintf(name, "terminalDaemon%d", i);
        spork("terminalDaemon", terminalDaemon, (void *)(long)i, USLOSS_MIN_STACK, 1)
    }
}

//
// STATIC FUNCTIONS
//

/*
* static int checkForKernelMode(void) - returns 0 if not in kernel mode
*/
static int checkForKernelMode(void) {
	// check if not in kernel mode
	return (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()); 
}

/*
* static void blockOnMbox(int mboxNum) - calls MboxRecv on the given mailbox,
*   causing the process to block until a message is sent to that mailbox
*   mboxNum - ID number of mailbox to block on
*/
static void blockOnMbox(int mboxNum) {
    char msg[] = "";
    MboxRecv(mboxNum, msg, 0);
}

/*
* static void unblockOnMbox(int mboxNum) - calls MboxSend on the given mailbox,
*   unblocking a process that may be blocked on a receive call to it
*   mboxNum - ID number of mailbox to unblock on
*/
static void unblockOnMbox(int mboxNum) {
    char msg[] = "";
    MboxSend(mboxNum, msg, 0);
}

/*
* static void sleepEnqueue(int pid) - Places the process with the given pid
*   in the sleep queue, which is ordered by sleep end time.
*   pid - PID of the process to place in the queue
*/
static void sleepEnqueue(int pid) {
    struct pcb *proc = &pcbTable[pid % MAXPROC];
    int endTime = proc->sleepEnd;
    // make proc the head if queue is empty
    if (sleepQueueHd == NULL) {
        sleepQueueHd = proc;
    }
    else {
        // find where proc belongs in the queue ordered by sleep end time
        struct pcb *next = sleepQueueHd;
        struct pcb *prev = NULL;
        while (next != NULL && endTime > next->sleepEnd) {
            prev = next;
            next = next->sleepQueueNext;
        }
        
        // place proc in its spot
        proc->sleepQueueNext = next;
        if (prev != NULL) {
            prev->sleepQueueNext = proc;
        }
    }
}

/*
* static void sleepDequeue() - removes the process from the head of the
*   sleep queue and unblocks it
*/
static void sleepDequeue() { 
    unblockProc(sleepQueueHd->pid);
    sleepQueueHd = sleepQueueHd->sleepQueueNext;
}

