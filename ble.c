
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

struct ble_list_entry
{
  struct ble_list_entry *next;
  void                  *data;
};

typedef struct ble_list_entry ble_list_entry_t;

LIST_HEAD_INIT (ble_list_entry_t, ble_update_list);

typedef struct
{
  ble_device_t *device;
  uint8         handle;
} ble_connection_params_t;

static ble_connection_params_t connection_params = 
{
  .device = NULL,
  .handle = 0xff
};


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

static ble_device_t * ble_find_device (ble_device_address_t *address)
{
  ble_device_t *device = NULL;
  ble_device_list_entry_t *device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if ((memcmp (address, &(device_list_entry->info.address), sizeof (*address))) == 0)
    {
      device = &(device_list_entry->info);
      break;
    }
    device_list_entry = device_list_entry->next;
  }

  return device;
}

static void ble_print_device (ble_device_t *device)
{
  int i;

  printf ("     Name: %s\n", device->name);
  printf ("  Address: ");
  for (i = 0; i < BLE_DEVICE_ADDRESS_LENGTH; i++)
  {
    printf ("%02x", device->address.byte[i]);
  }
  printf (" (%s)\n", ((device->address.type > 0) ? "random" : "public"));
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

static int ble_connect_direct (ble_device_t *device)
{
  int status;
  ble_message_t message;
  ble_command_connect_direct_t *connect_direct;

  printf ("BLE Connect direct request\n");
  ble_print_device (device);

  connect_direct = (ble_command_connect_direct_t *)(&message);
  BLE_CLASS_GAP_HEADER (connect_direct, BLE_COMMAND_CONNECT_DIRECT);
  connect_direct->address      = device->address;
  connect_direct->min_interval = BLE_MIN_CONNECT_INTERVAL;
  connect_direct->max_interval = BLE_MAX_CONNECT_INTERVAL;
  connect_direct->timeout      = BLE_CONNECT_TIMEOUT;
  connect_direct->latency      = BLE_CONNECT_LATENCY;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_connect_direct_t *connect_direct_rsp = (ble_response_connect_direct_t *)(&message);
    if (connect_direct_rsp->result == 0)
    {
      connection_params.device = device;
      connection_params.handle = connect_direct_rsp->handle;
    }
    else
    {
      printf ("BLE Connect direct response received with failure %d\n", connect_direct_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Connect direct failed with %d\n", status);
  }

  return status;
}

static int ble_connect_disconnect (void)
{
  int status;
  ble_message_t message;
  ble_command_disconnect_t *disconnect;

  printf ("BLE Disconnect request\n");

  disconnect = (ble_command_disconnect_t *)(&message);
  BLE_CLASS_CONNECTION_HEADER (disconnect, BLE_COMMAND_DISCONNECT);
  disconnect->handle = connection_params.handle;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_disconnect_t *disconnect_rsp = (ble_response_disconnect_t *)(&message);
    if (disconnect_rsp->result == 0)
    {
      connection_params.device = NULL;
      connection_params.handle = 0xff;
    }
    else
    {
      printf ("BLE Disconnect response received with failure %d\n", disconnect_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Disconnect failed with %d\n", status);
  }

  return status;
}

static int ble_update_profile (void)
{
  int status;
  ble_message_t message;
  ble_command_read_group_t *read_group;

  read_group = (ble_command_read_group_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (read_group, BLE_COMMAND_READ_BY_GROUP_TYPE);
  read_group->handle       = connection_params.handle;
  read_group->start_handle = 0x0001;
  read_group->end_handle   = 0xffff;
  read_group->length       = 2;
  read_group->data[0]      = 0x00;
  read_group->data[1]      = 0x28;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_read_group_t *read_group_rsp = (ble_response_read_group_t *)(&message);
    if (read_group_rsp->result != 0)
    {
      printf ("BLE Read group response received with failure %d\n", read_group_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Read group failed with %d\n", status);
  }
  
  return status;
}

int ble_set_timer (int millisec, int cause)
{
  int status = 1;
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

void ble_print_message (ble_message_t *message)
{
 printf ("Message not handled\n");
 printf (" type 0x%x, length %d, class 0x%x, command 0x%x\n", message->header.type,
                                                              message->header.length,
                                                              message->header.class,
                                                              message->header.command);
}

int ble_check_device_status (ble_device_status_e status)
{
  int num_of_devices = 0;
  ble_device_list_entry_t *device_list_entry = ble_device_list;
  ble_list_entry_t *list_entry = ble_update_list;

  while (ble_update_list != NULL)
  {
    list_remove ((list_entry_t **)(&ble_update_list), (list_entry_t *)list_entry);
    free (list_entry);
  }

  while (device_list_entry != NULL)
  {
    if (device_list_entry->info.status == status)
    {
      ble_list_entry_t *list_entry = (ble_list_entry_t *)malloc (sizeof (*list_entry));
      list_entry->data = &(device_list_entry->info);
      list_add ((list_entry_t **)(&ble_update_list), (list_entry_t *)list_entry);
      num_of_devices++;
    }
    device_list_entry = device_list_entry->next;
  }

  return num_of_devices;
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
    ble_print_message (&(message_list_entry->message));
    free (message_list_entry);
  }  
}

int ble_start_scan (void)
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

void ble_event_scan_response (ble_event_scan_response_t *scan_response)
{
  int i;
  ble_device_t *device;

  device = ble_find_device (&(scan_response->device_address));
  if (device == NULL)
  {
    ble_device_list_entry_t *device_list_entry 
      = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));

    device = &(device_list_entry->info);
    list_add ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);

    device->address      = scan_response->device_address;
    device->name         = NULL;
    device->status       = BLE_DEVICE_UPDATE_PROFILE;
    
    printf ("New device --\n");
  }
  else if (device->status == BLE_DEVICE_LOST)
  {
    printf ("Found device --\n");
    device->status = BLE_DEVICE_UPDATE_DATA;
  }
  else
  {
    printf ("Listed device --\n");
  }
  
  for (i = 0; i < scan_response->length; i += (scan_response->data[i] + 1))
  {
    ble_adv_data_t *adv_data = (ble_adv_data_t *)&(scan_response->data[i]);

    if (adv_data->type == BLE_ADV_LOCAL_NAME)
    {
      if (device->name != NULL)
      {
        free (device->name);
      }
      device->name = (char *)malloc (adv_data->length);
      device->name[adv_data->length-1] = '\0';
      strncpy (device->name, (char *)(adv_data->value), (adv_data->length-1));
    }
    else if ((adv_data->type == BLE_ADV_LOCAL_NAME_SHORT) &&
             (device->name != NULL))
    {
      device->name = (char *)malloc (adv_data->length);
      device->name[adv_data->length-1] = '\0';
      strncpy (device->name, (char *)(adv_data->value), (adv_data->length-1));
    }
  }

  ble_print_device (device);
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

int ble_start_profile (void)
{
  int status = 1;
  ble_device_t *device;

  device = (ble_device_t *)(ble_update_list->data);
  if (device != NULL)
  {
    status = ble_connect_direct (device);
  }
  
  return status;
}

int ble_event_connection_status (ble_event_connection_status_t *connection_status)
{
  int status = 1;

  printf ("BLE Connect status flags %02x, interval %d, timeout %d, latency %d, bonding %02x\n",
             connection_status->flags, connection_status->interval, connection_status->timeout,
             connection_status->latency, connection_status->bonding);

  if (connection_status->flags & BLE_CONNECT_ESTABLISHED)
  {
    if (connection_params.device->status == BLE_DEVICE_UPDATE_PROFILE)
    {
      status = ble_update_profile ();

      if (status <= 0)
      {
        status = ble_connect_disconnect ();
      }
    }
    else
    {
      /* status = ble_update_data (&(connection_params.device)); */
    }
  }

  if (status <= 0)
  {
    /* Go to next device */
    ble_list_entry_t *list_entry = ble_update_list;
    if (list_entry->next != NULL)
    {
      ble_device_t *device;
      
      list_remove ((list_entry_t **)(&ble_update_list), (list_entry_t *)list_entry);
      free (list_entry);
      device = (ble_device_t *)(ble_update_list->data);

      status = ble_connect_direct (device);
    }
    else
    {
      status = 0;
    }
  }

  return status;
}

int ble_event_disconnect (ble_event_disconnect_t *disconnect)
{
  int status = 1;
  
  printf ("BLE Disconnect reason %d\n", disconnect->cause);

  /* Go to next device */
  ble_list_entry_t *list_entry = ble_update_list;
  if (list_entry->next != NULL)
  {
    ble_device_t *device;
    
    list_remove ((list_entry_t **)(&ble_update_list), (list_entry_t *)list_entry);
    free (list_entry);
    device = (ble_device_t *)(ble_update_list->data);
 
    status = ble_connect_direct (device);
  }
  else
  {
    status = 0;
  }

  return status;
}

void ble_event_group_found (ble_event_group_found_t *group_found)
{
  printf ("BLE Found group start %04x, end %04x, uuid(%d) %02x%02x\n",
            group_found->start_handle, group_found->end_handle, group_found->length,
            group_found->data[1], group_found->data[0]);
}

void ble_event_procedure_completed (ble_event_procedure_completed_t *procedure_completed)
{
  printf ("BLE Procedure completed, char handle %04x\n", procedure_completed->char_handle);
  (void)ble_connect_disconnect ();
}

