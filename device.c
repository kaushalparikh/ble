
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
  {"Address",  DB_DEVICE_TABLE_COLUMN_ADDRESS,  DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA | DB_COLUMN_FLAG_UPDATE_KEY),   NULL},
  {"Name",     DB_DEVICE_TABLE_COLUMN_NAME,     DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
  {"Service",  DB_DEVICE_TABLE_COLUMN_SERVICE,  DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA | DB_COLUMN_FLAG_UPDATE_KEY),   NULL},
  {"Interval", DB_DEVICE_TABLE_COLUMN_INTERVAL, DB_COLUMN_TYPE_INT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA | DB_COLUMN_FLAG_UPDATE_VALUE), NULL},
  {"Status",   DB_DEVICE_TABLE_COLUMN_STATUS,   DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA | DB_COLUMN_FLAG_UPDATE_VALUE), NULL}
};

static db_table_list_entry_t db_static_tables[DB_NUM_STATIC_TABLES] =
{
  {NULL, "Device List", DB_DEVICE_TABLE_NUM_COLUMNS, db_device_table_columns, NULL, NULL, NULL, NULL}
};

static db_info_t *db_info = NULL;


static void ble_print_device_list (ble_device_list_entry_t *device_list_entry)
{
  printf ("BLE device list -- \n");
  
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
    ble_sync_list_entry_t *sync_list_entry;
    ble_sync_device_data_t *sync_device_data;
    db_column_value_t column_value;

    sync_list_entry            = (ble_sync_list_entry_t *)malloc (sizeof (*sync_list_entry));
    sync_list_entry->type      = BLE_SYNC_PUSH;
    sync_list_entry->data_type = BLE_SYNC_DEVICE;
    sync_list_entry->data      = malloc (sizeof (ble_sync_device_data_t));

    sync_device_data       = (ble_sync_device_data_t *)(sync_list_entry->data);
    sync_device_data->name = strdup (device_list_entry->name);

    column_value.text = malloc ((2 * BLE_DEVICE_ADDRESS_LENGTH) + 1);
    bin_to_string (column_value.text, device_list_entry->address.byte, BLE_DEVICE_ADDRESS_LENGTH);
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
    sync_device_data->address = strdup (column_value.text);
    free (column_value.text);
    column_value.text = malloc ((2 * service_list_entry->declaration->data_length) + 1);
    bin_to_string (column_value.text, service_list_entry->declaration->data,
                   service_list_entry->declaration->data_length);
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
    sync_device_data->service = strdup (column_value.text);
    free (column_value.text);
    
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
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
    sync_device_data->status = strdup (column_value.text);

    column_value.integer = (service_list_entry->update.interval)/(60 * 1000);
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);    
    sync_device_data->interval = (service_list_entry->update.interval)/(60 * 1000);
    
    db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE);
    ble_sync_push (sync_list_entry);

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

void ble_init_device_list (ble_device_list_entry_t **device_list)
{  
  if (db_info == NULL)
  {
    int32 status;
    
    status = db_open ("gateway.db", &db_info);
    if (status > 0)
    {
      status = db_create_table (db_info, &(db_static_tables[DB_DEVICE_LIST_TABLE]));
    }

    if (status > 0)
    {
      while ((status = db_read_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]))) > 0)
      {
        ble_device_address_t address;
        ble_device_list_entry_t *device_list_entry;
        ble_service_list_entry_t *service_list_entry;
        db_column_value_t column_value;
  
        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
        string_to_bin (address.byte, column_value.text, (2 * BLE_DEVICE_ADDRESS_LENGTH));
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
          
          list_add ((list_entry_t **)(device_list), (list_entry_t *)device_list_entry);
        }
  
        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);

        service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
  
        service_list_entry->declaration = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        service_list_entry->declaration->type = 0;
        service_list_entry->declaration->handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->declaration->uuid_length = 0;
        service_list_entry->declaration->data_length = (strlen (column_value.text) + 1)/2;
        service_list_entry->declaration->data = malloc (service_list_entry->declaration->data_length);
        string_to_bin (service_list_entry->declaration->data, column_value.text,
                       (2 * service_list_entry->declaration->data_length));
  
        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->include_list = NULL;
        service_list_entry->char_list = NULL;
  
        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);
        service_list_entry->update.char_list = NULL;
        service_list_entry->update.init = 1;
        service_list_entry->update.time = 0;
        service_list_entry->update.time_offset = 0;
        service_list_entry->update.wait = 0;
        service_list_entry->update.interval = (column_value.integer * 60 * 1000);
  
        list_add ((list_entry_t **)(&(device_list_entry->service_list)), (list_entry_t *)service_list_entry);

        column_value.text = malloc ((2 * BLE_DEVICE_ADDRESS_LENGTH) + 1);
        bin_to_string (column_value.text, device_list_entry->address.byte, BLE_DEVICE_ADDRESS_LENGTH);
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
        free (column_value.text);
        column_value.text = malloc ((2 * service_list_entry->declaration->data_length) + 1);
        bin_to_string (column_value.text, service_list_entry->declaration->data,
                       service_list_entry->declaration->data_length);
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
        free (column_value.text);
        column_value.integer = (service_list_entry->update.interval)/(60 * 1000);
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);    
        column_value.text = "Searching";
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
        
        db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), DB_WRITE_UPDATE);        
      }        
    }
  }

  ble_print_device_list (*device_list);
}

