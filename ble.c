
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

LIST_HEAD_INIT (ble_list_entry_t, ble_profile_list);
LIST_HEAD_INIT (ble_list_entry_t, ble_data_list);

typedef struct
{
  ble_list_entry_t         *device;
  ble_service_list_entry_t *service;
  ble_char_list_entry_t    *characteristics;
  ble_attribute_t          *attribute;
  uint8                     handle;
  timer_handle_t            timer;
} ble_connection_params_t;

static ble_connection_params_t connection_params = 
{
  .device          = NULL,
  .service         = NULL,
  .characteristics = NULL,
  .attribute       = NULL,
  .handle          = 0xff,
  .timer           = NULL,
};

static int sleep_interval = 0;
  

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

static void ble_print_service_list (void)
{
  int i;
  ble_service_list_entry_t *service_list = ((ble_device_t *)(connection_params.device->data))->service_list;  

  while (service_list != NULL)
  {
    ble_char_list_entry_t *char_list = service_list->char_list;
      
    printf ("Service type -- %04x\n", ((service_list->declaration.uuid[1] << 8)|service_list->declaration.uuid[0]));
    printf ("  start handle: %04x\n", service_list->start_handle);
    printf ("    end handle: %04x\n", service_list->end_handle);
    printf ("    value     : ");
    for (i = (service_list->declaration.value_length - 1); i >= 0; i--)
    {
      printf ("%02x", service_list->declaration.value[i]);
    }
    printf ("\n");

    while (char_list != NULL)
    {
      ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list->declaration.value);
      uint8 uuid_length = char_list->declaration.value_length - 3;
      
      printf ("      Characteristics handle: %04x\n", char_list->declaration.handle);
      printf ("                       value: %02x, %04x, ", char_decl->bitfield, char_decl->value_handle);
      for (i = (uuid_length - 1); i >= 0; i--)
      {
        printf ("%02x", char_decl->value_uuid[i]);
      }
      printf ("\n");
      if (char_list->description.handle != BLE_INVALID_GATT_HANDLE)
      {
        printf ("          description handle: %04x\n", char_list->description.handle);
        printf ("                       value: ");
        for (i = 0; i < char_list->description.value_length; i++)
        {
          printf ("%c", (int8)(char_list->description.value[i]));
        }
        printf ("\n");
      }
      if (char_list->format.handle != BLE_INVALID_GATT_HANDLE)
      {
        ble_char_format_t *char_format = (ble_char_format_t *)(char_list->format.value);
        
        printf ("               format handle: %04x\n", char_list->format.handle);
        printf ("                       value: %02x, %d\n", char_format->bitfield, char_format->exponent);
      }
      if (char_list->client_config.handle != BLE_INVALID_GATT_HANDLE)
      {
        ble_char_client_config_t *char_client_config = (ble_char_client_config_t *)(char_list->client_config.value);
          
        printf ("        client config handle: %04x\n", char_list->client_config.handle);
        printf ("                       value: %04x\n", char_client_config->bitfield);
      }

      char_list = char_list->next;
    }
    
    service_list = service_list->next;
  }
}

static void ble_update_data_list (void)
{
  ble_device_t *device = (ble_device_t *)(connection_params.device->data);
  ble_service_list_entry_t *service_list = device->service_list;
  ble_list_entry_t *list_entry = NULL;
  
  while (service_list != NULL)
  {
    ble_char_list_entry_t *char_list = service_list->char_list;

    while (char_list != NULL)
    {
      ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list->declaration.value);

      char_list->data.uuid_length = char_list->declaration.value_length - 3;
      char_list->data.handle      = char_decl->value_handle;
      memcpy (char_list->data.uuid, char_decl->value_uuid, char_list->data.uuid_length);

      if ((ble_lookup_uuid (char_list)) > 0)
      {
        if (list_entry == NULL)
        {
          list_entry       = (ble_list_entry_t *)malloc (sizeof (*list_entry));
          list_entry->data = device;
          list_add ((list_entry_t **)(&ble_data_list), (list_entry_t *)list_entry);
        }

        list_add ((list_entry_t **)(&(device->update_list)), (list_entry_t *)char_list);
      }
      char_list = char_list->next;
    }
    service_list = service_list->next;
  }
}

