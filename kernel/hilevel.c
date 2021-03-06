/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#include <stdio.h>
#include <stdlib.h>

/* We assume there will be two user processes, stemming from execution of the 
 * two user programs P1 and P2, and can therefore
 * 
 * - allocate a fixed-size process table (of PCBs), and then maintain an index 
 *   into it to keep track of the currently executing process, and
 * - employ a fixed-case of round-robin scheduling: no more processes can be 
 *   created, and neither can be terminated, so assume both are always ready
 *   to execute.
 */


pcb_t procTab[ MAX_PROCS ];        // PCB table
pcb_t* executing = NULL;           // Pointer to currently executing PCB
mlf_queues mlfq;                   // Multi-level feedback queue structure
int next_pid;                      // PID counter to ensure unique PIDs
bool available_stacks[MAX_PROCS];  // Free stack space table

/* The following functions are related to the scheduling and execution of processes */

// Resume/ begin execution of a process by the processor
void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  mlfq.timeCount = 0; // reset process execution timer

  char prev_pid = '?', next_pid = '?';

  if (prev == next) { return; }

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of process
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore execution context of process
    next_pid = '0' + next->pid;
  }

   /* PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );
	*/

    executing = next;                           // update executing process

  return;
}

// create queue node for given PCB
node* newNode(pcb_t* pcb) { 
    node* temp = (node*)malloc(sizeof(node)); 
    temp->pcb = pcb; 
    temp->next = NULL; 
    return temp; 
} 

bool isEmpty(queue* q) {
	return q->tail == NULL;
}

// place PCB node onto end of given queue
void enqueue(queue* q, pcb_t* pcb) {
    node* temp = newNode(pcb);

    if (isEmpty(q)) {
        q->head = q->tail = temp;
        return;
    }

    q->tail->next = temp;
    q->tail = temp;
}

// remove head node from given queue
void dequeue(queue* q) {
	if (isEmpty(q)) {
		return;
	}

    node* temp = q->head;
	q->head = q->head->next;

	if (q->head == NULL) {
		q->tail = NULL;
	}

	free(temp);
}

// delete PCB node from any point in a queue
void delPCBNode(queue* q, pcb_t* pcb) {

	if (pcb->status != STATUS_READY) {return;} // only PCBs with ready status are in a queue

	if (q->head->pcb == pcb) {
		dequeue(q);
		return;
	}

	node* temp1 = q->head;
	node* temp2;

	while (temp1->next->pcb != pcb) {
		temp1 = temp1->next;
	}

	temp2 = temp1->next;
	temp1->next = temp1->next->next;
	
	if (temp1->next == NULL) {
		q->tail = temp1;
	}

	free(temp2);
}

// retrieve highest priority PCB node from within the multi-level queue structure
node* mlfqHighestNode(mlf_queues* mlfq) {
	for (int i = 0; i < PRIORITY_LEVELS; i++) {
		if (!isEmpty(&mlfq->queues[i])) {
	      return mlfq->queues[i].head;
		}
	}
	return NULL;
}

// Place given PCB node (that has just finished being executed) into a queue 
void reQueue(pcb_t* pcb) {

	prty_t prev_prty = pcb->prty;
	prty_t next_prty;
	if (pcb->prty < 3) { 
		next_prty = pcb->prty + 1;
		pcb->prty++;
	}
	else {
		next_prty = pcb->prty;
	}
	enqueue(&mlfq.queues[next_prty-1], pcb);
}

// Scheduler
void multiLevelFeedbackSchedule(ctx_t* ctx){
	int prev = -1;	

	// Get previous process pcb
	for (int i = 0; i < MAX_PROCS; i++) {
		if (executing->pid == procTab[i].pid) {
			prev = i;
		}
	}

   	// if no previous process, dispatch next highest priority process
	if (prev == -1) {		
		node* top = mlfqHighestNode(&mlfq);
		dispatch(ctx, NULL, top->pcb);
		dequeue(&mlfq.queues[top->pcb->prty-1]);
		executing->status = STATUS_EXECUTING;
		return;
	}
	
	// increment no. time slices used by process
	mlfq.timeCount++;  
	
	// Check process has not used up allocated time slices at current priority level
	if (mlfq.timeCount < mlfq.queueTime[procTab[prev].prty-1]) { return; }

	// If process has used allocated time slice, requeue and dispatch next
	// highest priority process
	reQueue(&procTab[prev]);
	node* newTop = mlfqHighestNode(&mlfq);
	dispatch(ctx, &procTab[prev], newTop->pcb); 
	dequeue(&mlfq.queues[newTop->pcb->prty-1]);
	procTab[prev].status = STATUS_READY;
	executing->status = STATUS_EXECUTING;

	return;
}

