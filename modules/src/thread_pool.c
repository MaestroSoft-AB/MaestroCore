#include "maestromodules/thread_pool.h"
#include "maestromodules/linked_list.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* --------------------------- Internal --------------------------- */

struct Thread_Pool 
{
  pthread_mutex_t mutex;
  pthread_cond_t  task_added;
  pthread_cond_t  task_finished;

  pthread_t*      threads;
  Linked_List*    queue;

  int             max_threads;
  int             active_tasks;

  bool            stop; // for graceful shutdown
};

static void* tp_worker(void* _pool_ptr)
{
  Thread_Pool* Pool = (Thread_Pool*)_pool_ptr;

  while (1) {
    pthread_mutex_lock(&Pool->mutex);

    /* Unblocking mutex with cond_wait until there is a task in queue */
    while (Pool->queue->count == 0 && Pool->stop == false)
      pthread_cond_wait(&Pool->task_added, &Pool->mutex);
    
    /* Shutdown on stop=true */
    if (Pool->queue->count == 0 && Pool->stop == true) {
      pthread_mutex_unlock(&Pool->mutex);
      break;
    }

    /* Fetch task from first queue item and remove item from queue */
    TP_Task* Task = Pool->queue->head->item;
    if (Task) linked_list_item_remove(Pool->queue, Pool->queue->head);

    Pool->active_tasks++;

    /* Unlock mutex and run task */
    pthread_mutex_unlock(&Pool->mutex);
    if (Task) {

      Task->thread_func(Task->thread_arg);
      if (Task->callback_func)
        Task->callback_func(Task->callback_arg);
      free(Task);
    }

    /* Decrement active tasks and signal finish */
    pthread_mutex_lock(&Pool->mutex);
    Pool->active_tasks--;
    pthread_cond_signal(&Pool->task_finished);
    pthread_mutex_unlock(&Pool->mutex);

  }
  return NULL;
}

/* ---------------------------------------------------------------- */

Thread_Pool* tp_init(int _max_threads)
{
  int i;
  if (_max_threads <= 0) _max_threads = 2; // minimum max threads

  /* Allocate pool */
  Thread_Pool* Pool = calloc(1, sizeof(Thread_Pool));
  if (!Pool) {
    perror("calloc");
    return NULL;
  }

  /* Allocate threads */
  Pool->max_threads = _max_threads;
  Pool->threads = calloc(1, Pool->max_threads * (sizeof(pthread_t)));
  if (!Pool->threads) {
    perror("calloc");
    free(Pool);
    return NULL;
  }

  /* Initialize mutex and condition */
  pthread_mutex_init(&Pool->mutex, NULL);
  pthread_cond_init(&Pool->task_added, NULL);
  pthread_cond_init(&Pool->task_finished, NULL);

  /* Initialize queue */
  Pool->queue = linked_list_create();
  if (!Pool->queue) {
    perror("linked_list_create");
    free(Pool->threads);
    free(Pool);
    return NULL;
  }

  /* Create threads */
  for (i = 0; i < _max_threads; i++)
  {
    if (pthread_create(&Pool->threads[i], NULL, tp_worker, Pool)) {
      perror("pthread_create");
      for (int y = 0; y < i; y++)
        pthread_join(Pool->threads[y], NULL);
      free(Pool->threads);
      free(Pool);
      return NULL;
    }
  }

  Pool->active_tasks = 0;
  Pool->stop = false;

  return Pool;
}

int tp_task_add(Thread_Pool* _Pool, TP_Task* _Task)
{
  if (!_Pool || !_Task || !_Task->thread_func)
    return -1;

  /* Make persistent copy of task */
  TP_Task* Task = malloc(sizeof(TP_Task));
  if (!Task) {
    perror("malloc");
    return -10;
  }
  Task->callback_func = _Task->callback_func;
  Task->callback_arg = _Task->callback_arg;
  Task->thread_func = _Task->thread_func;
  Task->thread_arg = _Task->thread_arg;

  /* Lock mutex and add task to queue */
  pthread_mutex_lock(&_Pool->mutex);
  if (linked_list_item_add(_Pool->queue, NULL, Task) != 0) {
    perror("linked_list_item_add");
    pthread_mutex_unlock(&_Pool->mutex);
    free(Task);
    return -101;
  }

  /* Signal for a thread worker to run task and unlock mutex */
  pthread_cond_signal(&_Pool->task_added);
  pthread_mutex_unlock(&_Pool->mutex);
  
  return 0;
}

void tp_wait(Thread_Pool* _Pool)
{
  if (!_Pool) 
    return;

  pthread_mutex_lock(&_Pool->mutex);
  while (_Pool->active_tasks > 0 || _Pool->queue->count > 0)
    pthread_cond_wait(&_Pool->task_finished, &_Pool->mutex);

  pthread_mutex_unlock(&_Pool->mutex);
}

void tp_dispose(Thread_Pool* _Pool)
{
  if (!_Pool || _Pool->stop)
    return;

  /* Stop running threads gracefully */
  pthread_mutex_lock(&_Pool->mutex);
  _Pool->stop = true;
  pthread_cond_broadcast(&_Pool->task_added);
  pthread_mutex_unlock(&_Pool->mutex);

  /* Join and free threads */
  for (int i = 0; i < _Pool->max_threads; i++)
    pthread_join(_Pool->threads[i], NULL);
  free(_Pool->threads);

  /* Dispose of queue LL */
  pthread_mutex_lock(&_Pool->mutex);
  linked_list_foreach(_Pool->queue, node) {
    if (node->item != NULL)
      free(node->item); // free tasks if any still alloc'd
  }
  linked_list_destroy(&_Pool->queue);

  /* Unlock and destroy primitives */
  pthread_mutex_unlock(&_Pool->mutex);
  pthread_mutex_destroy(&_Pool->mutex);
  pthread_cond_destroy(&_Pool->task_added);
  pthread_cond_destroy(&_Pool->task_finished);

  free(_Pool);
}
