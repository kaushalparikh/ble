
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

struct ble_device_list_entry
{
  struct ble_device_list_entry *next;
  ble_device_t                  info;
};

typedef struct ble_device_list_entry ble_device_list_entry_t;

LIST_HEAD_INIT (ble_device_list_entry_t, ble_device_list);
LIST_HEAD_INIT (ble_device_list_entry_t, ble_ignore_list);

typedef struct
{
  ble_device_list_entry_t  *device;
  ble_service_list_entry_t *service;
  ble_char_list_entry_t    *characteristics;
  ble_attr_list_entry_t    *attribute;
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


static void ble_callback_data (void)
{
  ble_device_list_entry_t *device_list;

  device_list = ble_device_list;

  while (device_list != NULL)
  {
    if ((device_list->info.status == BLE_DEVICE_DISCOVER) ||
        (device_list->info.status == BLE_DEVICE_DATA))
    {
      ble_char_list_entry_t *update_list = device_list->info.update_list;
      while (update_list != NULL)
      {
        if (update_list->update.timer <= 0)
        {
          update_list->update.callback (device_list->info.id, update_list);
        }
        
        update_list = update_list->next;
      }
    }

    device_list = device_list->next;
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

static ble_device_t * ble_find_device (ble_device_address_t *address)
{
  ble_device_t *device = NULL;
  ble_device_list_entry_t *device_list_entry;

  device_list_entry = ble_device_list;
  while (device_list_entry != NULL)
  {
    if ((memcmp (address, &(device_list_entry->info.address), sizeof (*address))) == 0)
    {
      device = &(device_list_entry->info);
      break;
    }
    device_list_entry = device_list_entry->next;
  }

  if (device == NULL)
  {
    device_list_entry = ble_ignore_list;
    while (device_list_entry != NULL)
    {
      if ((memcmp (address, &(device_list_entry->info.address), sizeof (*address))) == 0)
      {
        device = &(device_list_entry->info);
        break;
      }
      device_list_entry = device_list_entry->next;
    }
  }

  return device;
}

static void ble_print_device (ble_device_t *device)
{
  int32 i;

  printf ("     Name: %s\n", device->name);
  printf ("  Address: 0x");
  for (i = 0; i < BLE_DEVICE_ADDRESS_LENGTH; i++)
  {
    printf ("%02x", device->address.byte[i]);
  }
  printf (" (%s)\n", ((device->address.type > 0) ? "random" : "public"));
}

static void ble_print_service_list (void)
{
  int32 i;
  ble_service_list_entry_t *service_list = connection_params.device->info.service_list;

  while (service_list != NULL)
  {
    uint16 uuid = ((service_list->declaration.uuid[1]) << 8) | service_list->declaration.uuid[0];
    ble_char_list_entry_t *char_list = service_list->char_list;
      
    printf ("Service -- %s\n", ((uuid == BLE_GATT_PRI_SERVICE) ? "primary" : "secondary"));
    printf ("  handle range: 0x%04x to 0x%04x\n", service_list->start_handle, service_list->end_handle);
    printf ("          uuid: 0x");
    for (i = (service_list->declaration.value_length - 1); i >= 0; i--)
    {
      printf ("%02x", service_list->declaration.value[i]);
    }
    printf ("\n");

    while (char_list != NULL)
    {
      ble_attr_list_entry_t *desc_list = char_list->desc_list;

      while (desc_list != NULL)
      {
        uuid = ((desc_list->uuid[1]) << 8) | desc_list->uuid[0];
        if (uuid == BLE_GATT_CHAR_DECL)
        {
          printf ("    Characteristics -- declaration\n");
          printf ("      handle: 0x%04x\n", desc_list->handle);
          printf ("       value: ");
        }
        else
        {
          if (uuid == BLE_GATT_CHAR_USER_DESC)
          {
            printf ("        Descriptor -- description\n");
          }
          else if (uuid == BLE_GATT_CHAR_FORMAT)
          {
            printf ("        Descriptor -- format\n");
          }
          else if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
          {
            printf ("        Descriptor -- client configuration\n");
          }

          printf ("          handle: 0x%04x\n", desc_list->handle);
          printf ("           value: ");
        }

        if (uuid == BLE_GATT_CHAR_DECL)
        {
          ble_char_decl_t *char_decl = (ble_char_decl_t *)(desc_list->value);
          uint8 value_uuid_length = desc_list->value_length - 3;
          printf ("0x%02x, 0x%04x, 0x", char_decl->properties, char_decl->value_handle);
          for (i = (value_uuid_length - 1); i >= 0; i--)
          {
            printf ("%02x", char_decl->value_uuid[i]);
          }
          printf ("\n");
        }
        else if (uuid == BLE_GATT_CHAR_USER_DESC)
        {
          for (i = 0; i < desc_list->value_length; i++)
          {
            printf ("%c", (int8)(desc_list->value[i]));
          }
          printf ("\n");
        }
        else if (uuid == BLE_GATT_CHAR_FORMAT)
        {
          ble_char_format_t *char_format = (ble_char_format_t *)(desc_list->value);
          printf ("0x%02x, %d\n", char_format->bitfield, char_format->exponent);
        }
        else if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
        {
          ble_char_client_config_t *char_client_config = (ble_char_client_config_t *)(desc_list->value);
          printf ("0x%04x\n", char_client_config->bitfield);
        }
        
        desc_list = desc_list->next;
      }
      
      char_list = char_list->next;
    }
    
    service_list = service_list->next;
  }
}

static void ble_free_service_list (void)
{
  ble_service_list_entry_t *service_list = connection_params.device->info.service_list;
  
  while (service_list != NULL)
  {
    ble_service_list_entry_t *service_list_entry = service_list;
    ble_char_list_entry_t *char_list = service_list->char_list;

    while (char_list != NULL)
    {
      ble_char_list_entry_t *char_list_entry = char_list;
      ble_attr_list_entry_t *desc_list = char_list->desc_list;

      while (desc_list != NULL)
      {
        ble_attr_list_entry_t *desc_list_entry = desc_list;
        list_remove ((list_entry_t **)(&desc_list), (list_entry_t *)desc_list_entry);
        free (desc_list_entry);
      }

      list_remove ((list_entry_t **)(&char_list), (list_entry_t *)char_list_entry);
      free (char_list_entry);      
    }

    list_remove ((list_entry_t **)(&service_list), (list_entry_t *)service_list_entry);
    free (service_list_entry);
  }

  connection_params.device->info.service_list = NULL;
}

static void ble_update_data_list (void)
{
  ble_device_t *device = &(connection_params.device->info);
  ble_service_list_entry_t *service_list = device->service_list;
  ble_char_list_entry_t *read_write_list = NULL;
  ble_char_list_entry_t *notify_indicate_list = NULL;
  
  while (service_list != NULL)
  {
    ble_char_list_entry_t *char_list = service_list->char_list;

    while (char_list != NULL)
    {
      if ((ble_lookup_uuid (char_list)) > 0)
      {
        ble_char_list_entry_t *update_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*update_list_entry));
        
        *update_list_entry = *char_list;
        if ((update_list_entry->update.type == BLE_CHAR_PROPERTY_READ) ||
            (update_list_entry->update.type == BLE_CHAR_PROPERTY_WRITE))
        {
          list_add ((list_entry_t **)(&read_write_list), (list_entry_t *)update_list_entry);
        }
        else if ((update_list_entry->update.type == BLE_CHAR_PROPERTY_NOTIFY) ||
                 (update_list_entry->update.type == BLE_CHAR_PROPERTY_INDICATE))
        {
          list_add ((list_entry_t **)(&notify_indicate_list), (list_entry_t *)update_list_entry);
        }
      }
      
      char_list = char_list->next;
    }
    
    service_list = service_list->next;
  }

  if ((notify_indicate_list != NULL) || (read_write_list != NULL))
  {
    list_concat ((list_entry_t **)(&(device->update_list)), (list_entry_t *)notify_indicate_list);
    list_concat ((list_entry_t **)(&(device->update_list)), (list_entry_t *)read_write_list);
  }
}

static void ble_update_timer (void)
{
  int32 current_time;
  ble_device_list_entry_t *device_list;

  current_time = clock_current_time ();
  device_list  = ble_device_list;

  while (device_list != NULL)
  {
    if ((device_list->info.status == BLE_DEVICE_DATA)  ||
        (device_list->info.status == BLE_DEVICE_DISCOVER))
    {
      ble_char_list_entry_t *update_list = device_list->info.update_list;

      while (update_list != NULL)
      {
        update_list->update.timer = update_list->update.expected_time - current_time;
        if (update_list->update.timer < 0)
        {
          update_list->update.timer = 0;
        }
          
        update_list = update_list->next;
      }
    }
    
    device_list = device_list->next;
  }
}

static int32 ble_wait_data (void)
{
  int32 status = -1;
  ble_char_list_entry_t *update_list = connection_params.device->info.update_list;

  while (update_list != NULL)
  {
    if (update_list->update.timer == 0)
    {
      status = 1;
      break;
    }
    
    update_list = update_list->next;
  }

  return status;
}

static int32 ble_get_wakeup (void)
{
  int32 wakeup_time;
  ble_device_list_entry_t *device_list;
  
  wakeup_time = 0x7fffffff;
  device_list = ble_device_list;

  while (device_list != NULL)
  {
    if ((device_list->info.status == BLE_DEVICE_DATA)  ||
        (device_list->info.status == BLE_DEVICE_DISCOVER))
    {
      ble_char_list_entry_t *update_list = device_list->info.update_list;

      while (update_list != NULL)
      {
        wakeup_time = (wakeup_time > update_list->update.expected_time)
                       ? update_list->update.expected_time : wakeup_time;
          
        update_list = update_list->next;
      }
    }
    
    device_list = device_list->next;
  }

  if (wakeup_time == 0x7fffffff)
  {
    wakeup_time = clock_current_time ();
  }

  return wakeup_time;
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
  ble_device_t *device = &(connection_params.device->info);

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
      ((timer_start (BLE_CONNECT_SETUP_TIMEOUT, BLE_TIMER_CONNECT_SETUP,
                     ble_callback_timer, &(connection_params.timer_info))) != 0))
  {
    status = -1;
  }

  return status;
}