void initMLFS(ctx_t* ctx) {
  // place all initialised processes into correct priority queue

  for (int i = 0; i < MAX_PROCS; i++) {
    if (procTab[i].status != STATUS_INVALID) {
      for (int j = 0; j < PRIORITY_LEVELS; j++) {
	    if (procTab[i].prty == j) { enqueue(&mlfq.queues[j-1], &procTab[i]);}
	  }	
	}
  }

  // Each priority queue is assigned double the time slot for processes than the queue above
  mlfq.queueTime[0] = 1;
  for (int i = 1; i < PRIORITY_LEVELS; i++) {
	  mlfq.queueTime[i] = mlfq.queueTime[i-1]*2;
  }

  mlfq.timeCount = 0;
}

/* The following functions are related to the use of the disk */

// convert 4 bytes to uint32
uint32_t b2u32(uint8_t *in) {
	uint32_t out = 0;
	for (int i = 0; i<4; i++) {
		out = out | ((uint32_t)(in[i]) << i*8);
	}
	return out;
}

// convert 2 bytes to uint16
uint16_t b2u16(uint8_t *in) {
	uint16_t out = 0;
	for (int i = 0; i<2; i++) {
		out = out | ((uint16_t)(in[i]) << i*8);
	}
	return out;
}

// read in a super block struct
s_block *readInSBlock() {
	int size = sizeof(s_block);
	uint8_t data[size];
	if (DISK_SUCCESS == disk_rd(0, data, size)) {
		PL011_putc(UART1,'P',true);
	}
	else {PL011_putc(UART1,'O',true);}

	s_block *s = malloc(sizeof(s_block));
   	s->inode_count = data[0];
	s->root_inode = data[4];

	return s;
}

// retrieve the inode count stored in the sblock
uint32_t getInodeCount(){
	/*s_block *s = readInSBlock();
	uint32_t y = s->inode_count;
	free(s);
*/
	uint8_t data[4];
	disk_rd(0,data,4);
	uint32_t y = data[0];
	return y;
}

// write an sblock struct to the sblock on disk
void writeSBlock(s_block *s) {
	uint8_t write[BLOCK_SIZE];
    write[0] = s->inode_count;
	write[4] = s->root_inode;
	for (int i = 8; i<BLOCK_SIZE; i++){
		write[i] = 0;
	}
	if (DISK_SUCCESS == disk_wr(0, write, BLOCK_SIZE)) {
		PL011_putc(UART1,'S',true);
	}
	else { PL011_putc(UART1,'N',true);}
	free(s);
}

// increment the inode count in the disk sblock
void incInodeCount(){
	s_block *s = readInSBlock();
	s->inode_count++;
 	writeSBlock(s);
}	

extern uint32_t p_stack_space;
extern void main_console();

