
#include <stdio.h>
#include <time.h>

#include "types.h"
#include "util.h"


void os_init (void)
{
  char *current_time;
  
  setlinebuf (stdout);
  current_time = clock_get_time ();
  printf ("\nStart time %s\n\n", current_time);
}

