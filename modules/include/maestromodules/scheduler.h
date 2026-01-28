/** FORMERLY KNOWN AS SMW
 * Simple task scheduler util */

#ifndef _scheduler_h_
#define _scheduler_h_

#include <bits/pthreadtypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <maestroutils/error.h>
#include <maestroutils/misc_utils.h>
#include <maestroutils/time_utils.h>

#ifndef SCHEDULER_MAX_TASKS
#define SCHEDULER_MAX_TASKS 10000
#endif

#define MIN_LOOP_MS 1 // Defines how many ms a scheduler task-loop needs to take at a minimum

typedef struct
{
  void* context;
  /*replace callback with work function from other module?*/
  void (*callback)(void* _context);

} Scheduler_Task;

typedef struct
{
  Scheduler_Task tasks[SCHEDULER_MAX_TASKS];

} Scheduler;

extern Scheduler Global_Scheduler;

int scheduler_init();
Scheduler_Task* scheduler_create_task(void* _context,
                                      void (*_callback)(void* _context));
void scheduler_destroy_task(Scheduler_Task* _Task);
void scheduler_work();
int scheduler_get_task_count(int _Scheduler_index);
void scheduler_dispose();

#endif