static int32 ble_connect_disconnect (void)
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

  return status;
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
  write_handle->header.length += connection_params.attribute->value_length;

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

void ble_callback_timer (void *timer_info)
{
  timer_list_entry_t *timer_list_entry;

  timer_list_entry = (timer_list_entry_t *)malloc (sizeof (*timer_list_entry));
  timer_list_entry->info = *((timer_info_t *)timer_info);
  list_add ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
}

void ble_event_scan_response (ble_event_scan_response_t *scan_response)
{
  int32 i;
  ble_device_t *device;

  printf ("BLE Scan response event\n");

  device = ble_find_device (&(scan_response->device_address));
  if (device == NULL)
  {
    ble_device_list_entry_t *device_list_entry 
        = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));

    device               = &(device_list_entry->info);
    device->address      = scan_response->device_address;
    device->name         = NULL;
    device->status       = BLE_DEVICE_DISCOVER_SERVICE;
    device->service_list = NULL;
    device->update_list  = NULL;
    
    list_add ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);
    printf ("New device --\n");
  }
  else if (device->status == BLE_DEVICE_DISCOVER)
  {
    device->status = BLE_DEVICE_DATA;
    printf ("Found device --\n");
  }
  else if (device->status != BLE_DEVICE_IGNORE)
  {
    printf ("Listed device --\n");
  }
  else
  {
    printf ("Ignored device --\n");
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
      device->name = (int8 *)malloc (adv_data->length);
      device->name[adv_data->length-1] = '\0';
      strncpy (device->name, (int8 *)(adv_data->value), (adv_data->length-1));
    }
    else if ((adv_data->type == BLE_ADV_LOCAL_NAME_SHORT) &&
             (device->name != NULL))
    {
      device->name = (int8 *)malloc (adv_data->length);
      device->name[adv_data->length-1] = '\0';
      strncpy (device->name, (int8 *)(adv_data->value), (adv_data->length-1));
    }
  }

  ble_print_device (device);
}

