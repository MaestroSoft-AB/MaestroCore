#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

/* Simple thread*/

/* ======================== STRUCTS ======================== */

typedef struct {
  void (*thread_func)(void*); 
  void* thread_arg;
  void (*callback_func)(void*); 
  void* callback_arg;

} TP_Task;

typedef struct Thread_Pool Thread_Pool;

/* ======================= INTERFACE ======================= */

/* Initializes a thread pool with given amount of threads */
Thread_Pool* tp_init(int _max_threads);

/* Adds a task to the thread pool queue to be executed when a 
 * thread becomes available */
int tp_task_add(Thread_Pool* _Pool, TP_Task* _Task);

/** Helper for caller to wait for all active tasks to finish
 * for example to let them catch up before disposing */
void tp_wait(Thread_Pool* _Pool);

/* Destroys the pool and its threads */
void tp_dispose(Thread_Pool* _Pool);

#endif
