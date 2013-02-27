
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "util.h"
#include "serial.h"
#include "ble.h"

struct timer_list_entry
{
  struct timer_list_entry *next;
  timer_info_t             info;
  int                      cause;
};

typedef struct timer_list_entry timer_list_entry_t;

LIST_HEAD_INIT (timer_list_entry_t, timer_expiry_list);

struct ble_message_list_entry
{
  struct ble_message_list_entry *next;
  ble_message_t                  message;
};

typedef struct ble_message_list_entry ble_message_list_entry_t;

LIST_HEAD_INIT (ble_message_list_entry_t, ble_message_list);

struct ble_device_list_entry
{
  struct ble_device_list_entry *next;
  ble_device_t                  info;
};

typedef struct ble_device_list_entry ble_device_list_entry_t;

LIST_HEAD_INIT (ble_device_list_entry_t, ble_device_list);


static void ble_timer_callback (timer_list_entry_t *timer_list_entry)
{
  list_add ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
}
  
static int ble_response (ble_message_t *response)
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
      if ((response->header.type != message.header.type)        ||
          (response->header.class != message.header.class)      ||
          (response->header.command != message.header.command))
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

  return status;
}

static int ble_command (ble_message_t *message)
{
  int status;

  status = serial_tx (((sizeof (message->header)) + message->header.length),
                      ((unsigned char *)message));

  if (status > 0)
  {
    /* Check for response */
    status = ble_response (message);
  }

  return status;
}

static ble_device_t * ble_device_find (ble_device_address_t *address, uint8 address_type)
{
  ble_device_t *device = NULL;
  ble_device_list_entry_t *device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if ((device_list_entry->info.address_type == address_type) &&
        ((memcmp (address, &(device_list_entry->info.address), BLE_DEVICE_ADDRESS_LENGTH)) == 0))
    {
      device = &(device_list_entry->info);
      break;
    }
    device_list_entry = device_list_entry->next;
  }

  return device;
}

static int ble_reset (void)
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

static int ble_hello (void)
{
  int status;
  ble_message_t message;
  ble_command_hello_t *hello;

  printf ("BLE Hello request\n");
  
  hello = (ble_command_hello_t *)(&message);
  BLE_CLASS_SYSTEM_HEADER (hello, BLE_COMMAND_HELLO);
  status = ble_command (&message);  
  if (status <= 0)
  {
    printf ("BLE Hello response failed with %d\n", status);
    status = -1;
  }

  return status;
}

static void ble_event_scan_response (ble_event_scan_response_t *scan_response)
{
  int i;
  int new_device = 0;
  ble_device_t *device;

  device = ble_device_find (&(scan_response->device_address), scan_response->address_type);
  if (device == NULL)
  {
    ble_device_list_entry_t *device_list_entry 
      = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));

    device = &(device_list_entry->info);
    list_add ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);

    device->address      = scan_response->device_address;
    device->address_type = scan_response->address_type;
    device->name         = NULL;
    device->tx_power     = -127;
    new_device           = 1;
  }
  
  for (i = 0; i < scan_response->length; )
  {
    if (scan_response->data[i+1] == BLE_ADV_TX_POWER)
    {
      device->tx_power = (int8)(scan_response->data[i+2]);
    }
    else if (scan_response->data[i+1] == BLE_ADV_LOCAL_NAME)
    {
      if (device->name != NULL)
      {
        free (device->name);
      }
      device->name = (char *)malloc (scan_response->data[i]);
      device->name[scan_response->data[i]] = '\0';
      strncpy (device->name, (char *)&(scan_response->data[i+2]), (scan_response->data[i] - 1));
    }
    else if ((scan_response->data[i+1] == BLE_ADV_LOCAL_NAME_SHORT) &&
             (device->name != NULL))
    {
      device->name = (char *)malloc (scan_response->data[i]);
      device->name[scan_response->data[i]] = '\0';
      strncpy (device->name, (char *)&(scan_response->data[i+2]), (scan_response->data[i] - 1));
    }
      
    i += (1 + scan_response->data[i]);
  }

  if (new_device)
  {
    printf ("Found device: %s\n", device->name);
    printf ("     Address: ");
    for (i = 0; i < BLE_DEVICE_ADDRESS_LENGTH; i++)
    {
      printf ("%02x", scan_response->device_address.byte[i]);
    }
    printf (" (%s)\n", ((device->address_type > 0) ? "random" : "public"));
    printf ("    Tx power: %d dBm\n", device->tx_power);
  }
}

static int ble_event (ble_message_t *message)
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
          printf ("  type %d, length %d, class %d, command %d\n", message->header.type,
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
      printf ("  type %d, length %d, class %d, command %d\n", message->header.type,
                                                              message->header.length,
                                                              message->header.class,
                                                              message->header.command);
      break;
    }
  }
    
  return 0;
}

int ble_set_timer (int millisec, int cause)
{
  int status = 0;
  timer_list_entry_t *timer_list_entry;

  timer_list_entry = (timer_list_entry_t *)malloc (sizeof (*timer_list_entry));
  timer_list_entry->info.millisec = millisec;
  timer_list_entry->info.callback = (void (*)(void *))ble_timer_callback;
  timer_list_entry->info.data     = timer_list_entry;
  timer_list_entry->cause         = cause;
  if ((timer_start (&(timer_list_entry->info))) != 0)
  {
    free (timer_list_entry);
    status = -1;
  }

  return status;
}