int32 ble_event_connection_status (ble_event_connection_status_t *connection_status)
{
  int32 status;

  printf ("BLE Connect event, status flags 0x%02x, interval %d, timeout %d, latency %d, bonding 0x%02x\n",
             connection_status->flags, connection_status->interval, connection_status->timeout,
             connection_status->latency, connection_status->bonding);

  if (connection_status->flags & BLE_CONNECT_ESTABLISHED)
  {
    connection_params.device->info.setup_time = timer_status (connection_params.timer_info);

    timer_stop (connection_params.timer_info);
    connection_params.timer_info = NULL;

    timer_start (BLE_CONNECT_DATA_TIMEOUT, BLE_TIMER_CONNECT_DATA,
                 ble_callback_timer, &(connection_params.timer_info));
    status = 1; 
  }
  else if (connection_status->flags & BLE_CONNECT_SETUP_FAILED)
  {
    connection_params.timer_info = NULL;
    connection_params.device->info.setup_time = BLE_CONNECT_SETUP_TIMEOUT;
    
    status = ble_end_procedure ();
  }
  else if (connection_status->flags & BLE_CONNECT_DATA_FAILED)
  {
    connection_params.timer_info = NULL;
    
    if (connection_params.device->info.status != BLE_DEVICE_DATA)
    {
      connection_params.device->info.status = BLE_DEVICE_DISCOVER_SERVICE;
      ble_free_service_list ();
    }
      
    status = ble_connect_disconnect ();
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
  ble_device_t *device = &(connection_params.device->info);

  printf ("BLE Read group event\n");

  service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
  service_list_entry->include_list = NULL;
  service_list_entry->char_list    = NULL;
  service_list_entry->start_handle = read_group->start_handle;
  service_list_entry->end_handle   = read_group->end_handle;

  service_list_entry->declaration.next         = NULL;
  service_list_entry->declaration.handle       = read_group->start_handle;
  service_list_entry->declaration.uuid_length  = BLE_GATT_UUID_LENGTH;
  service_list_entry->declaration.uuid[0]      = (BLE_GATT_PRI_SERVICE & 0xff);
  service_list_entry->declaration.uuid[1]      = ((BLE_GATT_PRI_SERVICE & 0xff00) >> 8);
  service_list_entry->declaration.value_length = read_group->length;
  service_list_entry->declaration.value        = malloc (read_group->length);
  memcpy (service_list_entry->declaration.value, read_group->data, read_group->length);

  list_add ((list_entry_t **)(&(device->service_list)), (list_entry_t *)service_list_entry);
}

void ble_event_find_information (ble_event_find_information_t *find_information)
{
  ble_service_list_entry_t *service = connection_params.service;
  
  printf ("BLE Find information event\n");

  if (find_information->length == BLE_GATT_UUID_LENGTH)
  {
    uint16 uuid = ((find_information->data[1]) << 8) | (find_information->data[0]);
    
    if (uuid == BLE_GATT_INCLUDE)
    {
      /* TODO: Include service declaration */
      ble_service_list_entry_t *include_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*include_list_entry));
      
      include_list_entry->declaration.handle      = find_information->char_handle;
      include_list_entry->declaration.uuid_length = BLE_GATT_UUID_LENGTH;
      include_list_entry->declaration.uuid[0]     = (BLE_GATT_INCLUDE & 0xff);
      include_list_entry->declaration.uuid[1]     = ((BLE_GATT_INCLUDE & 0xff00) >> 8);
  
      list_add ((list_entry_t **)(&(service->include_list)), (list_entry_t *)include_list_entry);  
    }
    else if ((uuid == BLE_GATT_CHAR_DECL)           ||
             (uuid == BLE_GATT_CHAR_USER_DESC)      ||
             (uuid == BLE_GATT_CHAR_FORMAT)         ||
             (uuid == BLE_GATT_CHAR_CLIENT_CONFIG))
    {
      ble_attr_list_entry_t *desc_list_entry = (ble_attr_list_entry_t *)malloc (sizeof (*desc_list_entry));
      ble_char_list_entry_t *char_list_entry;

      if (uuid == BLE_GATT_CHAR_DECL)
      {
        char_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*char_list_entry));
        char_list_entry->desc_list = NULL;
        list_add ((list_entry_t **)(&(service->char_list)), (list_entry_t *)char_list_entry);
      }
      else
      {
        char_list_entry = (ble_char_list_entry_t *)list_tail ((list_entry_t **)(&(service->char_list)));
      }
  
      desc_list_entry->handle       = find_information->char_handle;
      desc_list_entry->uuid_length  = find_information->length;
      memcpy (desc_list_entry->uuid, find_information->data, find_information->length);
      desc_list_entry->value_length = 0;
      desc_list_entry->value        = NULL;
      list_add ((list_entry_t **)(&(char_list_entry->desc_list)), (list_entry_t *)desc_list_entry);
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

