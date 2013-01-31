#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "serial.h"
#include "ble.h"


void master_loop (void)
{
  if ((ble_scan ()) >= 0)
  {
    while ((ble_read_message ()) >= 0)
    {
    }
  }
  else
  {
    printf ("BLE scan request failed\n");
  }
}

int main (int argc, char * argv[])
{
  int status;

  status = uart_init ("2458", "0001");

  if (status == 0)
  {
    /* Reset BLE */
    status = ble_reset ();

    if (status == 0)
    {
      master_loop ();
    }
  }

  uart_deinit ();

  return 0;
}