int ble_check_timer (void)
{
  return list_length ((list_entry_t **)(&timer_expiry_list));
}

int ble_receive_timer (ble_message_t *message)
{
  if (timer_expiry_list != NULL)
  {
    timer_list_entry_t *timer_list_entry = timer_expiry_list;

    message->header.type    = BLE_EVENT;
    message->header.length  = 1;
    message->header.class   = BLE_CLASS_HW;
    message->header.command = BLE_EVENT_SOFT_TIMER;
    message->data[0]        = (uint8)(timer_list_entry->cause);
    
    list_remove ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
    free (timer_list_entry);
  }

  return list_length ((list_entry_t **)(&timer_expiry_list));
}

void ble_flush_timer (void)
{
  while (timer_expiry_list != NULL)
  {
    timer_list_entry_t *timer_list_entry = timer_expiry_list;
    list_remove ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
    free (timer_list_entry);
  }  
}

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

int ble_check_serial (void)
{
  int status;
  ble_message_t message;

  while ((status = serial_rx (sizeof (message.header), (unsigned char *)(&(message.header)))) > 0)
  {
    if (message.header.length > 0)
    {
      status = serial_rx (message.header.length, message.data);
    }

    if (status > 0)
    {
      ble_message_list_entry_t *message_list_entry;

      /* Store the message */
      message_list_entry = (ble_message_list_entry_t *)(malloc (sizeof (*message_list_entry)));
      message_list_entry->message = message;
      list_add ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    }
  }

  return list_length ((list_entry_t **)(&ble_message_list));
}

int ble_receive_serial (ble_message_t *message)
{
  if (ble_message_list != NULL)
  {
    ble_message_list_entry_t *message_list_entry;

    message_list_entry = ble_message_list;
    *message = message_list_entry->message;
    list_remove ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    free (message_list_entry);
  }

  return list_length ((list_entry_t **)(&ble_message_list));
}

void ble_flush_serial (void)
{
  while (ble_message_list != NULL)
  {
    ble_message_list_entry_t *message_list_entry = ble_message_list;
    list_remove ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    free (message_list_entry);
  }  
}

int ble_scan_start (void)
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
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_scan_params_t *scan_params_rsp = (ble_response_scan_params_t *)(&message);
    if (scan_params_rsp->result != 0)
    {
      printf ("BLE Scan params response received with failure %d\n", scan_params_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Scan params response failed with %d\n", status);
  }

  if (status > 0)
  {
    ble_command_set_filtering_t *set_filtering = (ble_command_set_filtering_t *)(&message);
    BLE_CLASS_GAP_HEADER (set_filtering, BLE_COMMAND_SET_FILTERING);
    set_filtering->scan_policy    = BLE_SCAN_POLICY_ALL;
    set_filtering->scan_duplicate = BLE_SCAN_DUPLICATE_FILTER;
    set_filtering->adv_policy     = BLE_ADV_POLICY_ALL;
    status = ble_command (&message);

    if (status > 0)
    {
      ble_response_set_filtering_t *set_filtering_rsp = (ble_response_set_filtering_t *)(&message);
      if (set_filtering_rsp->result != 0)
      {
        printf ("BLE Set filtering response received with failure %d\n", set_filtering_rsp->result);
        status = -1;
      }
    }
    else
    {
      printf ("BLE Set filtering failed with %d\n", status);
    }
  }

  if (status > 0)
  {
    ble_command_discover_t *discover = (ble_command_discover_t *)(&message);
    BLE_CLASS_GAP_HEADER (discover, BLE_COMMAND_DISCOVER);
    discover->mode = BLE_DISCOVER_LIMITED;
    status = ble_command (&message);

    if (status > 0)
    {
      ble_response_discover_t *discover_rsp = (ble_response_discover_t *)(&message);
      if (discover_rsp->result != 0)
      {
        printf ("BLE Scan response received with failure %d\n", discover_rsp->result);
        status = -1;
      }
    }
    else
    {
      printf ("BLE Scan response failed with %d\n", status);
    }
  }

  if (status > 0)
  {
    status = ble_set_timer (BLE_SCAN_DURATION, BLE_TIMER_SCAN_STOP);
  }

  return status;
}

int ble_end_procedure (void)
{
  int status;
  ble_message_t message;
  ble_command_end_procedure_t *end_procedure;

  printf ("BLE End procedure request\n");

  end_procedure = (ble_command_end_procedure_t *)(&message);
  BLE_CLASS_GAP_HEADER (end_procedure, BLE_COMMAND_END_PROCEDURE);
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_end_procedure_t *end_procedure_rsp = (ble_response_end_procedure_t *)(&message);
    if (end_procedure_rsp->result != 0)
    {
      printf ("BLE End procedure response received with failure %d\n", end_procedure_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE End procedure response failed with %d\n", status);
  }

  return status;
}