int32 ble_event_attr_value (ble_event_attr_value_t *attr_value)
{
  int32 status = 1;
  ble_attr_list_entry_t *attribute = NULL;

  printf ("BLE Attribute value event, type %d, handle 0x%04x\n", attr_value->type, attr_value->attr_handle);

  if ((attr_value->type == BLE_ATTR_VALUE_NOTIFY) ||
      (attr_value->type == BLE_ATTR_VALUE_INDICATE))
  {
    ble_char_list_entry_t *update_list = connection_params.device->info.update_list;

    while (update_list != NULL)
    {
      attribute = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(update_list->desc_list)));
      
      if ((attribute->handle == attr_value->attr_handle) && (update_list->update.timer == 0))
      {
        update_list->update.timer = -1;

        if (update_list->update.init > 0)
        {
          update_list->update.init             = 0;
          update_list->update.expected_time    = (clock_current_time ()) - 1000;
          update_list->update.timer_correction = 0;
        }
        else
        {
          update_list->update.timer_correction = (clock_current_time ()) - update_list->update.expected_time;
        }

        break;
      }
      else
      {
        attribute = NULL;
      }
        
      update_list = update_list->next;
    }
  }
  else
  {
    attribute = connection_params.attribute;
  }

  if (attribute != NULL)
  {
    if ((attr_value->type == BLE_ATTR_VALUE_READ)      ||
        (attr_value->type == BLE_ATTR_VALUE_NOTIFY)    ||
        (attr_value->type == BLE_ATTR_VALUE_INDICATE)  ||
        (attr_value->type == BLE_ATTR_VALUE_READ_TYPE) ||
        ((attr_value->type == BLE_ATTR_VALUE_READ_BLOB) && (attribute->value_length == 0)))
    {
      if (attribute->value == NULL)
      {
        attribute->value = malloc (attr_value->length);
      }
      memcpy (attribute->value, attr_value->data, attr_value->length);
      attribute->value_length = attr_value->length;
    }
    else
    {
      attribute->value = realloc (attribute->value, (attribute->value_length + attr_value->length));
      memcpy ((attribute->value + attribute->value_length), attr_value->data, attr_value->length);
      attribute->value_length += attr_value->length;
    }

    if (((attr_value->type == BLE_ATTR_VALUE_NOTIFY)    ||
         (attr_value->type == BLE_ATTR_VALUE_INDICATE)) &&
        ((ble_wait_data ()) < 0))
    {
      status = ble_connect_disconnect ();
    }
  }
  else
  {
    printf ("Attribute value handle %04x not expected\n", attr_value->attr_handle);
  }

  return status;
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
  ble_device_list_entry_t *device_list = ble_device_list;

  while (device_list != NULL)
  {
    if (device_list->info.status == BLE_DEVICE_DISCOVER)
    {
      found++;
    }
    
    device_list = device_list->next;
  }
  
  return ((ble_device_list != NULL) ? found : 1);
}

