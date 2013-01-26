
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "serial.h"
#include "cmd_def.h"

static void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2);


int ble_reset (void)
{
  int status = 0;
  int retry;
  
    /* Reset BLE */
  ble_cmd_system_reset (0);

  /* Sleep for 0.5s (500000 us) */
  printf ("Resetting BLE ");
  usleep (500000);
  printf (".");

  /* Close & re-open UART after reset */
  uart_close ();

  retry = 10;
  do
  {
    /* Sleep for 0.5s (500000 us) */
    usleep(500000);
    printf(".");
    retry--;
  } while (((uart_open ()) < 0) && (retry > 0));
  printf(" done\n");

  /* Retry greater than zero, implies UART was
     opened successfully */
  if (retry > 0)
  {
    bglib_output = ble_output;
  }
  else
  {
    status = -1;
  }

  return status;
}

void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2)
{
  (void)uart_tx (bytes1, buffer1);
  (void)uart_tx (bytes2, buffer2);
}

int ble_read_message (void)
{
  unsigned char buffer[256];
  struct ble_header header;
  int status;

  status = uart_rx (sizeof(header), (unsigned char *)&header);
  if (status > 0)
  {
    if (header.lolen > 0)
    {
      status = uart_rx(header.lolen, buffer);
      if (status > 0)
      {
        struct ble_msg * message = (struct ble_msg *)ble_get_msg_hdr (header);
        if (message != NULL)
        {
          message->handler (buffer);
        }
        else
        {
          status = -1;
        }
      }
    }
  }

  return status;
}

int ble_scan (void)
{
  int status;
  struct ble_header scan_rsp_header;

  ble_cmd_gap_discover (gap_discover_observation);

  scan_rsp_header.type_hilen = (uint8)ble_dev_type_ble|(uint8)ble_msg_type_rsp|0x0;
  scan_rsp_header.lolen      = 2;
  scan_rsp_header.cls        = ble_cls_gap;
  scan_rsp_header.command    = ble_cmd_gap_discover_id;
  
  status = ble_read_message ();

  return status;
}

