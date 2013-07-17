
#include <stdio.h>
#include <pthread.h>

#include "types.h"
#include "util.h"


void os_init (void)
{
  char *current_time;
  
  setlinebuf (stdout);
  current_time = clock_get_time ();
  printf ("\nStart time %s\n\n", current_time);
}

int32 os_create_thread (void * (*start_function)(void *), uint8 priority,
                        int32 timeout, void **handle)
{
  int status;
  pthread_t thread;
  pthread_attr_t thread_attr;
  int thread_policy;
  struct sched_param thread_param;

  pthread_attr_init (&thread_attr);
  pthread_attr_getschedparam (&thread_attr, &thread_param);
  pthread_attr_getschedpolicy (&thread_attr, &thread_policy);

  if (priority == OS_THREAD_PRIORITY_MIN)
  {
    thread_param.sched_priority = 30;
  }
  else if (priority == OS_THREAD_PRIORITY_MAX)
  {
    thread_param.sched_priority = 70;
  }
  else
  {
    thread_param.sched_priority = 50;
  }

  thread_policy = SCHED_RR;

  pthread_attr_setschedparam (&thread_attr, &thread_param);
  pthread_attr_setschedpolicy (&thread_attr, thread_policy);

  if ((pthread_create (&thread, &thread_attr, start_function, (void *)timeout)) == 0)
  {
    *handle = (void *)thread;
    status = 1;
  }
  else
  {
    printf ("Unable to create thread with priority %d, timeout %d\n", priority, timeout);
    status = -1;
  }

  pthread_attr_destroy (&thread_attr);

  return status;
}

int32 os_destroy_thread (void *handle)
{
  int status = 1;

  if ((pthread_join ((pthread_t)handle, NULL)) != 0)
  {
    status = -1;
  }

  return status;
}

