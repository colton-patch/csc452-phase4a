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
    char writeBuf[MAXLINE]; // message to write to a terminal
    char readBuf[MAXLINE]; // buffer for message read from terminal 
    int readLen;
    struct pcb *sleepQueueNext;
    struct pcb termReadQueueNexts[4]; // terminals 0-3 read queue pointers
    struct pcb termWriteQueueNexts[4]; // terminals 0-3 write queue pointers

};

struct terminalControl {
    char readBuffers[10][MAXLINE + 1];
    int numFilledBufs;
    int nextFilledBuf;
}

//
// PROTOTYPES
//
void sleepHandler(USLOSS_Sysargs *args);
void termReadHandler(USLOSS_Sysargs *args);
void termWriteHandler(USLOSS_Sysargs *args);
int clockDaemon(void *arg);
static int checkForKernelMode(void);
static void grabLock(int mboxNum);
static void releaseLock(int mboxNum);
static void sleepEnqueue(int pid);
static void sleepDequeue();
static void termEnqueue(int read, int unit, int pid);
static void termDequeue(int read, int unit);

//
// GLOBAL VARIABLES
//
static int sleepingProcs = 0; // processes currently sleeping
static struct pcb *sleepQueueHd; // sleep queue head
static struct pcb *termReadQueueHds[4]; // terminals 0-3 read queue heads
static struct pcb *termWriteQueueHds[4]; // terminals 0-3 write queue heads
static struct pcb pcbTable[MAXPROC]; // shadow table of PCBs
int termWriteLocks[4]; // array of mbox IDs for the write locks for terminals 0-3
int termReadLocks[4]; // array of mbox IDs for the read locks for terminals 0-3
struct terminalControl termCtrls[4]; // array of terminal controls

//
// NON-STATIC FUNCTIONS
//

/*
* void phase4_init(void) - phase 4 initialization function. Fills
*   the system call vector with the handlers defined in phase 4 as well
*   as enabling interrupts, creating locks, and initializing terminal 
*   control structures.
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

    // enable interrupts for terminals
    for (int i=0; i<4; i++) {
        int crVal = 0x0;
        crVal |= 0x2;
        crVal |= 0x4;
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void*)(long)crVal);
    }
    
    // make locks
    for (int i=0; i<4; i++) {
        termWriteLocks[i] = MboxCreate(1, 0);
        termReadLocks[i] = MboxCreate(1, 0);
        releaseLock(termWriteLocks[i]);
        releaseLock(termReadLocks[i]);
    }

    // initialize terminal controls structures
    for (int i=0; i<4; i++) {
        termCtrls[i].nextFilledBuf = -1;
        termCtrls[i].numFilledBufs = 0;
    }
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

    // get current process
    int curPid = getpid();
    struct pcb *curProc = &pcbTable[curPid % MAXPROC];
    curProc->pid = curPid;

    // add to termRead queue and block
    termEnqueue(1, unit, curPid);
    blockMe();

    // fill buf with characters from readBuf field
    int i = 0;
    char *readBuf = curProc->readBuf;
    if (curProc->readLen < len) {

    } else {

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

    // get current process
    int curPid = getpid();
    struct pcb *curProc = &pcbTable[curPid % MAXPROC];
    curProc->pid = curPid;

    char *writeBuf = curProc->writeBuf;
    // fill its writeBuf field
    if (len < MAXLINE) {
        strncpy(writeBuf, buf);
        writeBuf[len] = "\0";
    } else {
        strncpy(writeBuf, buf, MAXLINE);
        writeBuf[MAXLINE] = "\0";
    }

    // add to termWrite queue and block
    termEnqueue(0, unit, curPid);
    blockMe();   

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

int terminalDaemon(void *arg) {
    int unit = (int)(long)arg;
    int zero = 0;
    int *status = &zero;

    int writing = 0; // whether or not writing is in progress
    int reading = 0; // whether or not reading a line is in progress
    int writeIdx = 0;
    int readIdx = 0;

    while (1) {
        waitDevice(USLOSS_TERM_DEV, unit, status);

        // check if there is already a stored line that can be read by a process
        if (termReadQueueHds[unit] != NULL && termCtrls[unit].numFilledBufs > 0) {
            int bufIdx = termCtrls[unit].nextFilledBuf
            strcpy(termReadQueueHds[unit]->readBuf, termCtrls[unit].readBuffers[bufIdx]);
            termCtrls[unit].numFilledBufs--;
            termCtrls[unit].nextFilledBuf--;

            termDequeue(1, unit);
        }

        // if ready for writing
        if (USLOSS_TERM_STAT_XMIT(status) == USLOSS_DEV_READY && termWriteQueueHds[unit] != NULL) {
            struct pcb *writingProc = termWriteQueueHds[unit];
            
            // if just starting to write, grab a lock so that nothing else can write
            if (!writing) {
                grabLock(termWriteLocks[unit]);
                writing = 1;
            }
            
            // if end of buffer is reached, release lock and unblock proc
            if (writingProc->writeBuf[writeIdx] == '\0' || writeIdx == MAXLINE) {
                writeIdx = 0;
                termDequeue(0, unit);
                releaseLock(termWriteLocks[unit]);
                writing = 0;

            } else {
                // write character to device
                int crVal = 0x1; // this turns on the ’send char’ bit (USLOSS spec page 9)
                crVal |= 0x2; // recv int enable
                crVal |= 0x4; // xmit int enable
                crVal |= (buf[writeIdx] << 8); // the character to send
                USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)crVal);

                writeIdx++;
            }
        }

        // if ready for reading
        if (USLOSS_TERM_STAT_RECV(status) == USLOSS_DEV_READY) {
            struct pcb *readingProc = termReadQueueHds[unit];
            int bufIdx = termCtrls[unit].nextFilledBuf;
            
            // if just starting to read, grab a lock so that nothing else can read
            if (!reading) {
                // if there have already been 10 stored buffers, ignore this read
                if (termCtrls[unit].numFilledBufs == 10) {
                    continue;
                }

                grabLock(termReadLocks[unit]);
                reading = 1;
            }
            
            // read character
            char nextChar = USLOSS_TERM_STAT_CHAR(status);

            // if the end of a line is reached
            if (nextChar == '\n' || writeIdx = MAXLINE) {
                readIdx = 0;
                termCtrls[unit].numFilledBufs++; 

                // give the buffer to a waiting process and unblock it
                if (termReadQueueHds[unit] != NULL) {
                    strcpy(termReadQueueHds[unit]->readBuf, termCtrls[unit].readBuffers[bufIdx]);
                    termCtrls[unit].numFilledBufs--;
                    termCtrls[unit].nextFilledBuf = (termCtrls[unit].nextFilledBuf + 1) % 10;
                    termDequeue(1, unit);
                }

                releaseLock(termWriteLocks[unit]);
                reading = 0;
            } 
            else {
                // store the next character in one of the terminals buffers
                termCtrls[unit].readBuffers[bufIdx][readIdx] = nextChar;
                readIdx++;
            }
        }
    }

    return 0; // to avoid warnings. loop will never terminate
}

void phase4_start_service_processes(void) {
    // spork clock daemon process
    spork("clockDaemon", clockDaemon, NULL, USLOSS_MIN_STACK, 1);

    // spork terminal daemon process for each terminal unit
    for (int i = 0; i < 4; i++) {
        char name[16];
        sprintf(name, "terminalDaemon%d", i);
        spork(name, terminalDaemon, (void *)(long)i, USLOSS_MIN_STACK, 1)
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
* static void grabLock(int mboxNum) - calls MboxRecv on the given mailbox,
*   causing the process to block until a message is sent to that mailbox
*   mboxNum - ID number of mailbox to block on
*/
static void grabLock(int mboxNum) {
    char msg[] = "";
    MboxRecv(mboxNum, msg, 0);
}