static void ble_data_callback (void)
{
  ble_device_t *device = (ble_device_t *)(connection_params.device->data);
  ble_char_list_entry_t *char_list = device->update_list;

  while (char_list != NULL)
  {
    if (char_list->update.timer == 0)
    {
      char_list->update.callback (char_list);
    }
    
    char_list = char_list->next;
  }
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

static int ble_connect_direct (void)
{
  int status;
  ble_message_t message;
  ble_command_connect_direct_t *connect_direct;
  ble_device_t *device = (ble_device_t *)(connection_params.device->data);

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

  if ((status > 0) &&
      ((connection_params.timer = ble_set_timer (BLE_CONNECT_SETUP_TIMEOUT, BLE_TIMER_CONNECT_STOP)) == NULL))
  {
    status = -1;
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
    if (disconnect_rsp->result != 0)
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

static int ble_read_group (void)
{
  int status;
  ble_message_t message;
  ble_command_read_group_t *read_group;

  printf ("BLE Read group request\n");
  
  read_group = (ble_command_read_group_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (read_group, BLE_COMMAND_READ_BY_GROUP_TYPE);
  read_group->conn_handle  = connection_params.handle;
  read_group->start_handle = BLE_MIN_GATT_HANDLE;
  read_group->end_handle   = BLE_MAX_GATT_HANDLE;
  read_group->length       = BLE_GATT_UUID_LENGTH;
  read_group->data[0]      = (BLE_GATT_PRI_SERVICE & 0xff);
  read_group->data[1]      = ((BLE_GATT_PRI_SERVICE & 0xff00) >> 8);
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

static int ble_find_information (void)
{
  int status;
  ble_message_t message;
  ble_command_find_information_t *find_information;

  printf ("BLE Find information request\n");

  find_information = (ble_command_find_information_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (find_information, BLE_COMMAND_FIND_INFORMATION);
  find_information->conn_handle  = connection_params.handle;
  find_information->start_handle = connection_params.service->start_handle + 1;
  find_information->end_handle   = connection_params.service->end_handle;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_find_information_t *find_information_rsp = (ble_response_find_information_t *)(&message);
    if (find_information_rsp->result != 0)
    {
      printf ("BLE Find information response received with failure %d\n", find_information_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Find information failed with %d\n", status);
  }

  return status;
}

static int ble_read_long_handle (void)
{
  int status;
  ble_message_t message;
  ble_command_read_handle_t *read_handle;

  printf ("BLE Read long request\n");
  
  read_handle = (ble_command_read_handle_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (read_handle, BLE_COMMAND_READ_LONG);
  read_handle->conn_handle = connection_params.handle;
  read_handle->attr_handle = connection_params.attribute->handle;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_read_handle_t *read_handle_rsp = (ble_response_read_handle_t *)(&message);
    if (read_handle_rsp->result != 0)
    {
      printf ("BLE Read long response received with failure %d\n", read_handle_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Read long failed with %d\n", status);
  }

  return status;
}

static int ble_read_handle (void)
{
  int status;
  ble_message_t message;
  ble_command_read_handle_t *read_handle;

  printf ("BLE Read request\n");
  
  read_handle = (ble_command_read_handle_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (read_handle, BLE_COMMAND_READ_BY_HANDLE);
  read_handle->conn_handle = connection_params.handle;
  read_handle->attr_handle = connection_params.attribute->handle;
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_read_handle_t *read_handle_rsp = (ble_response_read_handle_t *)(&message);
    if (read_handle_rsp->result != 0)
    {
      printf ("BLE Read response received with failure %d\n", read_handle_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Read failed with %d\n", status);
  }

  return status;
}

static int ble_write_handle (void)
{
  int status;
  ble_message_t message;
  ble_command_write_handle_t *write_handle;

  printf ("BLE Write request\n");
  
  write_handle = (ble_command_write_handle_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (write_handle, BLE_COMMAND_READ_BY_HANDLE);
  write_handle->conn_handle = connection_params.handle;
  write_handle->attr_handle = connection_params.attribute->handle;
  write_handle->length      = connection_params.attribute->value_length;
  memcpy (write_handle->data, connection_params.attribute->value, write_handle->length);
  status = ble_command (&message);

  if (status > 0)
  {
    ble_response_write_handle_t *write_handle_rsp = (ble_response_write_handle_t *)(&message);
    if (write_handle_rsp->result != 0)
    {
      printf ("BLE Write response received with failure %d\n", write_handle_rsp->result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Write failed with %d\n", status);
  }

  return status;  
}

timer_handle_t ble_set_timer (int millisec, int cause)
{
  timer_list_entry_t *timer_list_entry;

  timer_list_entry = (timer_list_entry_t *)malloc (sizeof (*timer_list_entry));
  timer_list_entry->info.millisec = millisec;
  timer_list_entry->info.callback = (void (*)(void *))ble_timer_callback;
  timer_list_entry->info.data     = timer_list_entry;
  timer_list_entry->cause         = cause;
  if ((timer_start (&(timer_list_entry->info))) != 0)
  {
    free (timer_list_entry);
    timer_list_entry = NULL;
  }

  return (timer_handle_t)timer_list_entry;
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

int ble_check_scan_list (void)
{
  int found = 0;
  ble_list_entry_t *list_entry = ble_data_list;

  while (list_entry != NULL)
  {
    if (((ble_device_t *)(list_entry->data))->status == BLE_DEVICE_DISCOVER)
    {
      found++;
    }
    list_entry = list_entry->next;
  }
  
  return found;  
}

int ble_check_profile_list (void)
{
  int found = 0;
  ble_list_entry_t *list_entry = ble_profile_list;

  while (list_entry != NULL)
  {
    if (((ble_device_t *)(list_entry->data))->status == BLE_DEVICE_DISCOVER_SERVICE)
    {
      found++;
    }
    list_entry = list_entry->next;
  }
  
  return found;  
}

int ble_check_data_list (void)
{
  int found = 0;
  ble_list_entry_t *list_entry = ble_data_list;

  while (list_entry != NULL)
  {
    if (((ble_device_t *)(list_entry->data))->status == BLE_DEVICE_UPDATE_DATA)
    {
      found++;
    }
    list_entry = list_entry->next;
  }
  
  return found;
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

  printf ("BLE Start scan request\n");

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
    discover->mode = BLE_DISCOVER_GENERIC;
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

  if ((status > 0) &&
      ((ble_set_timer (BLE_SCAN_DURATION, BLE_TIMER_SCAN_STOP)) == NULL))
  {
    status = -1;
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

int ble_start_profile (void)
{
  int status;

  connection_params.device = ble_profile_list;
  status = ble_connect_direct ();
  
  return status;
}

int ble_next_profile (void)
{
  int status;
  ble_list_entry_t *list_entry = connection_params.device;
  ble_device_t *device = (ble_device_t *)(list_entry->data);
  
  /* Go to next device */
  connection_params.device = connection_params.device->next;
  if (connection_params.device != NULL)
  {
    status = ble_connect_direct ();
  }
  else
  {
    status = 0;
  }

  if (device->status != BLE_DEVICE_DISCOVER_SERVICE)
  {
    list_remove ((list_entry_t **)(&ble_profile_list), (list_entry_t *)list_entry);
    free (list_entry);

    if (device->status == BLE_DEVICE_DISCOVER)
    {
      ble_device_list_entry_t *device_list_entry = ble_device_list;
      while (device_list_entry != NULL)
      {
        if (device == (&(device_list_entry->info)))
        {
          list_remove ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);
          free (device_list_entry);
          break;
        }
        device_list_entry = device_list_entry->next;
      }
    }
  }

  return status;
}

int ble_read_profile (void)
{
  int status;
  ble_device_t *device = (ble_device_t *)(connection_params.device->data);

  if (device->status == BLE_DEVICE_DISCOVER_SERVICE)
  {
    status = ble_read_group ();
    if (status > 0)
    {
      device->status = BLE_DEVICE_DISCOVER_CHAR_DESC;
    }
    else
    {
      status = ble_connect_disconnect ();
    }
  }
  else if (device->status == BLE_DEVICE_DISCOVER_CHAR_DESC)
  {
    if (connection_params.service == NULL)
    {
      connection_params.service = device->service_list;
    }
    else
    {
      connection_params.service = connection_params.service->next;      
    }

    status = ble_find_information ();
    if (status > 0)
    {
      if (connection_params.service->next == NULL)
      {
        device->status = BLE_DEVICE_DISCOVER_CHAR;
      }
    }
    else
    {
      device->status = BLE_DEVICE_DISCOVER_SERVICE;
      /* TODO: Clean-up service list */
      status = ble_connect_disconnect ();
    }
  }
  else
  {
    connection_params.attribute = NULL;
    
    if (connection_params.characteristics == NULL)
    {
      connection_params.service         = device->service_list;
      connection_params.characteristics = connection_params.service->char_list;
      connection_params.attribute       = &(connection_params.characteristics->declaration);
    }
    else if ((connection_params.characteristics->description.handle != BLE_INVALID_GATT_HANDLE) &&
             (connection_params.characteristics->description.value_length == 0))
    {
      connection_params.attribute = &(connection_params.characteristics->description);
    }
    else if ((connection_params.characteristics->client_config.handle != BLE_INVALID_GATT_HANDLE) &&
             (connection_params.characteristics->client_config.value_length == 0))
    {
      connection_params.attribute = &(connection_params.characteristics->client_config);
    }
    else if ((connection_params.characteristics->format.handle != BLE_INVALID_GATT_HANDLE) &&
             (connection_params.characteristics->format.value_length == 0))
    {
      connection_params.attribute = &(connection_params.characteristics->format);
    }
    else if (connection_params.characteristics->next != NULL)
    {
      connection_params.characteristics = connection_params.characteristics->next;
      connection_params.attribute = &(connection_params.characteristics->declaration);
    }
    else if (connection_params.service->next != NULL)
    {
      connection_params.service         = connection_params.service->next;
      connection_params.characteristics = connection_params.service->char_list;
      connection_params.attribute       = &(connection_params.characteristics->declaration);      
    }

    if (connection_params.attribute != NULL)
    {
      status = ble_read_long_handle ();
      if (status <= 0)
      {
        device->status = BLE_DEVICE_DISCOVER_SERVICE;
        /* TODO: Clean-up service list */
        status = ble_connect_disconnect ();
      }      
    }
    else
    {
      ble_update_data_list ();
      ble_print_service_list ();
      
      if (device->update_list != NULL)
      {
        device->id     = ble_identify_device (device->update_list);
        device->status = BLE_DEVICE_UPDATE_DATA;
      }
      else
      {
        device->status = BLE_DEVICE_IGNORE;
      }

      status = ble_connect_disconnect ();
    }
  }

  return status;
}

int ble_start_data (void)
{
  int status;

  connection_params.device = (ble_list_entry_t *)ble_data_list;
  while ((connection_params.device != NULL) &&
         (((ble_device_t *)(connection_params.device->data))->status != BLE_DEVICE_UPDATE_DATA))
  {
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    status = ble_connect_direct ();
  }
  else
  {
    status = 0;
  }
  
  return status;
}

int ble_update_data (void)
{
  int status;
  ble_device_t *device = (ble_device_t *)(connection_params.device->data);

  do
  {
    if (connection_params.characteristics == NULL)
    {
      connection_params.characteristics = device->update_list;
    }
    else
    {
      connection_params.characteristics = connection_params.characteristics->next;
    }
  } while ((connection_params.characteristics != NULL) &&
           (connection_params.characteristics->update.timer != 0));

  if (connection_params.characteristics != NULL)
  {
    connection_params.attribute = &(connection_params.characteristics->data);
    if (connection_params.characteristics->update.type == BLE_CHAR_READ_DATA)
    {
      status = ble_read_handle ();
    }
    else
    {
      status = ble_write_handle ();
    }
  }
  else
  {
    ble_data_callback ();
    status = ble_connect_disconnect ();
  }
    
  return status;
}

int ble_next_data (void)
{
  int status;
  
  /* Go to next device */
  do
  {
    connection_params.device = connection_params.device->next;
  } while ((connection_params.device != NULL) &&
           (((ble_device_t *)(connection_params.device->data))->status != BLE_DEVICE_UPDATE_DATA));
  
  if (connection_params.device != NULL)
  {
    status = ble_connect_direct ();
  }
  else
  {
    status = 0;
  }
  
  return status;
}

int ble_update_sleep (void)
{
  ble_device_list_entry_t *device_list;

  device_list = ble_device_list;
  sleep_interval = 0x7FFFFFFF;
  
  /* Loop through update list to find the minimum sleep interval */
  while (device_list != NULL)
  {
    ble_char_list_entry_t *char_list = device_list->info.update_list;
    while (char_list != NULL)
    {
      sleep_interval = (sleep_interval > char_list->update.timer) ? char_list->update.timer
                                                                  : sleep_interval;
      char_list = char_list->next;
    }
    device_list = device_list->next;
  }

  /* Subtract the sleep interval from timer. All characteristics
     with timer value 0 will be updated on wake-up */
  device_list = ble_device_list;
  while (device_list != NULL)
  {
    ble_char_list_entry_t *char_list = device_list->info.update_list;
    while (char_list != NULL)
    {
      char_list->update.timer -= sleep_interval;
      char_list = char_list->next;
    }
    device_list = device_list->next;
  }

  if (sleep_interval != 0x7FFFFFFF)
  {
    sleep_interval *= (60*1000);
  }
  else
  {
    sleep_interval = 10;
  }

  printf ("BLE sleep interval %d (ms)\n", sleep_interval);

  return sleep_interval;
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
    ble_list_entry_t *profile_list_entry = (ble_list_entry_t *)malloc (sizeof (*profile_list_entry));

    device               = &(device_list_entry->info);
    device->address      = scan_response->device_address;
    device->name         = NULL;
    device->status       = BLE_DEVICE_DISCOVER_SERVICE;
    device->service_list = NULL;
    device->update_list  = NULL;

    profile_list_entry->data = device;
    
    list_add ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);
    list_add ((list_entry_t **)(&ble_profile_list), (list_entry_t *)profile_list_entry);
    
    printf ("New device --\n");
  }
  else if (device->status == BLE_DEVICE_DISCOVER)
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

int ble_event_connection_status (ble_event_connection_status_t *connection_status)
{
  int status;

  printf ("BLE Connect status flags %02x, interval %d, timeout %d, latency %d, bonding %02x\n",
             connection_status->flags, connection_status->interval, connection_status->timeout,
             connection_status->latency, connection_status->bonding);

  if (connection_status->flags & BLE_CONNECT_ESTABLISHED)
  {
    timer_stop (&(((timer_list_entry_t *)connection_params.timer)->info));
    free (connection_params.timer);
    connection_params.timer = NULL;
    status = 1;
  }
  else if (connection_status->flags & BLE_CONNECT_FAILED)
  {
    ((ble_device_t *)(connection_params.device->data))->status = BLE_DEVICE_DISCOVER;
    status = ble_end_procedure ();
  }
  else
  {
    status = 0;
  }

  return status;
}

void ble_event_disconnect (ble_event_disconnect_t *disconnect)
{
  printf ("BLE Disconnect reason %04x\n", disconnect->cause);

  connection_params.service         = NULL;
  connection_params.characteristics = NULL;
  connection_params.attribute       = NULL;
  connection_params.handle          = 0xff;
  connection_params.timer           = NULL;
}

void ble_event_read_group (ble_event_read_group_t *read_group)
{
  ble_device_t *device;
  ble_service_list_entry_t *service_list_entry;

  device = (ble_device_t *)(connection_params.device->data);
  service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
  service_list_entry->include_list = NULL;
  service_list_entry->char_list    = NULL;
  service_list_entry->start_handle = read_group->start_handle;
  service_list_entry->end_handle   = read_group->end_handle;
  
  service_list_entry->declaration.handle       = read_group->start_handle;
  service_list_entry->declaration.uuid_length  = BLE_GATT_UUID_LENGTH;
  service_list_entry->declaration.uuid[0]      = (BLE_GATT_PRI_SERVICE & 0xff);
  service_list_entry->declaration.uuid[1]      = ((BLE_GATT_PRI_SERVICE & 0xff00) >> 8);
  service_list_entry->declaration.value_length = read_group->length;
  service_list_entry->declaration.value        = malloc (read_group->length);
  memcpy (service_list_entry->declaration.value, read_group->data, read_group->length);
  /* service_list_entry->attribute.permission = */

  list_add ((list_entry_t **)(&(device->service_list)), (list_entry_t *)service_list_entry);
}

void ble_event_find_information (ble_event_find_information_t *find_information)
{
  ble_service_list_entry_t *service = connection_params.service;
  
  if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
      (find_information->data[0] == (BLE_GATT_INCLUDE & 0xff)) &&
      (find_information->data[1] == ((BLE_GATT_INCLUDE & 0xff00) >> 8)))
  {
    /* TODO: Include service declaration */
    ble_service_list_entry_t *include_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*include_list_entry));
    
    include_list_entry->declaration.handle      = find_information->char_handle;
    include_list_entry->declaration.uuid_length = BLE_GATT_UUID_LENGTH;
    include_list_entry->declaration.uuid[0]     = (BLE_GATT_INCLUDE & 0xff);
    include_list_entry->declaration.uuid[1]     = ((BLE_GATT_INCLUDE & 0xff00) >> 8);

    list_add ((list_entry_t **)(&(service->include_list)), (list_entry_t *)include_list_entry);
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_DECL & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_DECL & 0xff00) >> 8)))
  {
    /* Characteristics declaration */
    ble_char_list_entry_t *char_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*char_list_entry));

    memset (char_list_entry, 0, sizeof (*char_list_entry));
    char_list_entry->declaration.handle      = find_information->char_handle;
    char_list_entry->declaration.uuid_length = find_information->length;
    memcpy (char_list_entry->declaration.uuid, find_information->data, find_information->length);

    list_add ((list_entry_t **)(&(service->char_list)), (list_entry_t *)char_list_entry);
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_USER_DESC & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_USER_DESC & 0xff00) >> 8)))
  {
    /* Characteristics user description */
    ble_char_list_entry_t *char_list_entry
                        = (ble_char_list_entry_t *)list_tail ((list_entry_t **)(&(service->char_list)));

    char_list_entry->description.handle      = find_information->char_handle;
    char_list_entry->description.uuid_length = find_information->length;
    memcpy (char_list_entry->description.uuid, find_information->data, find_information->length);
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_FORMAT & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_FORMAT & 0xff00) >> 8)))
  {
    /* Characteristics format */
    ble_char_list_entry_t *char_list_entry
                        = (ble_char_list_entry_t *)list_tail ((list_entry_t **)(&(service->char_list)));

    char_list_entry->format.handle      = find_information->char_handle;
    char_list_entry->format.uuid_length = find_information->length;
    memcpy (char_list_entry->format.uuid, find_information->data, find_information->length);
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_CLIENT_CONFIG  & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_CLIENT_CONFIG & 0xff00) >> 8)))
  {
    /* Characteristics client configuration */
    ble_char_list_entry_t *char_list_entry
                        = (ble_char_list_entry_t *)list_tail ((list_entry_t **)(&(service->char_list)));

    char_list_entry->client_config.handle      = find_information->char_handle;
    char_list_entry->client_config.uuid_length = find_information->length;
    memcpy (char_list_entry->client_config.uuid, find_information->data, find_information->length);
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_EXT & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_EXT & 0xff00) >> 8)))
  {
    /* Characteristics extended */
    printf ("Characteristics extended declaration descriptor not handle\n");
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_SERVER_CONFIG & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_SERVER_CONFIG & 0xff00) >> 8)))
  {
    /* Characteristics server configuration */
    printf ("Characteristics server configuration descriptor not handled\n");
  }
  else if ((find_information->length == BLE_GATT_UUID_LENGTH) &&
           (find_information->data[0] == (BLE_GATT_CHAR_AGG_FORMAT & 0xff)) &&
           (find_information->data[1] == ((BLE_GATT_CHAR_AGG_FORMAT & 0xff00) >> 8)))
  {
    /* Characteristics aggregrate format */
    printf ("Characteristics aggregrate format descriptor not handled\n");
  }
  else
  {
    /* Characteristics value */
    printf ("Characteristics value descriptor\n");
  }
}

void ble_event_attr_value (ble_event_attr_value_t *attr_value)
{
  ble_attribute_t *attribute = connection_params.attribute;

  attribute->value_length = attr_value->length;
  if (attribute->value == NULL)
  {
    attribute->value = malloc (attribute->value_length);
  }
  memcpy (attribute->value, attr_value->data, attribute->value_length);
}

