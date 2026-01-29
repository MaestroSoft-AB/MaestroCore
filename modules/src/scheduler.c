#include <maestromodules/scheduler.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
  int num_threads_schedulers;

  pthread_t* threads;
  Scheduler* schedulers;

} SchedulerThreads;

/* ----------------------- Global vars ----------------------- */

SchedulerThreads g_schedulerthreads;
/* const uint64_t min_loop_ms = MIN_LOOP_MS; // Defines how many ms a scheduler task-loop needs to
 * take at a minimum */

/* ----------------------------------------------------------- */

/*Check connections and change timeout depending on amount*/

/* ---------------------- Thread specific -------------------- */

void scheduler_thread_work(Scheduler* _Scheduler)
{
  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    if (_Scheduler->tasks[i].callback) {
      uint64_t start = SystemMonotonicMS();
      _Scheduler->tasks[i].callback(_Scheduler->tasks[i].context);
      uint64_t end = SystemMonotonicMS();

      uint64_t elapsed = end - start;

      if (elapsed < MIN_LOOP_MS) {
        ms_sleep(MIN_LOOP_MS - elapsed);
      }
      /* TODO: dynamic MIN_LOOP_MS depending on amount of connections */
    }
  }
}

void* scheduler_thread_init(void* _Args)
{
  Scheduler* scheduler = (Scheduler*)_Args;

  memset(scheduler, 0, sizeof(Scheduler));

  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    scheduler->tasks[i].context = NULL;
    scheduler->tasks[i].callback = NULL;
  }

  scheduler_thread_work(scheduler);

  return NULL;
}

/* ----------------------------------------------------------- */

int scheduler_init()
{
  g_schedulerthreads.num_threads_schedulers =
      sysconf(_SC_NPROCESSORS_ONLN) - 1; // Number of threads minus one for OS headroom

  g_schedulerthreads.threads =
      (pthread_t*)malloc(sizeof(pthread_t) * g_schedulerthreads.num_threads_schedulers);
  g_schedulerthreads.schedulers =
      (Scheduler*)malloc(sizeof(Scheduler) * g_schedulerthreads.num_threads_schedulers);

  for (int i = 0; i < g_schedulerthreads.num_threads_schedulers; i++) {
    pthread_create(&g_schedulerthreads.threads[i], NULL, scheduler_thread_init,
                   &g_schedulerthreads.schedulers[i]);
  }

  return 0;
}

// Todo: scheduler_get_task_count needs to return index of least busy worker so that
// scheduler_add_task can delegate task to to thread.

Scheduler_Task* scheduler_create_task(void* _context, void (*_callback)(void* _context))
{
  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    if (Global_Scheduler.tasks[i].context == NULL && Global_Scheduler.tasks[i].callback == NULL) {
      Global_Scheduler.tasks[i].context = _context;
      Global_Scheduler.tasks[i].callback = _callback;
      return &Global_Scheduler.tasks[i];
    }
  }

  return NULL;
}

// Todo: add scheduler_add_task function for "hivemind" to assign a task to thread.
// scheduler_get_task_count sends index of next in line.

void scheduler_destroy_task(Scheduler_Task* _Task)
{
  if (_Task == NULL)
    return;

  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    if (&Global_Scheduler.tasks[i] == _Task) {
      Global_Scheduler.tasks[i].context = NULL;
      Global_Scheduler.tasks[i].callback = NULL;
      break;
    }
  }
}

int scheduler_get_task_count(int _Scheduler_index)
{
  if (_Scheduler_index < 0 || _Scheduler_index > g_schedulerthreads.num_threads_schedulers - 1) {
    return -1;
  }

  int tasks = 0;
  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    if (g_schedulerthreads.schedulers[_Scheduler_index].tasks[i].callback != NULL) {
      tasks++;
    }
  }

  return tasks;
}

void scheduler_dispose() // TODO: Add free() for threads and schedulers as outlined in
                         // scheduler_ini()
{
  int i;
  for (i = 0; i < SCHEDULER_MAX_TASKS; i++) {
    Global_Scheduler.tasks[i].context = NULL;
    Global_Scheduler.tasks[i].callback = NULL;
  }
}