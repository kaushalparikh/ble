
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "device.h"

/* Database declaration/definition */
enum
{
  DB_DEVICE_LIST_TABLE = 0,
  DB_NUM_STATIC_TABLES = 1
};

enum
{
  DB_DEVICE_TABLE_COLUMN_NO       = 0,
  DB_DEVICE_TABLE_COLUMN_ADDRESS  = 1,
  DB_DEVICE_TABLE_COLUMN_NAME     = 2,
  DB_DEVICE_TABLE_COLUMN_SERVICE  = 3,
  DB_DEVICE_TABLE_COLUMN_INTERVAL = 4,
  DB_DEVICE_TABLE_COLUMN_STATUS   = 5,
  DB_DEVICE_TABLE_NUM_COLUMNS     = 6
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
    
    while ((status = db_read_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]))) > 0)
    {
      db_column_value_t column_value;
      ble_service_list_entry_t *service_list_entry;
      ble_device_address_t address;

      db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
      memcpy (address.byte, column_value.blob.data, BLE_DEVICE_ADDRESS_LENGTH);
      address.type = BLE_ADDR_PUBLIC;
      device_list_entry = ble_find_device (*device_list, &address);
      
      if (device_list_entry == NULL)
      {
        device_list_entry = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));
        
        device_list_entry->address      = address;
        device_list_entry->service_list = NULL;
        device_list_entry->status       = BLE_DEVICE_DISCOVER;
        device_list_entry->data         = NULL;

        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_NAME, &column_value);
        device_list_entry->name = strdup (column_value.text);
        
        list_add ((list_entry_t **)(&device_list), (list_entry_t *)device_list_entry);
      }

      db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
      service_list_entry = ble_find_service (device_list_entry->service_list, column_value.blob.data, column_value.blob.length);
      
      if (service_list_entry == NULL)
      {
        service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));

        service_list_entry->declaration               = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        service_list_entry->declaration->type         = 0;
        service_list_entry->declaration->handle       = BLE_INVALID_GATT_HANDLE;
        service_list_entry->declaration->uuid_length  = 0;
        service_list_entry->declaration->data_length  = column_value.blob.length;
        service_list_entry->declaration->data         = malloc (column_value.blob.length);
        memcpy (service_list_entry->declaration->data, column_value.blob.data, column_value.blob.length);

        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->include_list = NULL;
        service_list_entry->char_list    = NULL;

        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);
        service_list_entry->update.char_list   = NULL;
        service_list_entry->update.init        = 1;
        service_list_entry->update.time        = 0;
        service_list_entry->update.time_offset = 0;
        service_list_entry->update.wait        = 0;
        service_list_entry->update.interval    = (column_value.integer * 60 * 1000);

        list_add ((list_entry_t **)(&(device_list_entry->service_list)), (list_entry_t *)service_list_entry);  

        column_value.blob.data   = device_list_entry->address.byte;
        column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
        column_value.blob.data   = service_list_entry->declaration->data;
        column_value.blob.length = service_list_entry->declaration->data_length;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
        column_value.text = "Searching";
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
        db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0);
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

