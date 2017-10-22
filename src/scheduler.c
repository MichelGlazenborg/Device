#include <avr/io.h>
#include <avr/interrupt.h>
#include "scheduler.h"

// The array of tasks
Task sch_tasks_g[SCH_MAX_TASKS];

/*------------------------------------------------------------------*-

  sch_dispatch_tasks()

  This is the 'dispatcher' function.  When a task (function)
  is due to run, sch_dispatch_tasks() will run it.
  This function must be called (repeatedly) from the main loop.

-*------------------------------------------------------------------*/

void sch_dispatch_tasks(void)
{
   // Dispatches (runs) the next task (if one is ready)
   for (unsigned char index = 0; index < SCH_MAX_TASKS; index++)
   {
      if ((sch_tasks_g[index].runme > 0) && (sch_tasks_g[index].pTask != 0))
      {
         (*sch_tasks_g[index].pTask)();  // Run the task
         sch_tasks_g[index].runme--;   // Reset / reduce runme flag

         // Periodic tasks will automatically run again
         // - if this is a 'one shot' task, remove it from the array
         if (sch_tasks_g[index].period == 0)
         {
            sch_delete_task(index);
         }
      }
   }
}

/*------------------------------------------------------------------*-

  sch_add_task()

  Causes a task (function) to be executed at regular intervals 
  or after a user-defined delay

  pFunction - The name of the function which is to be scheduled.
              NOTE: All scheduled functions must be 'void, void' -
              that is, they must take no parameters, and have 
              a void return type. 
                   
  DELAY     - The interval (TICKS) before the task is first executed

  PERIOD    - If 'PERIOD' is 0, the function is only called once,
              at the time determined by 'DELAY'.  If PERIOD is non-zero,
              then the function is called repeatedly at an interval
              determined by the value of PERIOD (see below for examples
              which should help clarify this).


  RETURN VALUE:  

  Returns the position in the task array at which the task has been 
  added.  If the return value is SCH_MAX_TASKS then the task could 
  not be added to the array (there was insufficient space).  If the
  return value is < SCH_MAX_TASKS, then the task was added 
  successfully.  

  Note: this return value may be required, if a task is
  to be subsequently deleted - see sch_delete_task().

  EXAMPLES:

  Task_ID = sch_add_task(Do_X,1000,0);
  Causes the function Do_X() to be executed once after 1000 sch ticks.            

  Task_ID = sch_add_task(Do_X,0,1000);
  Causes the function Do_X() to be executed regularly, every 1000 sch ticks.            

  Task_ID = sch_add_task(Do_X,300,1000);
  Causes the function Do_X() to be executed regularly, every 1000 ticks.
  Task will be first executed at T = 300 ticks, then 1300, 2300, etc.            
 
-*------------------------------------------------------------------*/

unsigned char sch_add_task(void (*pFunction)(), const unsigned int DELAY, const unsigned int PERIOD)
{
   unsigned char index = 0;

   // First find a gap in the array (if there is one)
   while ((sch_tasks_g[index].pTask != 0) && (index < SCH_MAX_TASKS))
   {
      index++;
   }

   // Have we reached the end of the list?   
   if (index == SCH_MAX_TASKS)
   {
      // Task list is full, return an error code
      return SCH_MAX_TASKS;  
   }

   // If we're here, there is a space in the task array
   sch_tasks_g[index].pTask = pFunction;
   sch_tasks_g[index].delay = DELAY;
   sch_tasks_g[index].period = PERIOD;
   sch_tasks_g[index].runme = 0;

   // return position of task (to allow later deletion)
   return index;
}

/*------------------------------------------------------------------*-

  sch_delete_task()

  Removes a task from the scheduler.  Note that this does
  *not* delete the associated function from memory: 
  it simply means that it is no longer called by the scheduler. 
 
  TASK_INDEX - The task index.  Provided by sch_add_task(). 

  RETURN VALUE:  RETURN_ERROR or RETURN_NORMAL

-*------------------------------------------------------------------*/

unsigned char sch_delete_task(const unsigned char TASK_INDEX)
{
   // Return_code can be used for error reporting, NOT USED HERE THOUGH!
   unsigned char return_code = 0;

   sch_tasks_g[TASK_INDEX].pTask = 0;
   sch_tasks_g[TASK_INDEX].delay = 0;
   sch_tasks_g[TASK_INDEX].period = 0;
   sch_tasks_g[TASK_INDEX].runme = 0;

   return return_code;
}

/*------------------------------------------------------------------*-

  sch_init_t1()

  Scheduler initialisation function.  Prepares scheduler
  data structures and sets up timer interrupts at required rate.
  You must call this function before using the scheduler.  

-*------------------------------------------------------------------*/

void sch_init_t1(void)
{
   unsigned char i;

   for (i = 0; i < SCH_MAX_TASKS; i++)
   {
      sch_delete_task(i);
   }

   // Set up Timer 1
   // Values for 1ms and 10ms ticks are provided for various crystals

   OCR1A = (uint16_t) 625;                  // 10ms = (256/16.000.000) * 625
   TCCR1B = (1 << CS12) | (1 << WGM12);     // prescale op 64, top counter = value OCR1A (CTC mode)
   TIMSK1 = 1 << OCIE1A;                    // Timer 1 Output Compare A Match Interrupt Enable
}

/*------------------------------------------------------------------*-

  sch_start()

  Starts the scheduler, by enabling interrupts.

  NOTE: Usually called after all regular tasks are added,
  to keep the tasks synchronised.

  NOTE: ONLY THE SCHEDULER INTERRUPT SHOULD BE ENABLED!!! 
 
-*------------------------------------------------------------------*/

void sch_start(void)
{
      sei();
}

/*------------------------------------------------------------------*-

  SCH_Update

  This is the scheduler ISR.  It is called at a rate 
  determined by the timer settings in sch_init_t1().

-*------------------------------------------------------------------*/

ISR(TIMER1_COMPA_vect)
{
   unsigned char index;
   for (index = 0; index < SCH_MAX_TASKS; index++)
   {
      // Check if there is a task at this location
      if (sch_tasks_g[index].pTask)
      {
         if (sch_tasks_g[index].delay == 0)
         {
            // The task is due to run, Inc. the 'RunMe' flag
            sch_tasks_g[index].runme += 1;

            if (sch_tasks_g[index].period)
            {
               // Schedule periodic tasks to run again
               sch_tasks_g[index].delay = sch_tasks_g[index].period;
               sch_tasks_g[index].delay -= 1;
            }
         }
         else
         {
            // Not yet ready to run: just decrement the delay
            sch_tasks_g[index].delay -= 1;
         }
      }
   }
}