void ble_update_device_list (ble_device_list_entry_t **device_list)
{
  ble_sync_list_entry_t *sync_list_entry = NULL;

  ble_sync_pull (&sync_list_entry, BLE_SYNC_DEVICE);
  
  while (sync_list_entry != NULL)
  {
    ble_sync_list_entry_t *sync_list_entry_del = sync_list_entry;
    ble_sync_device_data_t *sync_device_data = (ble_sync_device_data_t *)(sync_list_entry->data);
    ble_device_address_t address;
    ble_device_list_entry_t *device_list_entry;
    uint8 uuid[BLE_MAX_UUID_LENGTH];
    ble_service_list_entry_t *service_list_entry;
    db_column_value_t column_value;
    uint8 write_type = DB_WRITE_UPDATE;

    string_to_bin (address.byte, sync_device_data->address, (2 * BLE_DEVICE_ADDRESS_LENGTH));
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
      
      list_add ((list_entry_t **)(device_list), (list_entry_t *)device_list_entry);
    }

    string_to_bin (uuid, sync_device_data->service, (strlen (sync_device_data->service)));
    service_list_entry = ble_find_service (device_list_entry->service_list,
                                           uuid, (((strlen (sync_device_data->service)) + 1)/2));
    if (service_list_entry == NULL)
    {
      service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
    
      service_list_entry->declaration               = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
      service_list_entry->declaration->type         = 0;
      service_list_entry->declaration->handle       = BLE_INVALID_GATT_HANDLE;
      service_list_entry->declaration->uuid_length  = 0;
      service_list_entry->declaration->data_length = (strlen (sync_device_data->service) + 1)/2;
      service_list_entry->declaration->data = malloc (service_list_entry->declaration->data_length);
      string_to_bin (service_list_entry->declaration->data, sync_device_data->service,
                     (2 * service_list_entry->declaration->data_length));

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
    else if (strcmp (sync_device_data->status, "Delete") == 0)
    {
      ble_clear_characteristics (service_list_entry->char_list);
      ble_clear_characteristics (service_list_entry->update.char_list);
      free (service_list_entry->declaration);

      list_remove ((list_entry_t **)(&(device_list_entry->service_list)), (list_entry_t *)service_list_entry);
      write_type = DB_WRITE_DELETE;

      if (device_list_entry->service_list == NULL)
      {
        free (device_list_entry->name);
        list_remove (((list_entry_t **)device_list), (list_entry_t *)device_list_entry);
      }
    }

    column_value.text = malloc ((2 * BLE_DEVICE_ADDRESS_LENGTH) + 1);
    bin_to_string (column_value.text, device_list_entry->address.byte, BLE_DEVICE_ADDRESS_LENGTH);
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
    free (column_value.text);
    column_value.text = malloc ((2 * service_list_entry->declaration->data_length) + 1);
    bin_to_string (column_value.text, service_list_entry->declaration->data,
                   service_list_entry->declaration->data_length);
    db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
    free (column_value.text);

    if ((write_type == DB_WRITE_INSERT) || (write_type == DB_WRITE_UPDATE))
    {
      if (write_type == DB_WRITE_INSERT)
      {
        column_value.text = sync_device_data->name;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_NAME, &column_value);            
        column_value.text = "Searching";
      }
      else
      {
        column_value.text = sync_device_data->status;
      }
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
      column_value.integer = sync_device_data->interval;
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type, DB_DEVICE_TABLE_COLUMN_INTERVAL, &column_value);
    }
    else
    {
      free (service_list_entry);
      if (device_list_entry->service_list == NULL)
      {
        free (device_list_entry);
      }
    }
    
    db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), write_type);

    free (sync_device_data->address);
    free (sync_device_data->name);
    free (sync_device_data->service);
    free (sync_device_data->status);
    free (sync_list_entry->data);
    list_remove ((list_entry_t **)(&sync_list_entry), (list_entry_t *)sync_list_entry_del);
    free (sync_list_entry_del);
  }

  ble_print_device_list (*device_list);
}