int32 ble_check_profile_list (void)
{
  int32 found = 0;
  ble_device_list_entry_t *device_list = ble_device_list;

  while (device_list != NULL)
  {
    if (device_list->info.status == BLE_DEVICE_DISCOVER_SERVICE)
    {
      found++;
    }
    device_list = device_list->next;
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

void ble_flush_message_list (void)
{
  while (timer_expiry_list != NULL)
  {
    timer_list_entry_t *timer_list_entry = timer_expiry_list;
    list_remove ((list_entry_t **)(&timer_expiry_list), (list_entry_t *)timer_list_entry);
    free (timer_list_entry);
  }

  while (ble_message_list != NULL)
  {
    ble_message_list_entry_t *message_list_entry = ble_message_list;
    list_remove ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
    ble_print_message (&(message_list_entry->message));
    free (message_list_entry);
  }  
}

int32 ble_start_scan (void)
{
  int32 status;
  ble_message_t message;
  ble_command_scan_params_t *scan_params;

  ble_update_timer ();

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
      ((timer_start (BLE_SCAN_DURATION, BLE_TIMER_SCAN_STOP,
                     ble_callback_timer, &(connection_params.timer_info))) != 0))
  {
    status = -1;
  }

  return status;
}

int32 ble_stop_scan (void)
{
  ble_update_timer ();
  connection_params.timer_info = NULL;
  
  return ble_end_procedure ();
}

int32 ble_start_profile (void)
{
  int32 status;

  ble_update_timer ();

  connection_params.device = ble_device_list;
  while ((connection_params.device != NULL) &&
         (connection_params.device->info.status != BLE_DEVICE_DISCOVER_SERVICE))
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

int32 ble_next_profile (void)
{
  int32 status;
  ble_device_list_entry_t *device_list_entry = connection_params.device;

  connection_params.device = connection_params.device->next;
  while ((connection_params.device != NULL) &&
         (connection_params.device->info.status != BLE_DEVICE_DISCOVER_SERVICE))
  {
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    status = ble_connect_direct ();
  }
  else
  {
    ble_update_timer ();
    status = 0;
  }

  if (device_list_entry->info.status == BLE_DEVICE_DISCOVER)
  {
    list_remove ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);
    free (device_list_entry);
  }
  else if (device_list_entry->info.status == BLE_DEVICE_IGNORE)
  {
    list_remove ((list_entry_t **)(&ble_device_list), (list_entry_t *)device_list_entry);
    list_add ((list_entry_t **)(&ble_ignore_list), (list_entry_t *)device_list_entry);
  }
  
  return status;
}

int32 ble_read_profile (void)
{
  int32 status;
  ble_device_t *device = &(connection_params.device->info);

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
      ble_free_service_list ();
      status = ble_connect_disconnect ();
    }
  }
  else if (device->status == BLE_DEVICE_DISCOVER_CHAR)
  {
    if (connection_params.attribute == NULL)
    {
      connection_params.service         = device->service_list;
      connection_params.characteristics = connection_params.service->char_list;
      connection_params.attribute       = connection_params.characteristics->desc_list;
    }
    else
    {
      if (connection_params.attribute->next == NULL)
      {
        if (connection_params.characteristics->next == NULL)
        {
          connection_params.service         = connection_params.service->next;
          connection_params.characteristics = connection_params.service->char_list;
          connection_params.attribute       = connection_params.characteristics->desc_list;
        }
        else
        {
          connection_params.characteristics = connection_params.characteristics->next;
          connection_params.attribute       = connection_params.characteristics->desc_list;
        }        
      }
      else
      {
        connection_params.attribute = connection_params.attribute->next;
      }
    }

    status = ble_read_long_handle ();
    if (status > 0)
    {
      if ((connection_params.attribute->next == NULL)       &&
          (connection_params.characteristics->next == NULL) &&
          (connection_params.service->next == NULL))
      {
        device->status = BLE_DEVICE_CONFIGURE_CHAR;
      }
    }
    else
    {
      device->status = BLE_DEVICE_DISCOVER_SERVICE;
      ble_free_service_list ();
      status = ble_connect_disconnect ();
    }
  }
  else
  {
    ble_print_service_list ();
    ble_update_data_list ();

    if (device->update_list != NULL)
    {
      int32 wakeup_time;
      ble_char_list_entry_t *update_list;

      wakeup_time = ble_get_wakeup ();
      update_list = device->update_list;

      if (wakeup_time <= (clock_current_time ()))
      {
        wakeup_time += 20000;
      }
      else
      {
        wakeup_time -= 10000;
      }

      while (update_list != NULL)
      {
        update_list->update.expected_time = wakeup_time;
        update_list = update_list->next;
      }

      device->id     = ble_identify_device (device->address.byte, device->update_list);
      device->status = BLE_DEVICE_DATA;
    }
    else
    {
      /* ble_free_service_list (); */
      device->status = BLE_DEVICE_IGNORE;
    }

    status = ble_connect_disconnect ();
  }

  return status;
}

