#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "globals.h"
#include "os.h"

void os_init();
void create_thread(uint16_t address, void* args, uint16_t stack_size);
void os_start();
uint8_t get_next_thread();
void start_system_timer();
__attribute__((naked)) void context_switch(uint16_t* new_tp, uint16_t* old_tp);
__attribute__((naked)) void thread_start(void);
struct system_t* getSystemInfo();
int getCurrentThread();
void setThreadState(int threadNum, threadState_t state);
void threadSwap(int newThread, int oldThread);
void thread_sleep(uint16_t ticks);

#define STACK_BUFFER 64
struct system_t sysInfo;

//Any OS specific initialization code
void os_init()
{
   serial_init();
   sysInfo.curThread = 0;
   sysInfo.numThreads = 0;
   sysInfo.interrupts = 0;
   sysInfo.runtime = 0;
}

//Start running the OS
void os_start()
{
   start_system_timer();
   //Save the spot after main for infite looping
   struct thread_t loopThread;
   sysInfo.threads[sysInfo.numThreads] = loopThread;
   context_switch(&sysInfo.threads[sysInfo.curThread].stackPtr, &sysInfo.threads[sysInfo.numThreads].stackPtr);
}

//Returns a pointer to the system info struct
struct system_t* getSystemInfo()
{
   return &sysInfo;
}

//Sets the thread state of the given thread to the given state
void setThreadState(int threadNum, threadState_t state)
{
   sysInfo.threads[threadNum].state = state;
}

//Returns the index of the current thread
int getCurrentThread()
{
   return sysInfo.curThread;
}

//Swaps the 2 given threads
void threadSwap(int newThread, int oldThread)
{
   context_switch(&sysInfo.threads[newThread].stackPtr, &sysInfo.threads[oldThread].stackPtr);
}

//Puts the current thread to sleep for |tick| interrupts
void thread_sleep(uint16_t ticks)
{
   sysInfo.threads[sysInfo.curThread].state = THREAD_SLEEPING;
   sysInfo.threads[sysInfo.curThread].sleepCount = ticks;
   context_switch(&sysInfo.threads[get_next_thread()].stackPtr, 
      &sysInfo.threads[sysInfo.curThread].stackPtr);
}

//The current thread releases the CPU for the next thread
void yield()
{
   sysInfo.threads[sysInfo.curThread].state = THREAD_READY;
   while(sysInfo.threads[sysInfo.curThread++].state != THREAD_READY && sysInfo.curThread < sysInfo.numThreads);

   context_switch(&sysInfo.threads[sysInfo.curThread].stackPtr, 
      &sysInfo.threads[sysInfo.curThread - 1].stackPtr);
}

//The current thread is blocked, let the next go
void blocked()
{
   sysInfo.threads[sysInfo.curThread++].state = THREAD_WAITING;

   context_switch(&sysInfo.threads[sysInfo.curThread].stackPtr, 
      &sysInfo.threads[sysInfo.curThread - 1].stackPtr);
}

/* Creates the thread stack for a new thread
 * address - address of the function for this thread
 * args - pointer to function arguments
 * stack_size - size of thread stack in bytes (does not include stack space to save registers)
*/
void create_thread(uint16_t address, void* args, uint16_t stack_size)
{
   struct thread_t thread;
   if(sysInfo.numThreads < MAX_THREADS)
   {
      thread.id = sysInfo.numThreads;
      thread.stackSize = stack_size + sizeof(struct regs_context_switch)
       + sizeof(struct regs_interrupt) + STACK_BUFFER;
      thread.stackBase = (uint16_t) malloc(thread.stackSize);
      thread.pcStart = address;
      thread.state = THREAD_READY;
      thread.sleepCount = 0;
      
      //Set up the stack
      //Move stack pointer up so theres only room for manual regs and a PC
      thread.stackPtr = thread.stackBase + thread.stackSize - 1;
      thread.stackPtr -= sizeof(struct regs_context_switch);

      //PC gets the address of thread start
      ((struct regs_context_switch *)thread.stackPtr)->pcl = (uint16_t)thread_start & 0xFF;
      ((struct regs_context_switch *)thread.stackPtr)->pch = ((uint16_t)thread_start & 0xFF00) >> 8;

      //Address in R2:R3
      ((struct regs_context_switch *)thread.stackPtr)->r2 = address & 0xFF;
      ((struct regs_context_switch *)thread.stackPtr)->r3 = (address & 0xFF00) >> 8;
      
      //Args go in R4:R5
      ((struct regs_context_switch *)thread.stackPtr)->r4 = (uint16_t)args & 0xFF;
      ((struct regs_context_switch *)thread.stackPtr)->r5 = ((uint16_t)args & 0xFF00) >> 8;

      sysInfo.threads[sysInfo.numThreads++] = thread;
   }
   return;
}

