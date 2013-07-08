
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "ble.h"

struct timer_list_entry
{
  struct timer_list_entry *next;
  timer_info_t             info;
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

LIST_HEAD_INIT (ble_device_list_entry_t, ble_device_list);

typedef struct
{
  ble_device_list_entry_t  *device;
  ble_service_list_entry_t *service;
  ble_char_list_entry_t    *characteristics;
  ble_attribute_t          *attribute;
  uint8                     handle;
  timer_info_t             *timer_info;
} ble_connection_params_t;

static ble_connection_params_t connection_params = 
{
  .device          = NULL,
  .service         = NULL,
  .characteristics = NULL,
  .attribute       = NULL,
  .handle          = 0xff,
  .timer_info      = NULL,
};

static int32 ble_init_time = 0;


static void ble_update_sleep (void)
{
  int32 current_time;
  ble_device_list_entry_t *device_list_entry;
  
  current_time      = clock_current_time ();
  device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if ((device_list_entry->status == BLE_DEVICE_DATA)  ||
        (device_list_entry->status == BLE_DEVICE_DISCOVER))
    {
      ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

      while (service_list_entry != NULL)
      {
        if (service_list_entry->update.char_list != NULL)
        {
          service_list_entry->update.wait = service_list_entry->update.time - current_time;
          if (service_list_entry->update.wait < 0)
          {
            service_list_entry->update.wait = 0;
          }
        }
          
        service_list_entry = service_list_entry->next;
      }
    }
    
    device_list_entry = device_list_entry->next;
  }
}

