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
// PROTOTYPES
//
void sleepHandler(USLOSS_Sysargs *args);
void termReadHandler(USLOSS_Sysargs *args);
void termWriteHandler(USLOSS_Sysargs *args);
static int checkForKernelMode(void);
static void blockOnMbox(int mboxNum);
static void unblockOnMbox(int mboxNum);

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

}

void phase4_start_service_processes(void) {}

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
