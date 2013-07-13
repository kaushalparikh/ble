
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "sync.h"
#include "device.h"

/* Database declaration/definition */
enum
{
  DB_DEVICE_LIST_TABLE = 0,
  DB_NUM_STATIC_TABLES
};

enum
{
  DB_DEVICE_TABLE_COLUMN_NO = 0,
  DB_DEVICE_TABLE_COLUMN_ADDRESS,
  DB_DEVICE_TABLE_COLUMN_NAME,
  DB_DEVICE_TABLE_COLUMN_SERVICE,
  DB_DEVICE_TABLE_COLUMN_INTERVAL,
  DB_DEVICE_TABLE_COLUMN_STATUS,
  DB_DEVICE_TABLE_NUM_COLUMNS
};

static db_column_entry_t db_device_table_columns[DB_DEVICE_TABLE_NUM_COLUMNS] = 
{
  {"No.",      DB_DEVICE_TABLE_COLUMN_NO,       DB_COLUMN_TYPE_INT,
    DB_COLUMN_FLAG_PRIMARY_KEY,                                                          NULL},
  {"Address",  DB_DEVICE_TABLE_COLUMN_ADDRESS,  DB_COLUMN_TYPE_BLOB,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_UPDATE_KEY),                               NULL},
  {"Name",     DB_DEVICE_TABLE_COLUMN_NAME,     DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
  {"Service",  DB_DEVICE_TABLE_COLUMN_SERVICE,  DB_COLUMN_TYPE_BLOB,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_UPDATE_KEY),                               NULL},
  {"Interval", DB_DEVICE_TABLE_COLUMN_INTERVAL, DB_COLUMN_TYPE_INT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
  {"Status",   DB_DEVICE_TABLE_COLUMN_STATUS,   DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA | DB_COLUMN_FLAG_UPDATE_VALUE), NULL}
};

static db_table_list_entry_t db_static_tables[DB_NUM_STATIC_TABLES] =
{
  {NULL, "Device List", DB_DEVICE_TABLE_NUM_COLUMNS, db_device_table_columns, NULL, NULL, NULL}
};

static db_info_t *db_info = NULL;

LIST_HEAD_INIT (ble_sync_list_entry_t, sync_list);


void ble_sync_device (ble_sync_list_entry_t *sync_list_entry)
{
  ble_sync_list_entry_t *temp_list_entry;

  if (sync_list_entry->type == BLE_SYNC_PULL)
  {
    temp_list_entry  = (ble_sync_list_entry_t *)malloc (sizeof (*temp_list_entry));
    *temp_list_entry = *sync_list_entry;
    list_add ((list_entry_t **)(&sync_list), (list_entry_t *)temp_list_entry);
  }
  else
  {
    temp_list_entry = sync_list;

    while  (temp_list_entry != NULL)
    {
      if (temp_list_entry->type == BLE_SYNC_PUSH)
      {
        list_remove ((list_entry_t **)(&sync_list), (list_entry_t *)temp_list_entry);
        sync_list_entry->data = temp_list_entry->data;
        free (temp_list_entry);

        break;
      }

      temp_list_entry = temp_list_entry->next;
    }
  }
}

void ble_print_device (ble_device_list_entry_t *device_list_entry)
{
  int32 i;
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

  printf ("     Name: %s\n", device_list_entry->name);
  printf ("  Address: 0x");
  for (i = 0; i < BLE_DEVICE_ADDRESS_LENGTH; i++)
  {
    printf ("%02x", device_list_entry->address.byte[i]);
  }
  printf (" (%s)\n", ((device_list_entry->address.type > 0) ? "random" : "public"));

  printf ("  Service: ");
  while (service_list_entry != NULL)
  {
    printf ("0x");
    for (i = 0; i < service_list_entry->declaration->data_length; i++)
    {
      printf ("%02x", service_list_entry->declaration->data[i]);
    }

    if (service_list_entry->next != NULL)
    {
      printf (", ");
    }
    
    service_list_entry = service_list_entry->next;
  }
  printf ("\n");
}

void ble_update_device (ble_device_list_entry_t *device_list_entry)
{
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

  while (service_list_entry != NULL)
  {
    db_column_value_t column_value;
    ble_sync_list_entry_t *sync_list_entry;
    ble_sync_device_data_t *sync_device_data;
 
    column_value.blob.data   = device_list_entry->address.byte;
    column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
    column_value.blob.data   = service_list_entry->declaration->data;
    column_value.blob.length = service_list_entry->declaration->data_length;
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
    
    if (device_list_entry->status == BLE_DEVICE_DATA)
    {
      if (service_list_entry->update.char_list != NULL)
      {
        column_value.text = "Active";
      }
      else
      {
        column_value.text = "Ignored";
      }
    }
    else
    {
      column_value.text = "Inactive";
    }
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
    
    db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0);

    sync_list_entry      = (ble_sync_list_entry_t *)malloc (sizeof (*sync_list_entry));
    sync_list_entry->type = BLE_SYNC_PUSH;
    sync_list_entry->data = malloc (sizeof (ble_sync_device_data_t));
    list_add ((list_entry_t **)(&sync_list), (list_entry_t *)sync_list_entry);
    
    sync_device_data           = (ble_sync_device_data_t *)(sync_list_entry->data);
    memcpy (sync_device_data->address, device_list_entry->address.byte, BLE_DEVICE_ADDRESS_LENGTH);
    sync_device_data->name     = strdup (device_list_entry->name);
    memcpy (sync_device_data->service, service_list_entry->declaration->data, service_list_entry->declaration->data_length);
    sync_device_data->interval = service_list_entry->update.interval;
    sync_device_data->status   = strdup (column_value.text);

    service_list_entry = service_list_entry->next;
  }
}