static int32 ble_response (ble_message_t *response)
{
  ble_message_t message;
  int32 status;

  while ((status = serial_rx (sizeof (message.header), (uint8 *)(&(message.header)))) > 0)
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
        ble_message_list_entry_t *message_list_entry 
            = (ble_message_list_entry_t *)malloc (sizeof (*message_list_entry));
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

static int32 ble_command (ble_message_t *message)
{
  int32 status;

  status = serial_tx (((sizeof (message->header)) + message->header.length),
                      ((uint8 *)message));

  if (status > 0)
  {
    /* Check for response */
    status = ble_response (message);
  }

  return status;
}

static void ble_init_timer (void)
{
  int32 wakeup_time;
  int32 sleep_interval;
  ble_service_list_entry_t *service_list_entry = connection_params.device->service_list;
  
  wakeup_time    = clock_current_time ();
  sleep_interval = ble_get_sleep ();
  
  if (sleep_interval <= 20000)
  {
    wakeup_time += 20000;
  }
  else
  {
    wakeup_time += (sleep_interval - 10000);
  }
  
  while (service_list_entry != NULL)
  {
    if (service_list_entry->update.char_list != NULL)
    {
      service_list_entry->update.time = wakeup_time;
    }
      
    service_list_entry = service_list_entry->next;
  }
}

static int32 ble_reset (void)
{
  int32 status;
  ble_message_t message;
  ble_command_reset_t *reset;

  reset = (ble_command_reset_t *)(&message);
  BLE_CLASS_SYSTEM_HEADER (reset, BLE_COMMAND_RESET);
  reset->mode = BLE_RESET_NORMAL;
  status = serial_tx (((sizeof (reset->header)) + reset->header.length),
                      (uint8 *)reset);
  
  if (status <= 0)
  {
    printf ("BLE Reset failed with %d\n", status);
    status = -1;
  }
  
  return status;
}

static int32 ble_hello (void)
{
  int32 status;
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

static int32 ble_end_procedure (void)
{
  int32 status;
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

static int32 ble_connect_direct (void)
{
  int32 status;
  ble_message_t message;
  ble_command_connect_direct_t *connect_direct;

  printf ("BLE Connect direct request\n");
  ble_print_device (connection_params.device);

  connect_direct = (ble_command_connect_direct_t *)(&message);
  BLE_CLASS_GAP_HEADER (connect_direct, BLE_COMMAND_CONNECT_DIRECT);
  connect_direct->address      = connection_params.device->address;
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

  (void)timer_start (BLE_CONNECT_SETUP_TIMEOUT, BLE_TIMER_CONNECT_SETUP,
                     ble_callback_timer, &(connection_params.timer_info));

  return status;
}

static void ble_connect_disconnect (void)
{
  int32 status;
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

  if (connection_params.timer_info != NULL)
  {
    timer_stop (connection_params.timer_info);
    connection_params.timer_info = NULL;
  }
}

static int32 ble_read_group (void)
{
  int32 status;
  ble_message_t message;
  ble_command_read_group_t *read_group;

  printf ("BLE Read group request\n");
  
  read_group = (ble_command_read_group_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (read_group, BLE_COMMAND_READ_BY_GROUP_TYPE);
  read_group->conn_handle  = connection_params.handle;
  read_group->start_handle = BLE_MIN_GATT_HANDLE;
  read_group->end_handle   = BLE_MAX_GATT_HANDLE;
  read_group->length       = BLE_GATT_UUID_LENGTH;
  BLE_UNPACK_GATT_UUID (BLE_GATT_PRI_SERVICE, read_group->data);
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

static int32 ble_find_information (void)
{
  int32 status;
  ble_message_t message;
  ble_command_find_information_t *find_information;

  printf ("BLE Find information request, handle range 0x%04x to 0x%04x\n",
               connection_params.service->start_handle + 1,
               connection_params.service->end_handle);

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

static int32 ble_read_long_handle (void)
{
  int32 status;
  ble_message_t message;
  ble_command_read_handle_t *read_handle;

  printf ("BLE Read long request, handle 0x%04x\n", connection_params.attribute->handle);
  
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

static int32 ble_read_handle (void)
{
  int32 status;
  ble_message_t message;
  ble_command_read_handle_t *read_handle;

  printf ("BLE Read request, handle 0x%04x\n", connection_params.attribute->handle);
  
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

static int32 ble_write_handle (void)
{
  int32 status;
  ble_message_t message;
  ble_command_write_handle_t *write_handle;

  printf ("BLE Write request, handle 0x%04x\n", connection_params.attribute->handle);
  
  write_handle = (ble_command_write_handle_t *)(&message);
  BLE_CLASS_ATTR_CLIENT_HEADER (write_handle, BLE_COMMAND_WRITE_ATTR_CLIENT);
  write_handle->header.length += connection_params.attribute->data_length;

  write_handle->conn_handle = connection_params.handle;
  write_handle->attr_handle = connection_params.attribute->handle;
  write_handle->length      = connection_params.attribute->data_length;
  memcpy (write_handle->data, connection_params.attribute->data, write_handle->length);
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

void ble_callback_timer (void *timer_info)
{
  timer_list_entry_t *timer_list_entry;

  timer_list_entry = (timer_list_entry_t *)malloc (sizeof (*timer_list_entry));
  timer_list_entry->info = *((timer_info_t *)timer_info);
  list_add ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
}

void ble_event_scan_response (ble_event_scan_response_t *scan_response)
{
  ble_device_list_entry_t *device_list_entry;

  printf ("BLE Scan response event\n");

  device_list_entry = ble_find_device (ble_device_list, &(scan_response->device_address));
  if (device_list_entry != NULL)
  {
    if (device_list_entry->status == BLE_DEVICE_DISCOVER)
    {
      device_list_entry->status = BLE_DEVICE_DISCOVER_SERVICE;
      printf ("New device --\n");
    }
    else
    {
      printf ("Listed device --\n");
    }
  
    ble_print_device (device_list_entry);
  }
  else
  {
    printf ("Ignored device --\n");
  }
}

int32 ble_event_connection_status (ble_event_connection_status_t *connection_status)
{
  int32 status;

  printf ("BLE Connect event, status flags 0x%02x, interval %d, timeout %d, latency %d, bonding 0x%02x\n",
             connection_status->flags, connection_status->interval, connection_status->timeout,
             connection_status->latency, connection_status->bonding);

  if (connection_status->flags & BLE_CONNECT_ESTABLISHED)
  {
    timer_stop (connection_params.timer_info);
    connection_params.timer_info = NULL;

    timer_start (BLE_CONNECT_DATA_TIMEOUT, BLE_TIMER_CONNECT_DATA,
                 ble_callback_timer, &(connection_params.timer_info));
    status = 1; 
  }
  else if (connection_status->flags & BLE_CONNECT_SETUP_FAILED)
  {
    connection_params.timer_info = NULL;
    
    (void)ble_end_procedure ();
    status = -1;
  }
  else if (connection_status->flags & BLE_CONNECT_DATA_FAILED)
  {
    connection_params.timer_info = NULL;
    
    if (connection_params.device->status != BLE_DEVICE_DATA)
    {
      connection_params.device->status = BLE_DEVICE_DISCOVER_SERVICE;
    }
      
    ble_connect_disconnect ();
    status = -1;
  }
  else
  {
    status = 0;
  }

  return status;
}

void ble_event_disconnect (ble_event_disconnect_t *disconnect)
{
  printf ("BLE Disconnect event, reason 0x%04x\n", disconnect->cause);

  connection_params.service         = NULL;
  connection_params.characteristics = NULL;
  connection_params.attribute       = NULL;
  connection_params.handle          = 0xff;
    
  if (connection_params.timer_info != NULL)
  {
    timer_stop (connection_params.timer_info);
    connection_params.timer_info = NULL;
  }
}

void ble_event_read_group (ble_event_read_group_t *read_group)
{
  ble_service_list_entry_t *service_list_entry;

  printf ("BLE Read group event\n");

  service_list_entry = ble_find_service (connection_params.device, read_group->data, read_group->length);

  if (service_list_entry != NULL)
  {
    service_list_entry->start_handle = read_group->start_handle;
    service_list_entry->end_handle   = read_group->end_handle;

    service_list_entry->declaration->type        = BLE_ATTR_TYPE_READ;
    service_list_entry->declaration->handle      = read_group->start_handle;
    service_list_entry->declaration->uuid_length = BLE_GATT_UUID_LENGTH;
    BLE_UNPACK_GATT_UUID (BLE_GATT_PRI_SERVICE, service_list_entry->declaration->uuid);
  }
  else
  {
    int32 i;
    
    printf ("BLE ignore service uuid 0x");
    for (i = 0; i < read_group->length; i++)
    {
      printf ("%02x", read_group->data[i]);
    }
    printf ("\n");
  }
}

void ble_event_find_information (ble_event_find_information_t *find_information)
{
  ble_service_list_entry_t *service = connection_params.service;
  
  printf ("BLE Find information event\n");

  if (find_information->length == BLE_GATT_UUID_LENGTH)
  {
    uint16 uuid = BLE_PACK_GATT_UUID (find_information->data);
    
    if (uuid == BLE_GATT_INCLUDE)
    {
      /* TODO: Include service declaration */
      ble_service_list_entry_t *include_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*include_list_entry));
      
      include_list_entry->declaration->handle      = find_information->attr_handle;
      include_list_entry->declaration->uuid_length = BLE_GATT_UUID_LENGTH;
      BLE_UNPACK_GATT_UUID (BLE_GATT_INCLUDE, include_list_entry->declaration->uuid);
  
      list_add ((list_entry_t **)(&(service->include_list)), (list_entry_t *)include_list_entry);  
    }
    else if ((uuid == BLE_GATT_CHAR_DECL)           ||
             (uuid == BLE_GATT_CHAR_USER_DESC)      ||
             (uuid == BLE_GATT_CHAR_FORMAT)         ||
             (uuid == BLE_GATT_CHAR_CLIENT_CONFIG))
    {
      ble_char_list_entry_t *char_list_entry;
      ble_attribute_t *attribute;

      if (uuid == BLE_GATT_CHAR_DECL)
      {
        char_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*char_list_entry));
        
        char_list_entry->declaration   = NULL;
        char_list_entry->value         = NULL;
        char_list_entry->description   = NULL;
        char_list_entry->client_config = NULL;
        char_list_entry->format        = NULL;
        
        list_add ((list_entry_t **)(&(service->char_list)), (list_entry_t *)char_list_entry);
      }
      else
      {
        char_list_entry = (ble_char_list_entry_t *)list_tail ((list_entry_t **)(&(service->char_list)));
      }

      if (uuid == BLE_GATT_CHAR_DECL)
      {
        char_list_entry->declaration = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        attribute                    = char_list_entry->declaration;
      }
      else if (uuid == BLE_GATT_CHAR_USER_DESC)
      {
        char_list_entry->description = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        attribute                    = char_list_entry->description;
      }
      else if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
      {
        char_list_entry->client_config = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        attribute                      = char_list_entry->client_config;
      }
      else
      {
        char_list_entry->format = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        attribute               = char_list_entry->format;
      }

      attribute->type         = BLE_ATTR_TYPE_READ;
      attribute->handle       = find_information->attr_handle;
      attribute->uuid_length  = find_information->length;
      memcpy (attribute->uuid, find_information->data, find_information->length);
      attribute->data_length  = 0;
      attribute->data         = NULL;

      if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
      {
        attribute->type |= BLE_ATTR_TYPE_WRITE;
      }
    }
    else if ((uuid == BLE_GATT_CHAR_EXT)           ||
             (uuid == BLE_GATT_CHAR_SERVER_CONFIG) ||
             (uuid == BLE_GATT_CHAR_AGG_FORMAT)    ||
             (uuid == BLE_GATT_CHAR_VALID_RANGE))
    {
      printf ("Characteristics descriptor 0x%04x not handled\n", uuid);
    }
    else
    {
      printf ("Characteristics value descriptor (16-bit)\n");
    }
  }
  else
  {
    printf ("Characteristics value descriptor (128-bit)\n");
  }
}

void ble_event_attr_value (ble_event_attr_value_t *attr_value)
{
  ble_attribute_t *attribute = NULL;

  printf ("BLE Attribute value event, type %d, handle 0x%04x\n", attr_value->type, attr_value->attr_handle);

  attribute = ble_find_attribute (connection_params.device->service_list, attr_value->attr_handle);

  if (attribute != NULL)
  {
    if ((attr_value->type == BLE_ATTR_VALUE_READ)      ||
        (attr_value->type == BLE_ATTR_VALUE_NOTIFY)    ||
        (attr_value->type == BLE_ATTR_VALUE_INDICATE)  ||
        (attr_value->type == BLE_ATTR_VALUE_READ_TYPE) ||
        ((attr_value->type == BLE_ATTR_VALUE_READ_BLOB) && (attribute->data == NULL)))
    {
      attribute->data = malloc (attr_value->length);
      memcpy (attribute->data, attr_value->data, attr_value->length);
      attribute->data_length = attr_value->length;
    }
    else
    {
      attribute->data = realloc (attribute->data, (attribute->data_length + attr_value->length));
      memcpy ((attribute->data + attribute->data_length), attr_value->data, attr_value->length);
      attribute->data_length += attr_value->length;
    }  
  }
  else
  {
    printf ("Attribute value handle %04x not expected\n", attr_value->attr_handle);
  }

  if ((attr_value->type == BLE_ATTR_VALUE_NOTIFY) ||
      (attr_value->type == BLE_ATTR_VALUE_INDICATE))
  {
    ble_message_list_entry_t *message_list_entry
        = (ble_message_list_entry_t *)malloc (sizeof (*message_list_entry));

    message_list_entry->message.header.type    = BLE_EVENT;
    message_list_entry->message.header.length  = 5;
    message_list_entry->message.header.class   = BLE_CLASS_ATTR_CLIENT;
    message_list_entry->message.header.command = BLE_EVENT_PROCEDURE_COMPLETED;

    list_add ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
  }
}

int32 ble_init (void)
{
  int32 status = -1;
  int32 ble_init_attempt = 0;

  ble_init_time = clock_current_time ();

  do
  {
    ble_init_attempt++;

    printf ("BLE init attempt %d\n", ble_init_attempt);
    
    /* Find BLE device and initialize */
    status = serial_init ();
    sleep (1);
  
    if (status > 0)
    {
      int32 serial_init_attempt = 0;
      
      printf ("BLE Reset request\n");
  
        /* Reset BLE */
      (void)ble_reset ();
      sleep (1);
  
      /* Close & re-open UART after reset */
      serial_deinit ();
  
      do
      {
        serial_init_attempt++;
        sleep (1);
        
        status = serial_init ();
      } while ((status < 0) && (serial_init_attempt < 2));
  
      if (status > 0)
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
  } while ((status < 0) && (ble_init_attempt < 2));

  return status;
}

void ble_deinit (void)
{
  serial_deinit ();
}

void ble_print_message (ble_message_t *message)
{
 printf ("Message not handled\n");
 printf (" type 0x%02x, length %d, class 0x%02x, command 0x%02x\n", message->header.type,
                                                                    message->header.length,
                                                                    message->header.class,
                                                                    message->header.command);
}

int32 ble_check_scan_list (void)
{
  int32 found = 0;
  ble_device_list_entry_t *device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if (device_list_entry->status == BLE_DEVICE_DISCOVER)
    {
      found++;
    }
    
    device_list_entry = device_list_entry->next;
  }
  
  return found;
}

int32 ble_check_profile_list (void)
{
  int32 found = 0;
  ble_device_list_entry_t *device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if (device_list_entry->status == BLE_DEVICE_DISCOVER_SERVICE)
    {
      found++;
    }
    
    device_list_entry = device_list_entry->next;
  }
  
  return found;  
}

int32 ble_check_data_list (void)
{
  int32 found = 0;
  ble_device_list_entry_t *device_list_entry = ble_device_list;

  while (device_list_entry != NULL)
  {
    if (device_list_entry->status == BLE_DEVICE_DATA)
    {
      found++;
    }
    
    device_list_entry = device_list_entry->next;
  }
  
  return found;
}

int32 ble_check_message_list (void)
{
  int32 status;
  int32 pending;
  ble_message_t message;

  pending = list_length ((list_entry_t **)(&timer_expiry_list));

  while ((status = serial_rx (sizeof (message.header), (uint8 *)(&(message.header)))) > 0)
  {
    if (message.header.length > 0)
    {
      status = serial_rx (message.header.length, message.data);
    }

    if (status > 0)
    {
      ble_message_list_entry_t *message_list_entry
          = (ble_message_list_entry_t *)malloc (sizeof (*message_list_entry));
      message_list_entry->message = message;
      list_add ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    }
  }

  pending += (list_length ((list_entry_t **)(&ble_message_list)));

  return pending;
}

int32 ble_receive_message (ble_message_t *message)
{
  int32 pending;

  if (timer_expiry_list != NULL)
  {
    timer_list_entry_t *timer_list_entry = timer_expiry_list;

    message->header.type    = BLE_EVENT;
    message->header.length  = 1;
    message->header.class   = BLE_CLASS_HW;
    message->header.command = BLE_EVENT_SOFT_TIMER;
    message->data[0]        = (uint8)(timer_list_entry->info.event);
    
    list_remove ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
    free (timer_list_entry);
  }
  else if (ble_message_list != NULL)
  {
    ble_message_list_entry_t *message_list_entry;

    message_list_entry = ble_message_list;
    *message = message_list_entry->message;
    list_remove ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    free (message_list_entry);
  }

  pending  = list_length ((list_entry_t **)(&timer_expiry_list));
  pending += (list_length ((list_entry_t **)(&ble_message_list)));

  return pending;
}

void ble_start_scan (void)
{
  int32 status;
  ble_message_t message;
  ble_command_scan_params_t *scan_params;

  ble_update_sleep ();
  ble_get_device_list (&ble_device_list);

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

  (void)timer_start (BLE_SCAN_DURATION, BLE_TIMER_SCAN_STOP,
                     ble_callback_timer, &(connection_params.timer_info));
}

void ble_stop_scan (void)
{
  ble_update_sleep ();
  connection_params.timer_info = NULL;
  (void)ble_end_procedure ();
}

void ble_start_profile (void)
{
  ble_update_sleep ();
  
  connection_params.device = ble_device_list;
  while ((connection_params.device != NULL) &&
         (connection_params.device->status != BLE_DEVICE_DISCOVER_SERVICE))
  {
    connection_params.device = connection_params.device->next;
  }

  ble_clear_service (connection_params.device);
  (void)ble_connect_direct ();
}

void ble_next_profile (void)
{
  ble_update_sleep ();
  
  connection_params.device = connection_params.device->next;
  while ((connection_params.device != NULL) &&
         (connection_params.device->status != BLE_DEVICE_DISCOVER_SERVICE))
  {
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    ble_clear_service (connection_params.device);
    (void)ble_connect_direct ();
  }
  else
  {
    timer_info_t *timer_info = NULL;
    
    (void)timer_start (BLE_MIN_TIMER_DURATION, BLE_TIMER_PROFILE_STOP,
                       ble_callback_timer, &timer_info);    
  }
}

void ble_read_profile (void)
{
  int32 status;

  if (connection_params.device->status == BLE_DEVICE_DISCOVER_SERVICE)
  {
    status = ble_read_group ();
    if (status > 0)
    {
      connection_params.device->status = BLE_DEVICE_DISCOVER_CHAR_DESC;
    }
    else
    {
      ble_connect_disconnect ();
    }
  }
  else if (connection_params.device->status == BLE_DEVICE_DISCOVER_CHAR_DESC)
  {
    if (connection_params.service == NULL)
    {
      connection_params.service = connection_params.device->service_list;
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
        connection_params.device->status = BLE_DEVICE_DISCOVER_CHAR;
      }
    }
    else
    {
      connection_params.device->status = BLE_DEVICE_DISCOVER_SERVICE;
      ble_connect_disconnect ();
    }
  }
  else if (connection_params.device->status == BLE_DEVICE_DISCOVER_CHAR)
  {
    int32 char_read_pending = 0;
    
    if (connection_params.attribute == NULL)
    {
      connection_params.service         = connection_params.device->service_list;
      connection_params.characteristics = connection_params.service->char_list;
      connection_params.attribute       = connection_params.characteristics->declaration;

      if ((connection_params.characteristics->description != NULL) ||
          (connection_params.characteristics->client_config != NULL) ||
          (connection_params.characteristics->format != NULL))
      {
        char_read_pending = 1;
      }
    }
    else
    {
      if ((connection_params.characteristics->description != NULL) &&
          (connection_params.characteristics->description->data == NULL))
      {
        connection_params.attribute = connection_params.characteristics->description;

        if ((connection_params.characteristics->client_config != NULL) ||
            (connection_params.characteristics->format != NULL))
        {
          char_read_pending = 1;
        }
      }
      else if ((connection_params.characteristics->client_config != NULL) &&
               (connection_params.characteristics->client_config->data == NULL))
      {
        connection_params.attribute = connection_params.characteristics->client_config;

        if (connection_params.characteristics->format != NULL)
        {
          char_read_pending = 1;
        }        
      }      
      else if ((connection_params.characteristics->format != NULL) &&
               (connection_params.characteristics->format->data == NULL))
      {
        connection_params.attribute = connection_params.characteristics->format;
      }
      else
      {
        if (connection_params.characteristics->next == NULL)
        {
          connection_params.service         = connection_params.service->next;
          connection_params.characteristics = connection_params.service->char_list;
          connection_params.attribute       = connection_params.characteristics->declaration;        
        }
        else
        {
          connection_params.characteristics = connection_params.characteristics->next;
          connection_params.attribute       = connection_params.characteristics->declaration;
        }
      
        if ((connection_params.characteristics->description != NULL) ||
            (connection_params.characteristics->client_config != NULL) ||
            (connection_params.characteristics->format != NULL))
        {
          char_read_pending = 1;
        }
      }
    }

    status = ble_read_long_handle ();
    if (status > 0)
    {
      if ((!char_read_pending)                              &&
          (connection_params.characteristics->next == NULL) &&
          (connection_params.service->next == NULL))
      {
        connection_params.device->status = BLE_DEVICE_CONFIGURE_CHAR;
        printf ("Done\n");
      }
    }
    else
    {
      connection_params.device->status = BLE_DEVICE_DISCOVER_SERVICE;
      ble_connect_disconnect ();
    }
  }
  else
  {
    ble_print_service (connection_params.device);

    if ((ble_init_service (connection_params.device)) > 0)
    {
      int32 current_time = clock_current_time ();
      ble_service_list_entry_t *service_list_entry = connection_params.device->service_list;
  
      while (service_list_entry != NULL)
      {
        if (service_list_entry->update.char_list != NULL)
        {
          service_list_entry->update.time = current_time;
        }
          
        service_list_entry = service_list_entry->next;
      }

      connection_params.device->status = BLE_DEVICE_DATA;
    }
    else
    {
      connection_params.device->status = BLE_DEVICE_DISCOVER_SERVICE;
    }

    ble_connect_disconnect ();
  }
}

void ble_start_data (void)
{
  ble_update_sleep ();
  
  connection_params.device  = ble_device_list;
  while (connection_params.device != NULL)
  {
    if (connection_params.device->status == BLE_DEVICE_DATA)
    {
      connection_params.service = connection_params.device->service_list;
      while (connection_params.service != NULL)
      {
        if ((connection_params.service->update.char_list != NULL) &&
            (connection_params.service->update.wait <= 0))
        {
          break;
        }
          
        connection_params.service = connection_params.service->next;
      }
    }

    if (connection_params.service != NULL)
    {
      break;
    }
    
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    (void)ble_connect_direct ();
  }
}

void ble_next_data (void)
{
  connection_params.device->callback (connection_params.device);  
  ble_update_sleep ();

  connection_params.device = connection_params.device->next;
  while (connection_params.device != NULL)
  {
    if (connection_params.device->status == BLE_DEVICE_DATA)
    {
      connection_params.service = connection_params.device->service_list;
      while (connection_params.service != NULL)
      {
        if ((connection_params.service->update.char_list != NULL) &&
            (connection_params.service->update.wait <= 0))
        {
          break;
        }
          
        connection_params.service = connection_params.service->next;
      }
    }

    if (connection_params.service != NULL)
    {
      break;
    }
    
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    (void)ble_connect_direct ();
  }
  else
  {
    timer_info_t *timer_info = NULL;
    
    if (((clock_current_time ())- ble_init_time) > (24*60*60*1000))
    {
      ble_deinit ();
      (void)ble_init ();
    }
    
    (void)timer_start (BLE_MIN_TIMER_DURATION, BLE_TIMER_DATA_STOP,
                       ble_callback_timer, &timer_info);
  }
}

void ble_update_data (void)
{
  int32 status = 1;

  if (connection_params.handle != 0xff)
  {
    int32 update_pending = 0;
    ble_service_list_entry_t *service_list_entry = connection_params.device->service_list;

    while ((service_list_entry != NULL) && (!update_pending))
    {
      ble_char_list_entry_t *update_list_entry = service_list_entry->update.char_list;

      while ((update_list_entry != NULL) && (!update_pending))
      {
        if ((((update_list_entry->value->type & BLE_ATTR_TYPE_WRITE) &&
             (update_list_entry->value->data != NULL))) ||
            ((update_list_entry->value->type & (BLE_ATTR_TYPE_READ | BLE_ATTR_TYPE_NOTIFY | BLE_ATTR_TYPE_INDICATE)) &&
             (update_list_entry->value->data == NULL)))
        {
          update_pending = 1;
        }
          
        update_list_entry = update_list_entry->next;
      }

      service_list_entry = service_list_entry->next;
    }
    
    if ((update_pending) && (connection_params.service != NULL))
    {
      if (connection_params.characteristics == NULL)
      {
        connection_params.characteristics = connection_params.service->update.char_list;
      }
      else
      {
        if (connection_params.characteristics->next == NULL)
        {
          connection_params.service = connection_params.service->next;
          
          while (connection_params.service != NULL)
          {
            if ((connection_params.service->update.char_list != NULL) &&
                (connection_params.service->update.wait <= 0))
            {
              connection_params.characteristics = connection_params.service->update.char_list;
              break;
            }
  
            connection_params.service = connection_params.service->next;
          }
        }
        else
        {
          connection_params.characteristics = connection_params.characteristics->next;
        }
      }
      
      if (connection_params.characteristics->value->type & BLE_ATTR_TYPE_READ)
      {
        connection_params.attribute = connection_params.characteristics->value;
        
        status = ble_read_long_handle ();
      }
      else
      {
        if ((connection_params.characteristics->value->type & (BLE_ATTR_TYPE_NOTIFY |
                                                               BLE_ATTR_TYPE_INDICATE)))
        {
          connection_params.attribute = connection_params.characteristics->client_config;
        }
        else
        {
          connection_params.attribute = connection_params.characteristics->value;
        }
        
        status = ble_write_handle ();

        if (connection_params.characteristics->value->type & BLE_ATTR_TYPE_WRITE)
        {
          free (connection_params.attribute->data);
          connection_params.attribute->data        = NULL;
          connection_params.attribute->data_length = 0;
        }
      }
    }
    else if (!update_pending)
    {
      ble_connect_disconnect ();
    }
  }
  else
  {
    status = 0;
  }
}

int32 ble_get_sleep (void)
{
  int32 min_sleep_interval;
  ble_device_list_entry_t *device_list_entry;

  min_sleep_interval = 0x7fffffff;
  device_list_entry  = ble_device_list;

  /* Loop through update list to find the minimum sleep interval */
  while (device_list_entry != NULL)
  {
    if ((device_list_entry->status == BLE_DEVICE_DATA)  ||
        (device_list_entry->status == BLE_DEVICE_DISCOVER))
    {
      ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

      while (service_list_entry != NULL)
      {
        if (service_list_entry->update.char_list != NULL)
        {
          min_sleep_interval = (min_sleep_interval > service_list_entry->update.wait)
                                ? service_list_entry->update.wait : min_sleep_interval;
        }

        service_list_entry = service_list_entry->next;
      }
    }
    
    device_list_entry = device_list_entry->next;
  }

  if ((min_sleep_interval == 0x7fffffff) || (min_sleep_interval == 0))
  {
    min_sleep_interval = 10;
  }
  
  return min_sleep_interval;
}