int32 ble_start_data (void)
{
  int32 status;

  ble_update_timer ();

  connection_params.device = ble_device_list;
  while (connection_params.device != NULL)
  {
    ble_char_list_entry_t *update_list = NULL;
    
    if (connection_params.device->info.status == BLE_DEVICE_DATA)
    {
      update_list = connection_params.device->info.update_list;
      while ((update_list != NULL) && (update_list->update.timer > 0))
      {
        update_list = update_list->next;
      }
    }

    if (update_list != NULL)
    {
      break;
    }
    
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

int32 ble_next_data (void)
{
  int32 status;

  ble_callback_data ();
  ble_update_timer ();

  connection_params.device = connection_params.device->next;
  while (connection_params.device != NULL)
  {
    ble_char_list_entry_t *update_list = NULL;
    
    if (connection_params.device->info.status == BLE_DEVICE_DATA)
    {
      update_list = connection_params.device->info.update_list;
      while ((update_list != NULL) && (update_list->update.timer > 0))
      {
        update_list = update_list->next;
      }
    }

    if (update_list != NULL)
    {
      break;
    }
    
    connection_params.device = connection_params.device->next;
  }

  if (connection_params.device != NULL)
  {
    status = ble_connect_direct ();
  }
  else
  {
    int32 current_time = clock_current_time ();

    if ((current_time - ble_init_time) > (24*60*60*1000))
    {
      ble_deinit ();
      (void)ble_init ();
    }
    
    status = 0;
  }
  
  return status;
}

int32 ble_update_data (void)
{
  int32 status;

  if (connection_params.handle != 0xff)
  {
    ble_device_t *device = &(connection_params.device->info);

    if ((connection_params.characteristics != NULL) &&
        ((connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_READ) ||
         (connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_WRITE)))
    {
      connection_params.characteristics->update.timer = -1;

      if (connection_params.characteristics->update.init > 0)
      {
        connection_params.characteristics->update.init             = 0;
        connection_params.characteristics->update.expected_time    = (clock_current_time ()) - 1000;
        connection_params.characteristics->update.timer_correction = 0;
      }
      else
      {
        connection_params.characteristics->update.timer_correction
          = (clock_current_time ()) - connection_params.characteristics->update.expected_time;
      }
    }
  
    if (connection_params.characteristics == NULL)
    {
      connection_params.characteristics = device->update_list;
    }
    else
    {
      connection_params.characteristics = connection_params.characteristics->next;
    }
  
    while ((connection_params.characteristics != NULL) &&
           (connection_params.characteristics->update.timer > 0))
    {
      connection_params.characteristics = connection_params.characteristics->next;
    }
  
    if (connection_params.characteristics != NULL)
    {
      connection_params.attribute
          = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(connection_params.characteristics->desc_list)));
  
      if (connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_READ)
      {
        status = ble_read_long_handle ();
      }
      else if ((connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_INDICATE) ||
               (connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_NOTIFY)   ||
               (connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_WRITE))
      {
        if ((connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_INDICATE) ||
            (connection_params.characteristics->update.type == BLE_CHAR_PROPERTY_NOTIFY))
        {
          connection_params.attribute = connection_params.characteristics->desc_list;
          
          while (connection_params.attribute != NULL)
          {
            uint16 uuid = ((connection_params.attribute->uuid[1]) << 8) | connection_params.attribute->uuid[0];
            if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
            {
              break;
            }
            
            connection_params.attribute = connection_params.attribute->next;
          }
        }
  
        status = ble_write_handle ();
      }
      else
      {
        ble_message_list_entry_t *message_list_entry
            = (ble_message_list_entry_t *)malloc (sizeof (*message_list_entry));
  
        printf ("BLE Update data characteristics property %d not supported\n",
                                                          connection_params.characteristics->update.type);
  
        message_list_entry->message.header.type    = BLE_EVENT;
        message_list_entry->message.header.length  = 5;
        message_list_entry->message.header.class   = BLE_CLASS_ATTR_CLIENT;
        message_list_entry->message.header.command = BLE_EVENT_PROCEDURE_COMPLETED;
  
        list_add ((list_entry_t **)(&ble_message_list), (list_entry_t *)message_list_entry);
  
        status = 1;
      }
    }
    else if ((ble_wait_data ()) < 0)
    {
      status = ble_connect_disconnect ();
    }
    else
    {
      status = 1;
    }
  }
  else
  {
    status = 0;
  }
    
  return status;
}

int32 ble_get_sleep (void)
{
  int32 min_sleep_interval;
  ble_device_list_entry_t *device_list;
  
  min_sleep_interval = 0x7fffffff;
  device_list        = ble_device_list;

  /* Loop through update list to find the minimum sleep interval */
  while (device_list != NULL)
  {
    if ((device_list->info.status == BLE_DEVICE_DATA)  ||
        (device_list->info.status == BLE_DEVICE_DISCOVER))
    {
      ble_char_list_entry_t *update_list = device_list->info.update_list;

      while (update_list != NULL)
      {
        min_sleep_interval = (min_sleep_interval > update_list->update.timer)
                              ? update_list->update.timer : min_sleep_interval;
          
        update_list = update_list->next;
      }
    }
    
    device_list = device_list->next;
  }

  /* Subtract the sleep interval from timer. All characteristics
     with timer value 0 will be updated on wake-up */
  if ((min_sleep_interval == 0x7fffffff) || (min_sleep_interval == 0))
  {
    min_sleep_interval = 10;
  }
  
  printf ("BLE Sleep interval %d millisec\n", min_sleep_interval);

  return min_sleep_interval;
}

