
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "types.h"
#include "util.h"


static void timer_event (int event_id, siginfo_t *event_info, void *unused)
{
  timer_info_t *timer_info = event_info->si_value.sival_ptr;
  timer_info->callback (timer_info);
  timer_delete ((timer_t)(timer_info->handle));
  free (timer_info);
}

int32 timer_start (int32 millisec, int32 event,
                   void (*callback)(void *), timer_info_t **timer_info)
{
  struct sigaction timer_action;
  struct sigevent signal_event;
  timer_t timer_id;
  int status;

  *timer_info = (timer_info_t *)malloc (sizeof (**timer_info));

  /* Fill timer info */
  (*timer_info)->millisec = millisec;
  (*timer_info)->event    = event;
  (*timer_info)->callback = callback;

  /* Establish handler for timer signal */
  timer_action.sa_flags     = SA_SIGINFO;
  timer_action.sa_sigaction = timer_event;
  sigemptyset (&timer_action.sa_mask);
  status = sigaction (SIGRTMIN, &timer_action, NULL);
  if (status == 0)
  {
    /* Create the timer */
    signal_event.sigev_notify = SIGEV_SIGNAL;
    signal_event.sigev_signo  = SIGRTMIN;
    signal_event.sigev_value.sival_ptr = *timer_info;
    status = timer_create (CLOCK_MONOTONIC, &signal_event, &timer_id);
    if (status == 0)
    {
      struct itimerspec timer_spec;

      (*timer_info)->handle = (void *)timer_id;
      timer_spec.it_value.tv_sec  = millisec / 1000;
      timer_spec.it_value.tv_nsec = (millisec % 1000) * 1000000;
      /* One-shot timer */
      timer_spec.it_interval.tv_sec  = 0;
      timer_spec.it_interval.tv_nsec = 0;
  
      /* Start timer */
      status = timer_settime (timer_id, 0, &timer_spec, NULL);
      if (status != 0)
      {
        timer_delete (timer_id);
        printf ("Unable to start timer\n");
      }
    }
    else
    {
      printf ("Unable to create timer\n");
    }
  }
  else
  {
    printf ("Unable to set timer expiry handler\n");
  }

  if (status != 0)
  {
    free (*timer_info);
  }

  return status;
}

int32 timer_status (timer_info_t *timer_info)
{
  int current_count = -1;

  struct itimerspec timer_spec;
  
  if ((timer_gettime ((timer_t)(timer_info->handle), &timer_spec)) == 0)
  {
    current_count = timer_info->millisec
                     - ((timer_spec.it_value.tv_sec * 1000)
                         + (timer_spec.it_value.tv_nsec / 1000000));
  }

  return current_count;
}

int32 timer_stop (timer_info_t *timer_info)
{
  timer_t timer_id = (timer_t)(timer_info->handle);
  free (timer_info);
  return timer_delete (timer_id);
}

int32 clock_get_count (void)
{
  int millisec = -1;
  struct timespec current_time;

  if ((clock_gettime (CLOCK_MONOTONIC, &current_time)) == 0)
  {
    millisec = (current_time.tv_sec * 1000)
               + (current_time.tv_nsec / 1000000);
  }

  return millisec;
}

int8 * clock_get_time (void)
{
  char current_time[50];
  time_t utc;
  struct tm *utc_tm;
  
  utc = time (NULL);
  utc_tm = localtime (&utc);
  strftime(current_time, 50, "%F %T", utc_tm);

  return strdup (current_time);
}

#ifdef UTIL_TIMER_TEST

void callback (void *timer_info)
{
  printf ("Timer %d expired\n", ((timer_info_t *)timer_info)->handle);
}

int main (void)
{
  timer_info_t *timer_info = NULL;
  int status;

  status = timer_start (3000, 0, callback, &timer_info);
  sleep (4);
  status = timer_start (3000, 0, callback, &timer_info);
  sleep (2);
  status = timer_stop (timer_info);

  return 0;
}

#endif

