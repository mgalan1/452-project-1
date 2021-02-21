/* ------------------------------------------------------------------------
   phase1.c

   Group members: Arianna Boatner, Laura Bolanos arboleda, and Marisa Flor Galan 

   Class: CSCV 452
   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
struct proc_struct *readyList; 

/* Linked list to keep track of zombie children - Added by Arianna */
struct zombieNode *zombieList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* the  table  that  holds  the  function  pointers  for  the  interrupt  handlers */
void clock_handler(int dev, void *unit);

/* -------------------------- Functions ----------------------------------- */
/*------------------------------------------------------------------------
   Name - insertRL
   Purpose - Helper function to insert a pointer to PCB to our ready queue 
             Using a linked list 

   Parameters: none
   Returns - nothing

   This function was added in by Arianna on 2/1/2021. 
------------------------------------------------------------------------ */
void insertRL(proc_ptr proc)
{
   proc_ptr walker, previous; //pointers to PCB
   previous = NULL;
   walker = readyList;
   while (walker != NULL && walker->priority <= proc->priority ){
      previous = walker;
      walker = walker->next_proc_ptr;
   }

   if (previous == NULL){
      //process goes at front of ReadyList 
      proc->next_proc_ptr = readyList;
      readyList = proc;
   }
   else {
      // Process goes after previous 
      previous->next_proc_ptr = proc;
      proc->next_proc_ptr = walker;
   } 
   return;
} 

/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   // Code to loop over PCB array

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   readyList = NULL;

   /* Initialize the clock interrupt handler */
   // Line below added in by Arianna
   int_vec[CLOCK_DEV] = clock_handler;  //function pointer is used here

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }
  
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 4 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot;
   
   // TODO need to call malloc() in here

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   if((PSR_CURRENT_MODE & psr_get()) == 0) {
      // In user mode
      console("User mode is on\n");
      halt(1);
   } 
   else
   {
      /* We ARE in kernel mode */
      psr_set( psr_get() & ~PSR_CURRENT_INT );
   }

   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK) { // Added in by Arianna - Got an abort trap here
      console("The stack size was too small");
      return -2;
   }

   struct proc_struct *newNode = NULL; //creates a new node for the linked list
   newNode = malloc(sizeof(struct proc_struct));
   if(newNode==NULL){
      fprintf(stderr, "out of memory!\n");
      exit(1);
   }

   /* find an empty slot in the process table */
   /* Block of code added in by Arianna 
      Purpose of the added code is to assign
      a PID value for the new process. This is just one 
      way to assign a new PID for a new process.

      Reference: 1-27-2021 Lecture ~57 min mark + Phase 1 slides
    */

   proc_slot = next_pid % MAXPROC;
   int pid_count = 0;
   while ( pid_count < MAXPROC && ProcTable[proc_slot].status != EMPTY ) {
      next_pid++;
      proc_slot = next_pid % MAXPROC;
      pid_count++;
   }

   //Assuming we have found the index in the process table  ProcTable  to  maintain  your  new  processstackptr = malloc(stacksize);

   /* Return if process table is full */ 
   // Added by Arianna
   if ( pid_count >= MAXPROC ){
    enableInterrupts();
    if (DEBUG && debugflag){
      console("fork1(): process table full\n");
      return -1;
    }
   }

   /* fill-in entry in process table */
   ProcTable[proc_slot].pid = next_pid++; // Line added in by Arianna

   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }

   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].start_func = f;
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   // Return the PID value
   return ProcTable[proc_slot].pid; 

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n"); 

   /* Enable interrupts */
   enableInterrupts(); 

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result); 

}  
/* launch */
 



/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
   /*
   We need something like this:
      *code = kid_ptr->exit_code;
      return kid_pid;
   */
   return 0;
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   // Check if there is an active child process - If so, then call halt(1)
   // Added in by Arianna
   if (Current->child_proc_ptr) //Don't know if we should be checking anything else with child_proc_ptr
   {
      halt(1);
   }

   //Change the status of the current running process to quit 
   p1_quit(Current->status);

   //TODO Update ReadyList and ZombieList

} 


/* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
 void dispatcher(void)
{
   proc_ptr next_process;

/*    // We need to have the round priority based scheduling using a linked list here 
   // We also need to perform a context switch in here
   if(Current != NULL)
   {
      // There is a dispatcher
      console("Current dispatcher: %s\n", Current->name); 
   }
   else
   {
      // There is not a dispatcher
      USLOSS_Console("Dispatcher is called: NULL\n"); 
   }

   proc_ptr next_process = NULL;

   if((PSR_CURRENT_MODE & psr_get()) == 0) {
      //not in kernel mode
      console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
      halt(1);
  } 
   int time;

   USLOSS_DeviceInput(0, 0, &time);

   if (Current== NULL || Current->status == QUIT)
   {
      proc_struct tempVal = RemoveLL();
      Current = &tempVal;
      USLOSS_DeviceInput(0, 0, &(Current->startedTime));
      Current->status = BLOCKED;
      p1_switch(-1, Current->pid);
      USLOSS_ContextSwitch(NULL, &(Current->state));
   }

   else
   {
      proc_struct tempVal = RemoveLL();
      next_process = &tempVal;

      if(Current->status == BLOCKED)
      {
         Current->status = READY;

         // Add to the linked list
         insertRL(Current); 
      }
      next_process->status = BLOCKED;
      
      // We need to have the priority based scheduling using a linked list
      console("Current process= %s, Next process= %s\n",Current->name,next_process->name); //DEBUGGING
      p1_switch(Current->pid, next_process->pid);
      enableInterrupts();
      USLOSS_ContextSwitch(&(Current->state), &(next_process->state))

   }
 */
   p1_switch(Current->pid, next_process->pid);

}  /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char * dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
//Code added in by Arianna 2/1/2021
 static void check_deadlock()
{
   if (check_io() == 1){
      return;
   }
 
   /* Checking if everyone is terminated
      Check the number of processes. 
   */

   // If there is only one active process,
   halt(0);

   // Otherwise:
   halt(1);

} /* check_deadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
      //not in kernel mode
      console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
      halt(1);
  } else
      /* We ARE in kernel mode */
      psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */


/*
 * This function enables interrupts.
 * This was created by Arianna
 */
void enableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
      //not in kernel mode
      console("Not in kernel mode\n");
      halt(1);
  } else
      /* We ARE in kernel mode */
      psr_set( psr_get() | PSR_CURRENT_INT );

} /* enableInterrupts */


// This function is needed to implement the round-robin scheduling policy
// This was created by Arianna
void clock_handler(int dev, void * unit)
{
   if((PSR_CURRENT_MODE & psr_get()) == 0) {
      //not in kernel mode
      console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
      halt(1);
   }

    if (DEBUG && debugflag){

      Current->startedTime = sys_clock();

      if (Current->startedTime >= 80000)
      {
         dispatcher();
      }

      enableInterrupts();
   }

}