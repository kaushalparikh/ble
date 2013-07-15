
#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "sync.h"
#include "device.h"
#include "temperature.h"


void * ble_sync (void *duration)
{
  printf ("Sync thread start with duration %d (sec)\n", (int32)duration);
  sleep ((int32)duration);
  printf ("Sync thread end\n");

  return NULL;
}

