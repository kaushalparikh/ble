
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "serial.h"
#include "ble.h"

struct ble_message_list_entry
{
  struct ble_message_list_entry *next;
  ble_message_t                  message;
};

typedef struct ble_message_list_entry ble_message_list_entry_t;

LIST_HEAD_INIT (ble_message_list_entry_t, ble_message_list);


int ble_init (void)
{
  int status = 0;
  int init_attempt = 0;
  
  /* Find BLE device and initialize */
  status = serial_init ();
  sleep (1);

  if (status == 0)
  {
    printf ("BLE Reset request\n");

      /* Reset BLE */
    (void)ble_reset ();
    sleep (1);

    /* Close & re-open UART after reset */
    serial_deinit ();

    do
    {
      init_attempt++;
      sleep (1);
      
      status = serial_init ();
    } while ((status < 0) && (init_attempt < 2));

    if (status == 0)
    {
      /* Ping BLE */
      status = ble_hello ();
    }
    else
    {
      printf ("BLE Reset request failed\n");
    }

    if (status < 0)
    {
      serial_deinit ();
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

void ble_event_scan_response (ble_event_scan_response_t *scan_response)
{
  int i;
  
  printf ("Found device: ");
  for (i = 0; i < 6; i++)
  {
    printf ("%02x", scan_response->device_address.byte[i]);
  }
  printf ("\n");
  printf ("  RSSI %d, packet type %u, address type %u, bond %u\n",
                    scan_response->rssi, scan_response->packet_type,
                    scan_response->address_type, scan_response->bond);
  printf ("  Data (%u) ", scan_response->length);
  for (i = 0; i < scan_response->length; i++)
  {
    printf ("%02x", scan_response->data[i]);
  }
  printf ("\n");

  for (i = 0; i < scan_response->length; )
  {
    printf ("    Adv data type   %02x\n", scan_response->data[i+1]);
    printf ("             length %d\n", scan_response->data[i]);
    i += (1 + scan_response->data[i]);
  }
}

int ble_event (ble_message_t *message)
{
  switch (message->header.class)
  {
    case BLE_CLASS_GAP:
    {
      switch (message->header.command)
      {
        case BLE_EVENT_SCAN_RESPONSE:
        {
          ble_event_scan_response ((ble_event_scan_response_t *)(message->data));
          break;
        }

        default:
        {
          printf ("Event not handled\n");
          printf ("  type %d, length %d, class %d, command %d", message->header.type,
                                                                message->header.length,
                                                                message->header.class,
                                                                message->header.command);
          break;
        }
      }
      
      break;
    }

    default:
    {
      printf ("Class not handled\n");
      printf ("  type %d, length %d, class %d, command %d", message->header.type,
                                                            message->header.length,
                                                            message->header.class,
                                                            message->header.command);
      break;
    }
  }
    
  return 0;
}

int ble_receive_message (ble_message_t *response)
{
  ble_message_t message;
  int status;

  while ((status = serial_rx (sizeof (message.header), (unsigned char *)(&(message.header)))) > 0)
  {
    if (message.header.length > 0)
    {
      status = serial_rx (message.header.length, message.data);
    }

    if (status > 0)
    {
      if ((response == NULL) ||
          ((response->header.type != message.header.type)        ||
           (response->header.class != message.header.class)      ||
           (response->header.command != message.header.command)))
      {
        ble_message_list_entry_t *message_list_entry;

        /* Store the message */
        message_list_entry = (ble_message_list_entry_t *)(malloc (sizeof (*message_list_entry)));
        message_list_entry->message = message;
        list_add ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
      }
      else
      {
        *response = message;
        break;
      }
    }
  }
      
  if (response == NULL)
  {
    while (ble_message_list != NULL)
    {
      ble_message_list_entry_t *message_list_entry;

      message_list_entry = ble_message_list;
      if (message_list_entry->message.header.type == BLE_EVENT)
      {
        /* Store for message for processing */
        status = ble_event (&(message_list_entry->message));
      }
      else
      {
        printf ("Message not handled\n");
        printf ("  type %d, length %d, class %d, command %d", message_list_entry->message.header.type,
                                                              message_list_entry->message.header.length,
                                                              message_list_entry->message.header.class,
                                                              message_list_entry->message.header.command);
      }

      list_remove ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
      free (message_list_entry);
    }
  }

  return status;
}

int ble_send_message (ble_message_t *message)
{
  int status;

  status = serial_tx (((sizeof (message->header)) + message->header.length),
                      ((unsigned char *)message));

  if (status > 0)
  {
    /* Check for response */
    status = ble_receive_message (message);
  }

  return status;
}

int ble_scan (void)
{
  int status;
  ble_message_t message;
  ble_command_scan_params_t *scan_params;

  printf ("BLE Scan request\n");

  scan_params = (ble_command_scan_params_t *)(&message);
  BLE_CLASS_GAP_HEADER (scan_params, BLE_COMMAND_SET_SCAN_PARAMS);
  scan_params->interval = BLE_SCAN_INTERVAL;
  scan_params->window   = BLE_SCAN_WINDOW;
  scan_params->mode     = BLE_SCAN_ACTIVE;
  status = ble_send_message (&message);

  if (status > 0)
  {
    ble_response_scan_params_t *scan_params_rsp = (ble_response_scan_params_t *)(&message);
    if (scan_params_rsp->result == 0)
    {
      ble_command_discover_t *discover_cmd = (ble_command_discover_t *)(&message);
      BLE_CLASS_GAP_HEADER (discover_cmd, BLE_COMMAND_DISCOVER);
      discover_cmd->mode = BLE_DISCOVER_LIMITED;
      status = ble_send_message (&message);

      if (status > 0)
      {
        ble_response_discover_t *discover_rsp = (ble_response_discover_t *)(&message);
        if (discover_rsp->result > 0)
        {
          printf ("BLE Scan response received with failure %d\n", discover_rsp->result);
          status = -1;
        }
      }
      else
      {
        printf ("BLE Scan response failed with %d\n", status);
        status = -1;
      }
    }
    else
    {
      printf ("BLE Scan params response received with failure %d\n", scan_params_rsp->result);
    }
  }
  else
  {
    printf ("BLE Scan params response failed with %d\n", status);
    status = -1;
  }

  return status;
}

int ble_hello (void)
{
  int status;
  ble_message_t message;
  ble_command_hello_t *hello;

  printf ("BLE Hello request\n");
  
  hello = (ble_command_hello_t *)(&message);
  BLE_CLASS_SYSTEM_HEADER (hello, BLE_COMMAND_HELLO);
  status = ble_send_message (&message);  
  if (status <= 0)
  {
    printf ("BLE Hello response failed with %d\n", status);
    status = -1;
  }

  return status;
}

int ble_reset (void)
{
  int status;
  ble_message_t message;
  ble_command_reset_t *reset;

  reset = (ble_command_reset_t *)(&message);
  BLE_CLASS_SYSTEM_HEADER (reset, BLE_COMMAND_RESET);
  reset->mode = BLE_RESET_NORMAL;
  status = serial_tx (((sizeof (reset->header)) + reset->header.length),
                      (unsigned char *)reset);
  if (status <= 0)
  {
    printf ("BLE Reset failed with %d\n", status);
    status = -1;
  }
  
  return status;
}

