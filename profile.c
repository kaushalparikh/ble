
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "temperature.h"

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


void ble_update_char_type (ble_char_list_entry_t * char_list_entry, uint8 type)
{
  ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
  
  if (type == BLE_ATTR_TYPE_READ)
  {
    if (char_decl->type & BLE_ATTR_TYPE_READ)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_READ;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_INDICATE)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_INDICATE;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_NOTIFY)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_NOTIFY;
    }
    else
    {
      char_list_entry->value->type = 0;
    }
  }
  else
  {
    if (char_decl->type & BLE_ATTR_TYPE_WRITE)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_WRITE_NO_RSP)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE_NO_RSP;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_WRITE_SIGNED)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE_SIGNED;
    }
    else
    {
      char_list_entry->value->type = 0;
    }
  }

  if (char_list_entry->value->type & (BLE_ATTR_TYPE_INDICATE | BLE_ATTR_TYPE_NOTIFY))
  {
    ((ble_char_client_config_t *)(char_list_entry->client_config->data))->bitfield
      = (char_list_entry->value->type & BLE_ATTR_TYPE_INDICATE)
        ? BLE_CHAR_CLIENT_INDICATE : BLE_CHAR_CLIENT_NOTIFY;
  }
}

ble_attribute_t * ble_find_attribute (ble_service_list_entry_t *service_list_entry,
                                      uint16 handle)
{
  ble_attribute_t *attribute = NULL;
  
  while ((service_list_entry != NULL) && (attribute == NULL))
  {
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

    while ((char_list_entry != NULL) && (attribute == NULL))
    {
      if ((char_list_entry->declaration != NULL) && (char_list_entry->declaration->handle == handle))
      {
        attribute = char_list_entry->declaration;
      }
      else if ((char_list_entry->value != NULL) && (char_list_entry->value->handle == handle))
      {
        attribute = char_list_entry->value;
      }
      else if ((char_list_entry->description != NULL) && (char_list_entry->description->handle == handle))
      {
        attribute = char_list_entry->description;
      }        
      else if ((char_list_entry->client_config != NULL) && (char_list_entry->client_config->handle == handle))
      {
        attribute = char_list_entry->client_config;
      }
      else if ((char_list_entry->format != NULL) && (char_list_entry->format->handle == handle))
      {
        attribute = char_list_entry->format;
      }

      char_list_entry = char_list_entry->next;
    }

    char_list_entry = service_list_entry->update.char_list;

    while ((char_list_entry != NULL) && (attribute == NULL))
    {
      if ((char_list_entry->declaration != NULL) && (char_list_entry->declaration->handle == handle))
      {
        attribute = char_list_entry->declaration;
      }
      else if ((char_list_entry->value != NULL) && (char_list_entry->value->handle == handle))
      {
        attribute = char_list_entry->value;
      }
      else if ((char_list_entry->description != NULL) && (char_list_entry->description->handle == handle))
      {
        attribute = char_list_entry->description;
      }        
      else if ((char_list_entry->client_config != NULL) && (char_list_entry->client_config->handle == handle))
      {
        attribute = char_list_entry->client_config;
      }
      else if ((char_list_entry->format != NULL) && (char_list_entry->format->handle == handle))
      {
        attribute = char_list_entry->format;
      }        
      
      char_list_entry = char_list_entry->next;
    }
    
    service_list_entry = service_list_entry->next;
  }

  return attribute;
}

void ble_print_service (ble_service_list_entry_t *service_list_entry)
{
  while (service_list_entry != NULL)
  {
    int32 i;
    uint16 uuid = BLE_PACK_GATT_UUID (service_list_entry->declaration->uuid);
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
      
    printf ("Service -- %s\n", ((uuid == BLE_GATT_PRI_SERVICE) ? "primary" : "secondary"));
    printf ("  handle range: 0x%04x to 0x%04x\n", service_list_entry->start_handle, service_list_entry->end_handle);
    printf ("          uuid: 0x");
    for (i = (service_list_entry->declaration->data_length - 1); i >= 0; i--)
    {
      printf ("%02x", service_list_entry->declaration->data[i]);
    }
    printf ("\n");

    while (char_list_entry != NULL)
    {
      if (char_list_entry->declaration != NULL)
      {
        ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->declaration->uuid);

        printf ("    Characteristics -- declaration\n");
        printf ("      handle: 0x%04x\n", char_list_entry->declaration->handle);
        printf ("       value: ");
        printf ("0x%02x, 0x%04x, 0x", char_decl->type, char_decl->handle);
        for (i = (char_list_entry->declaration->data_length - 4); i >= 0; i--)
        {
          printf ("%02x", char_decl->uuid[i]);
        }
        printf ("\n");        
      }

      if (char_list_entry->description != NULL)
      {
        uuid = BLE_PACK_GATT_UUID (char_list_entry->description->uuid);

        printf ("        Descriptor -- description\n");
        printf ("          handle: 0x%04x\n", char_list_entry->description->handle);
        printf ("           value: ");
        for (i = 0; i < char_list_entry->description->data_length; i++)
        {
          printf ("%c", (int8)(char_list_entry->description->data[i]));
        }
        printf ("\n");       
      }

      if (char_list_entry->client_config != NULL)
      {
        ble_char_client_config_t *client_config = (ble_char_client_config_t *)(char_list_entry->client_config->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->client_config->uuid);

        printf ("        Descriptor -- client configuration\n");
        printf ("          handle: 0x%04x\n", char_list_entry->client_config->handle);
        printf ("           value: ");
        printf ("0x%04x\n", client_config->bitfield);      
      }

      if (char_list_entry->format != NULL)
      {
        ble_char_format_t *format = (ble_char_format_t *)(char_list_entry->format->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->client_config->uuid);
        
        printf ("        Descriptor -- format\n");
        printf ("          handle: 0x%04x\n", char_list_entry->format->handle);
        printf ("           value: ");
        printf ("0x%02x, %d\n", format->bitfield, format->exponent);
      }
      
      char_list_entry = char_list_entry->next;
    }
    
    service_list_entry = service_list_entry->next;
  }
}