/*
* static void releaseLock(int mboxNum) - calls MboxSend on the given mailbox,
*   unblocking a process that may be blocked on a receive call to it
*   mboxNum - ID number of mailbox to unblock on
*/
static void releaseLock(int mboxNum) {
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

/*
* static void termEnqueue(int read, int unit, int pid) - places
*   the process with the given pid in one of ther terminal queues.
*   read : 1 if placing in a read queue, 0 if placing in a write queue.
*   unit : the unit (0-3) of the terminal whose queue to add to.
*   pid : PID of the process to be enqueued
*/
static void termEnqueue(int read, int unit, int pid) {
    // get the desired queue head
    struct pcb **queueHd;
    if (read) {
        queueHd = &termReadQueueHds[unit];
    } else {
        queueHd = &termWriteQueueHds[unit];
    }

    struct pcb *proc = pcbTable[pid % MAXPROC];

    // place proc as the head if the head is empty
    if (*queueHd == NULL) {
        *queueHd = proc;
    } else {
        // traverse queue
        struct pcb *next = *queueHd;
        struct pcb *prev = NULL;
        while (next != NULL) {
            prev = next;
            if (read) {
                next = next->termReadQueueNexts[unit];
            } else {
                next = next->termWriteQueueNexts[unit];
            }
        }
        
        // place proc in its spot
        if (read) {
            prev->termReadQueueNexts[unit] = proc;
            proc->termReadQueueNexts[unit] = NULL;
        } else {
            prev->termWriteQueueNexts[unit] = proc;
            proc->termWriteQueueNexts[unit] = NULL;
        }
    }
}

/*
* static void termDequeue(int read, int unit) - removes the process
*   from the head of a terminal queue and unblocks it.
*   read : 1 if removing from a read queue, 0 if removing from a write queue.
*   unit : the unit (0-3) of the terminal whose queue to remove from.
*/
static void termDequeue(int read, int unit) {
    struct pcb **queueHd;
    if (read) {
        queueHd = &termReadQueueHds[unit];
    } else {
        queueHd = &termWriteQueueHds[unit];
    }

    unblockProc(*queueHd->pid);

    if (read) {
        *queueHd = *queueHd->termReadQueueNexts[unit];
    } else {
        *queueHd = *queueHd->termWriteQueueNexts[unit];
    }
}


