/* From root: 
 * gcc -Imodules/include modules/tests/test_thread_pool.c modules/src/thread_pool.c modules/src/linked_list.c -o tp_test */

#include "maestromodules/thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_THREADS 16

void thread_func(void* _arg)
{
  int randnum = rand() % 1337;
  printf("num: %d\n", randnum);
  usleep(50000);
}

int main()
{
  Thread_Pool* Pool = tp_init(MAX_THREADS);
  if (!Pool)
  {
    perror("tp_init");
    return 1;
  }

  srand(time(NULL));

  for (int i = 0; i < 100; i++)
  {
    TP_Task Task = { .thread_func = thread_func, NULL, NULL, NULL };
    tp_task_add(Pool, &Task);
  }

  tp_dispose(Pool);

  return 0;
}