void hilevel_handler_rst( ctx_t* ctx              ) { 
    // Configure interrupt handling mechanism

    TIMER0->Timer1Load  = 0x00001000; // select period
	TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
    TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
	TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
	TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer
	
	GICC0->PMR          = 0x000000F0; // unmask all            interrupts
	GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
	GICC0->CTLR         = 0x00000001; // enable GIC interface
	GICD0->CTLR         = 0x00000001; // enable GIC distributor
	
	int_enable_irq();

  /* Invalidate all entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for( int i = 0; i < MAX_PROCS; i++ ) {
    procTab[ i ].status = STATUS_INVALID;
	available_stacks[i] = true;
  }

  /* Automatically execute the console:
   * - the CPSR value of 0x50 means the processor is switched into USR mode, 
   *   with IRQ interrupts enabled
   * - the PC and SP values match the entry point and top of the stack space. 
   */

  next_pid = 1;

  memset( &procTab[ 0 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = console
  procTab[ 0 ].pid      = next_pid++;
  procTab[ 0 ].status   = STATUS_READY;
  procTab[ 0 ].tos      = ( uint32_t )( &p_stack_space );
  procTab[ 0 ].prty     = 1;
  procTab[ 0 ].ctx.cpsr = 0x50;
  procTab[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  procTab[ 0 ].ctx.sp   = procTab[ 0 ].tos;

  available_stacks[0] = false; // the top stack area in the stack space is now being used

  // Initialise the feedback queue and start scheduling
  initMLFS(ctx);
  multiLevelFeedbackSchedule(ctx);  

  return;
}

// Find an empty stack area in the program stack space
uint32_t getNextStack(){
	for (int i = 0; i < MAX_PROCS; i++) {
		if (available_stacks[i] == true) { 
			available_stacks[i] = false;
			return ((uint32_t) &p_stack_space - i*STACK_SIZE);
		}
	}
}


void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) { 
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction, 
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      multiLevelFeedbackSchedule( ctx );
      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );  
      char*  x = ( char* )( ctx->gpr[ 1 ] );  
      int    n = ( int   )( ctx->gpr[ 2 ] ); 

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }
      
      ctx->gpr[ 0 ] = n;

      break;
    }
	
	case 0x03 : { //0x03 => fork()

	  // print pid of next function
      PL011_putc( UART0, next_pid, true );
	  

	  // get unused PCB from procTab
	  int free_pcb = -1;
	  for (int i = 0; i < MAX_PROCS; i++) {
		if (procTab[i].status == STATUS_INVALID) {
			free_pcb = i;
			break;
		}
	  }

	  // if no free PCBs, (i.e. MAX_PROCS reached, return error
	  if (free_pcb == -1) {
		  ctx -> gpr[0] = -1;
		  break;
	  }

	  // procTab[free_pcb] = *executing;
	  memcpy( &procTab[free_pcb].ctx, ctx, sizeof(ctx_t));

  	  procTab[ free_pcb ].pid        = next_pid++;
  	  procTab[ free_pcb ].status     = STATUS_READY;
 	  procTab[ free_pcb ].prty       = 1;
	  procTab[ free_pcb ].ctx.gpr[0] = 0; // fork() returns 0 to child process

	  // create and place PCB node for new process
	  enqueue(&mlfq.queues[procTab[free_pcb].prty-1],&procTab[free_pcb]);

	  // fork() returns pid to parent process
	  ctx -> gpr[0] = procTab[free_pcb].pid;

	  // get address of top of new stack space
	  procTab[free_pcb].tos = getNextStack();

	  // copy stack and correctly place stack pointer
	  int offset = executing->tos - ctx->sp;
	  procTab[free_pcb].ctx.sp = procTab[free_pcb].tos - offset;
	  memcpy((uint32_t*) procTab[free_pcb].ctx.sp, (uint32_t*) ctx->sp, offset);

	  break;
	}
	
	case 0x04 : { // 0x04 => exit(success?) 
	  for(int i = 0; i < MAX_PROCS; i++) {
		if (procTab[i].pid == executing->pid) {
	      memset( &procTab[i], 0, sizeof(pcb_t) ); // reset PCB
          procTab[i].status = STATUS_INVALID;	// PCB available to be used		
		}
	  }
	  executing = NULL;
	  multiLevelFeedbackSchedule(ctx);
	  break;
	}

	case 0x05 : { // 0x05 => exec(addr)
	  
	  // get address of process main function to execute
	  uint32_t addr = (uint32_t)ctx->gpr[0];

 	  ctx->pc = addr;
	  ctx->sp = executing->tos;

	  break;
	}

	case 0x06 : { // 0x06 => kill(pid, x)
		
	  int pid = (int)ctx->gpr[0];
	  int x = (int)ctx->gpr[1];

	  if (pid == 0) { // terminate all processes except console
		for (int i = 1; i < MAX_PROCS; i++) {
		  delPCBNode(&mlfq.queues[procTab[i].prty-1], &procTab[i]); // remove process from queue
		  memset( &procTab[i], 0, sizeof(pcb_t) ); // reset PCB
		  procTab[i].status = STATUS_INVALID;	// PCB available to be used		
		} 
	  }

	  if (x == 0 | x == 1) { 
		// terminate process
		for (int i = 0; i < MAX_PROCS; i++) { 
		  if (procTab[i].pid == pid) {
			delPCBNode(&mlfq.queues[procTab[i].prty-1], &procTab[i]); // remove process from queue
	  	 	memset( &procTab[i], 0, sizeof(pcb_t) ); // reset PCB
			procTab[i].status = STATUS_INVALID;	// PCB available to be used		
		  }
		}
	  }

	  ctx->gpr[0] = 0; // if success
	  break;
	}
	
	case 0x08 : { // sem_init( id )
	  uint32_t* sem = malloc(sizeof(uint32_t));
	  *sem = ctx->gpr[0];
	  ctx->gpr[0] = (uint32_t) sem;
	  break;
	}
	
	case 0x09 : { // sem_close ( *sem )
	  free((uint32_t*)ctx->gpr[0]);	
	  break;
	}
/*	
	case 0x10 : { // new_inode

	}
	
	case 0x11 : { // 

	}
	
	case 0x11 : { // open

	}

	case 0x10 : { // mkdir

	}
*/
    default   : {
      break;
    }
  }

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
   // Read  the interrupt identifier so we know the source.

   uint32_t id = GICC0->IAR;

   // Handle the interrupt, then clear source.

   if( id == GIC_SOURCE_TIMER0 ) {
	   multiLevelFeedbackSchedule(ctx);
	   TIMER0->Timer1IntClr = 0x01;
   }

   // Write to the interrupt identifier to signal we're done.

   GICC0->EOIR = id;

   return;
}
