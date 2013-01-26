#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "serial.h"
#include "ble.h"


void print_help (void)
{
  printf("Usage: ble-master [OPTION...]\n\n");
  printf("\t-d <device> Specify BLE <device>\n");
  printf("\t-h          Print usage\n");
}

int parse_options (int argc, char * argv[])
{
  int status = 0;
  int option;

  while ((option = getopt (argc, argv, "hd:")) != -1)
  {
    switch (option)
    {
      case 'd':
      {
        uart_device (optarg);
        break;
      }

      case 'h':
      {
        print_help ();
        status = -1;
        break;
      }

      default:
      {
        printf ("Invalid option %c\n", option);
        status = -1;
        break;
      }
    }
  }

  return status;
}

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

  /* Default serial device, if command line option
     is provided, it will be overridden */
  uart_device ("/dev/ttyACM0");

  status = parse_options (argc, argv);
  if (status == 0)
  {
    status = uart_open ();
    if (status > 0)
    {
      /* Reset BLE */
      status = ble_reset ();

      if (status == 0)
      {
        master_loop ();
      }

      uart_close ();
    }
  }

  return 0;
}