// Return the id of the next thread to run always thread 0, priority
uint8_t get_next_thread()
{
   sysInfo.curThread = 0;
   while(sysInfo.threads[sysInfo.curThread].state != THREAD_READY && sysInfo.curThread < sysInfo.numThreads)
      sysInfo.curThread++;

   return sysInfo.curThread;
}

//Update any sleeping threads
void updateSleep()
{
   int i;
   for(i; i < sysInfo.numThreads; i++)
   {
      if(sysInfo.threads[i].state == THREAD_SLEEPING)
      {
         if(--sysInfo.threads[i].sleepCount == 0)
            sysInfo.threads[i].state = THREAD_READY;
      }
   }
}

//This interrupt routine is automatically run every 10 milliseconds
ISR(TIMER0_COMPA_vect) {
   volatile int current = sysInfo.curThread;
   //print_string("Interrupt on ");
   //print_int(current);
   //The following statement tells GCC that it can use registers r18-r27, 
   //and r30-31 for this interrupt routine.  These registers (along with
   //r0 and r1) will automatically be pushed and popped by this interrupt routine.
   asm volatile ("" : : : "r18", "r19", "r20", "r21", "r22", "r23", "r24", \
                 "r25", "r26", "r27", "r30", "r31");                        

   sysInfo.interrupts++;
   updateSleep();
   
   sysInfo.threads[current].state = THREAD_READY;
   context_switch(&sysInfo.threads[get_next_thread()].stackPtr, 
      &sysInfo.threads[current].stackPtr);
}

//This interrupt routine is run once a second
ISR(TIMER1_COMPA_vect) {
   sysInfo.runtime++;
}

//New start system timer for program 5
void start_system_timer() {
   TIMSK0 |= _BV(OCIE0A);  /* IRQ on compare.  */
   TCCR0A |= _BV(WGM01); //clear timer on compare match

   //22KHz settings
   TCCR0B |= _BV(CS01) | _BV(CS01); //slowest prescalar /1024
   OCR0A = 180; 

   //start timer 1 to generate interrupt every 1 second
   OCR1A = 15625;
   TIMSK1 |= _BV(OCIE1A);  /* IRQ on compare.  */
   TCCR1B |= _BV(WGM12) | _BV(CS12) | _BV(CS10); //slowest prescalar /1024
}

//Start pulse wave modulation
void start_audio_pwm() {
   //run timer 2 in fast pwm mode
   TCCR2A |= _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
   TCCR2B |= _BV(CS20);

   DDRD |= _BV(PD3); //make OC2B an output
}

__attribute__((naked)) void context_switch(uint16_t* new_tp, uint16_t* old_tp) 
{
   //Save regs
   asm volatile("push r2");
   asm volatile("push r3");
   asm volatile("push r4");
   asm volatile("push r5");
   asm volatile("push r6");
   asm volatile("push r7");
   asm volatile("push r8");
   asm volatile("push r9");
   asm volatile("push r10");
   asm volatile("push r11");
   asm volatile("push r12");
   asm volatile("push r13");
   asm volatile("push r14");
   asm volatile("push r15");
   asm volatile("push r16");
   asm volatile("push r17");
   asm volatile("push r28");
   asm volatile("push r29");
   
   //Switch stackptrs
   //Set up Z to point to SP
   asm volatile("ldi r31, 0x00");
   asm volatile("ldi r30, 0x5d");
   //Put stack pointer into r16:17
   asm volatile("ld r16, z+");
   asm volatile("ld r17, z");

   //Set up Z to point to arg2
   asm volatile("movw r30, r22");
   //Store stack pointer from r16:17 into arg2
   asm volatile("st z+, r16");
   asm volatile("st z, r17");

   //Set up Z to point to arg1
   asm volatile("movw r30, r24");
   //Put that value into r16:17
   asm volatile("ld r16, z+");
   asm volatile("ld r17, z");

   //Set up Z to point to SP
   asm volatile("ldi r31, 0x00");
   asm volatile("ldi r30, 0x5d");
   //Store new stack pointer from r16:17 into real stack pointer
   asm volatile("st z+, r16");
   asm volatile("st z, r17");

   //Pop regs
   asm volatile("pop r29");
   asm volatile("pop r28");
   asm volatile("pop r17");
   asm volatile("pop r16");
   asm volatile("pop r15");
   asm volatile("pop r14");
   asm volatile("pop r13");
   asm volatile("pop r12");
   asm volatile("pop r11");
   asm volatile("pop r10");
   asm volatile("pop r9");
   asm volatile("pop r8");
   asm volatile("pop r7");
   asm volatile("pop r6");
   asm volatile("pop r5");
   asm volatile("pop r4");
   asm volatile("pop r3");
   asm volatile("pop r2");
   
   //Pop PC, with ret
   asm volatile("ret");
}

__attribute__((naked)) void thread_start(void) {
   sei(); //enable interrupts - leave this as the first statement in thread_start()
   //Set up Z to point to address of thread code
   asm volatile("movw r30, r2");
   asm volatile("ijmp");
   return;
}