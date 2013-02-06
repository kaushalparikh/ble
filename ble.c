
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "serial.h"
#include "cmd_def.h"
#include "ble.h"

static void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2);


int ble_init (void)
{
  int status = 0;
  int init_attempt = 0;
  
  /* Find BLE device and initialize */
  status = uart_init ("2458", "0001");
  sleep (1);

  if (status == 0)
  {
    /* BLE TX function pointer */
    bglib_output = ble_output;

    printf ("BLE Reset request\n");

      /* Reset BLE */
    ble_cmd_system_reset (0);
    sleep (1);

    /* Close & re-open UART after reset */
    uart_close ();

    do
    {
      init_attempt++;
      sleep (1);
      
      status = uart_open ();
    } while ((status <= 0) && (init_attempt < 2));

    if (status > 0)
    {
      /* Ping BLE */
      status = ble_hello ();
    }
    else
    {
      printf ("BLE Reset request failed\n");
      bglib_output = NULL;
    }

    if (status <= 0)
    {
      uart_deinit ();
    }
  }
  else
  {
    printf ("Can't find BLE device\n");
  }

  return status;
}

void ble_deinit (void)
{
}

void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2)
{
  (void)uart_tx (bytes1, buffer1);
  (void)uart_tx (bytes2, buffer2);
}

int ble_read_message (struct ble_header *rsp_header, unsigned char *rsp_buffer)
{
  struct ble_header header;
  unsigned char buffer[256];
  int status;

  status = uart_rx (sizeof(header), (unsigned char *)&header);
  if (status > 0)
  {
    if (header.lolen > 0)
    {
      status = uart_rx (header.lolen, buffer);
    }

    if (rsp_header != NULL)
    {
      /* Response provided, match it and if successful
         copy the response */
      if ((rsp_header->type_hilen == header.type_hilen) &&
          (rsp_header->lolen == header.lolen)           &&
          (rsp_header->cls == header.cls)               &&
          (rsp_header->command == header.command))
      {
        if (rsp_header->lolen > 0)
        {
          memcpy (rsp_buffer, buffer, rsp_header->lolen);
        }
      }
      else
      {
        status = -1;
      }
    }
    else if (status > 0)
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

  return status;
}

int ble_scan (void)
{
  int status;
  struct ble_header rsp_header;
  short int result;

  printf ("BLE Scan request\n");

  ble_cmd_gap_discover (gap_discover_observation);

  rsp_header.type_hilen = (uint8)ble_dev_type_ble|(uint8)ble_msg_type_rsp|0x0;
  rsp_header.lolen      = 2;
  rsp_header.cls        = ble_cls_gap;
  rsp_header.command    = ble_cmd_gap_discover_id;
  
  status = ble_read_message (&rsp_header, (unsigned char *)&result);
  if (status > 0)
  {
    if (result > 0)
    {
      printf ("BLE Scan response received with failure %d\n", result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Hello response failed with %d\n", status);
  }

  return status;
}

int ble_hello (void)
{
  int status;
  struct ble_header rsp_header;

  printf ("BLE Hello request\n");

  ble_cmd_system_hello ();

  rsp_header.type_hilen = (uint8)ble_dev_type_ble|(uint8)ble_msg_type_rsp|0x0;
  rsp_header.lolen      = 0;
  rsp_header.cls        = ble_cls_system;
  rsp_header.command    = ble_cmd_system_hello_id;
  
  status = ble_read_message (&rsp_header, NULL);
  if (status <= 0)
  {
    printf ("BLE Hello response failed with %d\n", status);
  }

  return status;
}