ble_device_list_entry_t * ble_find_device (ble_device_list_entry_t *device_list_entry,
                                           ble_device_address_t *address)
{
  while (device_list_entry != NULL)
  {
    if ((memcmp (address, &(device_list_entry->address), sizeof (*address))) == 0)
    {
      break;
    }
    
    device_list_entry = device_list_entry->next;
  }

  return device_list_entry;
}

void ble_get_device (ble_device_list_entry_t **device_list)
{
  int32 status = 1;
  
  if (db_info == NULL)
  {
    status = db_open ("gateway.db", &db_info);
    if (status > 0)
    {
      status = db_create_table (db_info, &(db_static_tables[DB_DEVICE_LIST_TABLE]));
    }
  }

  if (status > 0)
  {
    ble_device_list_entry_t *device_list_entry;
    ble_sync_list_entry_t *sync_list_entry = sync_list;
 
    while (sync_list_entry != NULL)
    {
      ble_sync_list_entry_t *sync_list_entry_del = NULL;

      if (sync_list_entry->type == BLE_SYNC_PULL)
      {
        uint8 write_type = DB_WRITE_UPDATE;
        ble_sync_device_data_t *sync_device_data = (ble_sync_device_data_t *)(sync_list_entry->data);
        ble_device_address_t address;
        ble_service_list_entry_t *service_list_entry;
        db_column_value_t column_value;

        memcpy (address.byte, sync_device_data->address, BLE_DEVICE_ADDRESS_LENGTH);
        address.type = BLE_ADDR_PUBLIC;
        
        device_list_entry = ble_find_device (*device_list, &address);
        if (device_list_entry == NULL)
        {
          device_list_entry = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));
          
          device_list_entry->address      = address;
          device_list_entry->service_list = NULL;
          device_list_entry->status       = BLE_DEVICE_DISCOVER;
          device_list_entry->data         = NULL;
          device_list_entry->name         = strdup (sync_device_data->name);
          
          list_add ((list_entry_t **)(&device_list), (list_entry_t *)device_list_entry);
          write_type = DB_WRITE_INSERT;
        }

        service_list_entry = ble_find_service (device_list_entry->service_list,
                                               sync_device_data->service, BLE_GATT_UUID_LENGTH);
        if (service_list_entry == NULL)
        {
          service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
        
          service_list_entry->declaration               = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
          service_list_entry->declaration->type         = 0;
          service_list_entry->declaration->handle       = BLE_INVALID_GATT_HANDLE;
          service_list_entry->declaration->uuid_length  = 0;
          service_list_entry->declaration->data_length  = BLE_GATT_UUID_LENGTH;
          service_list_entry->declaration->data         = malloc (service_list_entry->declaration->data_length);
          memcpy (service_list_entry->declaration->data, sync_device_data->service,
                  service_list_entry->declaration->data_length);

          service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
          service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
          service_list_entry->include_list = NULL;
          service_list_entry->char_list    = NULL;

          service_list_entry->update.char_list   = NULL;
          service_list_entry->update.init        = 1;
          service_list_entry->update.time        = 0;
          service_list_entry->update.time_offset = 0;
          service_list_entry->update.wait        = 0;
          service_list_entry->update.interval    = (sync_device_data->interval * 60 * 1000);

          list_add ((list_entry_t **)(&(device_list_entry->service_list)), (list_entry_t *)service_list_entry);  
          write_type = DB_WRITE_INSERT;
        }

        column_value.blob.data   = device_list_entry->address.byte;
        column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
        column_value.blob.data   = service_list_entry->declaration->data;
        column_value.blob.length = service_list_entry->declaration->data_length;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
        column_value.text = sync_device_data->status;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);

        if (write_type == DB_WRITE_INSERT)
        {
          column_value.text = sync_device_data->name;
          db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_NAME, &column_value);
          column_value.integer = sync_device_data->interval;
          db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);
        }

        db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type);

        free (sync_device_data->name);
        free (sync_device_data->status);
        sync_list_entry_del = sync_list_entry;
      }

      sync_list_entry = sync_list_entry->next;
      
      if (sync_list_entry_del != NULL)
      {
        free (sync_list_entry_del->data);
        list_remove ((list_entry_t **)(&sync_list), (list_entry_t *)sync_list_entry_del);
        free (sync_list_entry_del);
        sync_list_entry_del = NULL;
      }
    }

    printf ("BLE device list -- \n");
    device_list_entry = *device_list;
    if (device_list_entry != NULL)
    {
      while (device_list_entry != NULL)
      {
        ble_print_device (device_list_entry);
        device_list_entry = device_list_entry->next;
      }
    }
    else
    {
      printf (" No devices\n");
    }
  }
}

