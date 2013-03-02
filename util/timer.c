
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "util.h"


static void timer_event (int event_id, siginfo_t *event_info, void *unused)
{
  timer_info_t *timer_info = event_info->si_value.sival_ptr;
  timer_info->callback (timer_info->data);
  timer_delete ((timer_t)(timer_info->id));
}

int timer_start (timer_info_t *timer_info)
{
  struct sigaction timer_action;
  struct sigevent signal_event;
  timer_t timer_id;
  int status;

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
    signal_event.sigev_value.sival_ptr = timer_info;
    status = timer_create (CLOCK_MONOTONIC, &signal_event, &timer_id);
    if (status == 0)
    {
      struct itimerspec timer_spec;

      timer_info->id = (int)timer_id;
      timer_spec.it_value.tv_sec  = timer_info->millisec / 1000;
      timer_spec.it_value.tv_nsec = (timer_info->millisec % 1000) * 1000000;
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

  return status;
}

int timer_stop (timer_info_t *timer_info)
{
  return timer_delete ((timer_t)(timer_info->id));
}

#ifdef UTIL_TIMER_TEST

void callback (timer_info_t *timer_info)
{
  printf ("Timer %d expired\n", timer_info->id);
}

int main (void)
{
  timer_info_t timer_info;
  int status;

  timer_info.millisec = 3000;
  timer_info.callback = (void (*)(void *))callback;
  timer_info.data     = &timer_info;

  status = timer_start (&timer_info);
  sleep (4);
  status = timer_start (&timer_info);
  sleep (2);
  status = timer_stop (&timer_info);

  return 0;
}

#endif