void ble_update_service (ble_service_list_entry_t *service_list_entry, int8 *device_name,
                         void *device_data)
{
  while (service_list_entry != NULL)
  {
    if ((service_list_entry->update.char_list != NULL) &&
        (service_list_entry->update.wait <= 0))
    {
      uint8 uuid_length;
      uint16 uuid;
  
      uuid_length = service_list_entry->declaration->data_length;
      uuid        = BLE_PACK_GATT_UUID (service_list_entry->declaration->data);
  
      if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_TEMPERATURE_SERVICE_UUID))
      {
        ble_update_temperature (service_list_entry, device_name, device_data);
      }
    }

    service_list_entry = service_list_entry->next;
  }
}

int32 ble_init_service (ble_service_list_entry_t *service_list_entry, ble_device_address_t *device_address,
                        int8 *device_name, void **device_data)
{
  int32 status = 0;
  
  while (service_list_entry != NULL)
  {
    int32 found = 0;
    uint8 uuid_length;
    uint16 uuid;

    uuid_length = service_list_entry->declaration->data_length;
    uuid        = BLE_PACK_GATT_UUID (service_list_entry->declaration->data);

    if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_TEMPERATURE_SERVICE_UUID))
    {
      found = ble_init_temperature (service_list_entry, device_name, db_info, device_data);
    }

    if (found > 0)
    {
      db_column_value_t column_value;
 
      column_value.blob.data   = device_address->byte;
      column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_ADDRESS, &column_value);
      column_value.blob.data   = service_list_entry->declaration->data;
      column_value.blob.length = service_list_entry->declaration->data_length;
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_SERVICE, &column_value);
      column_value.text = "Active";
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, DB_DEVICE_TABLE_COLUMN_STATUS, &column_value);
      db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0);
    }
     
    status             += found;
    service_list_entry  = service_list_entry->next;
  }

  return status;  
}

ble_service_list_entry_t * ble_find_service (ble_service_list_entry_t *service_list_entry,
                                             uint8 *uuid, uint8 uuid_length)
{
  while (service_list_entry != NULL)
  {
    if ((memcmp (uuid, service_list_entry->declaration->data, uuid_length)) == 0)
    {
      break;
    }
    
    service_list_entry = service_list_entry->next;
  }

  return service_list_entry;  
}

void ble_clear_service (ble_service_list_entry_t *service_list_entry)
{
  while (service_list_entry != NULL)
  {
    while (service_list_entry->char_list != NULL)
    {
      ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

      if (char_list_entry->declaration != NULL)
      {
        if (char_list_entry->declaration->data != NULL)
        {
          free (char_list_entry->declaration->data);
        }
        free (char_list_entry->declaration);
      }
      if (char_list_entry->value != NULL)
      {
        if (char_list_entry->value->data != NULL)
        {
          free (char_list_entry->value->data);
        }
        free (char_list_entry->value);
      }
      if (char_list_entry->description != NULL)
      {
        if (char_list_entry->description->data != NULL)
        {
          free (char_list_entry->description->data);
        }
        free (char_list_entry->description);
      }
      if (char_list_entry->client_config != NULL)
      {
        if (char_list_entry->client_config->data != NULL)
        {
          free (char_list_entry->client_config->data);
        }
        free (char_list_entry->client_config);
      }
      if (char_list_entry->format != NULL)
      {
        if (char_list_entry->format->data != NULL)
        {
          free (char_list_entry->format->data);
        }
        free (char_list_entry->format);
      }
      
      list_remove ((list_entry_t **)(&(service_list_entry->char_list)), (list_entry_t *)char_list_entry);
      free (char_list_entry);   
    }

    service_list_entry = service_list_entry->next;
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

void ble_get_device_list (ble_device_list_entry_t **device_list)
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
        column_value.text = "Inactive";
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

