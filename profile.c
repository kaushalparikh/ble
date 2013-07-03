
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"

/* Time service definitions */
/* Invalid date */
#define BLE_INVALID_DATE_PARAMETER  (0)

typedef struct PACKED
{
  uint16 year;
  uint8  month;
  uint8  day;
  uint8  hour;
  uint8  minute;
  uint8  second;
} ble_char_time_t;

/* Measurement interval characteristics data length */
#define BLE_MEAS_INTERVAL_UUID         (0x2a21)
#define BLE_MEAS_INTERVAL_LENGTH       (2)
#define BLE_INVALID_MEAS_INTERVAL      (0x7fffffff)

/* Temperature service definitions */
/* Update interval in millisec */
#define BLE_TEMPERATURE_SERVICE_UUID   (0x1809)
#define BLE_TEMPERATURE_MEAS_UUID      (0x2a1c)
#define BLE_TEMPERATURE_TYPE_UUID      (0x2a1d)

#define BLE_TEMPERATURE_MEAS_INTERVAL      (10*60*1000)
#define BLE_MIN_TEMPERATURE_MEAS_INTERVAL  (1*60*1000)
#define BLE_MAX_TEMPERATURE_MEAS_INTERVAL  (30*60*1000)

typedef struct PACKED
{
  uint8           flags;
  float           meas_value;
  ble_char_time_t meas_time;
  uint8           type;
} ble_char_temperature_t;

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
    DB_COLUMN_FLAG_NOT_NULL,                                                             NULL},
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

enum
{
  DB_TEMPERATURE_TABLE_COLUMN_NO          = 0,
  DB_TEMPERATURE_TABLE_COLUMN_TIME        = 1,
  DB_TEMPERATURE_TABLE_COLUMN_TEMPERATURE = 2,
  DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL   = 3,
  DB_TEMPERATURE_TABLE_NUM_COLUMNS        = 4
};

static db_column_entry_t db_temperature_table_columns[DB_TEMPERATURE_TABLE_NUM_COLUMNS] =
{
  {"No.",               DB_TEMPERATURE_TABLE_COLUMN_NO,          DB_COLUMN_TYPE_INT,
    DB_COLUMN_FLAG_PRIMARY_KEY,                                                          NULL},
  {"Time",              DB_TEMPERATURE_TABLE_COLUMN_TIME,        DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
  {"Temperature (C)",   DB_TEMPERATURE_TABLE_COLUMN_TEMPERATURE, DB_COLUMN_TYPE_FLOAT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
  {"Battery Level (%)", DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL,   DB_COLUMN_TYPE_INT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),                               NULL},
};

static db_info_t db_info = 
{
  .file_name  = "gateway.db",
  .handle     = NULL,
  .table_list = NULL,
};


static void ble_update_char_type (ble_attr_list_entry_t * desc_list_entry, uint8 type)
{
  if (type == BLE_CHAR_TYPE_READ)
  {
    if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_READ)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_READ;
    }
    else if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_INDICATE)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_INDICATE;
    }
    else if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_NOTIFY)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_NOTIFY;
    }
    else
    {
      desc_list_entry->declaration.type = 0;
    }    
  }
  else
  {
    if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_WRITE)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_WRITE;
    }
    else if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_WRITE_NO_RSP)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_WRITE_NO_RSP;
    }
    else if (desc_list_entry->declaration.type & BLE_CHAR_TYPE_WRITE_SIGNED)
    {
      desc_list_entry->declaration.type = BLE_CHAR_TYPE_WRITE_SIGNED;
    }
    else
    {
      desc_list_entry->declaration.type = 0;
    }
  }

  if ((desc_list_entry->declaration.type == BLE_CHAR_TYPE_INDICATE) ||
      (desc_list_entry->declaration.type == BLE_CHAR_TYPE_NOTIFY))
  {
    uint16 bitfield = (desc_list_entry->declaration.type == BLE_CHAR_TYPE_INDICATE)
                      ? BLE_CHAR_CLIENT_INDICATE : BLE_CHAR_CLIENT_NOTIFY;

    desc_list_entry = ble_find_char_desc (desc_list_entry, BLE_GATT_CHAR_CLIENT_CONFIG);
    
    if (desc_list_entry != NULL)
    {
      desc_list_entry->data_length            = sizeof (ble_char_client_config_t);
      desc_list_entry->client_config.bitfield = bitfield;
    }
  }
}

static void ble_update_temperature (void *data)
{
  int32 update_failed = 0;
  ble_service_list_entry_t *service_list_entry = (ble_service_list_entry_t *)data;
  ble_char_list_entry_t *update_list_entry = service_list_entry->update.char_list;

  service_list_entry->update.expected_time += service_list_entry->update.interval;
  service_list_entry->update.pending        = 1;

  while (update_list_entry != NULL)
  {
    ble_char_list_entry_t *update_list_entry_tmp = NULL;
    ble_attr_list_entry_t *char_value
      = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(update_list_entry->desc_list)));
    uint16 uuid = BLE_PACK_GATT_UUID (char_value->uuid);

    if (uuid == BLE_TEMPERATURE_MEAS_UUID)
    {
      if (char_value->data_length != 0)
      {
        ble_char_temperature_t *temperature = (ble_char_temperature_t *)(char_value->data);
      
        printf ("  Temperature flags: 0x%02x\n", temperature->flags);
        printf ("              value: %.1f\n", temperature->meas_value);
        printf ("               date: %02d/%02d/%04d\n", temperature->meas_time.day,
                                                         temperature->meas_time.month,
                                                         temperature->meas_time.year);
        printf ("               time: %02d:%02d:%02d\n", temperature->meas_time.hour,
                                                         temperature->meas_time.minute,
                                                         temperature->meas_time.second);
        printf ("               type: 0x%02x\n", temperature->type);
        printf ("   Timer correction: %d\n", service_list_entry->update.timer_correction);
      }
      else
      {
        update_failed = 1;

        printf ("  Temperature not read\n");
      }
    }
    else if ((uuid == BLE_TEMPERATURE_TYPE_UUID) ||
             (uuid == BLE_MEAS_INTERVAL_UUID))
    {
      update_list_entry_tmp = update_list_entry;
    }
    else
    {
      printf ("BLE unexpected uuid 0x%04x in temperature update list\n", uuid);
    }

    if (char_value->data_length != 0)
    {
      free (char_value->data);
      char_value->data        = NULL;
      char_value->data_length = 0;
    }

    update_list_entry = update_list_entry->next;
    if (update_list_entry_tmp != NULL)
    {
      list_remove ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry_tmp);
      free (update_list_entry_tmp);
      update_list_entry_tmp = NULL;
    }
  }

  if (update_failed > 0)
  {
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
    
    /* Add measurement interval to update list */
    while (char_list_entry != NULL)
    {
      uint8 uuid_length;
      uint16 uuid;
      ble_attr_list_entry_t *desc_list_entry
        = ble_find_char_desc (char_list_entry->desc_list, BLE_GATT_CHAR_DECL);;
      
      uuid_length = desc_list_entry->data_length - 3;
      uuid        = BLE_PACK_GATT_UUID (desc_list_entry->declaration.uuid);

      if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_MEAS_INTERVAL_UUID))
      {
        update_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*update_list_entry));
        *update_list_entry = *char_list_entry;
        list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry);

        desc_list_entry = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(char_list_entry->desc_list)));
        desc_list_entry->data_length = BLE_MEAS_INTERVAL_LENGTH;
        desc_list_entry->data        = malloc (desc_list_entry->data_length);
        desc_list_entry->data[0]     = (service_list_entry->update.interval/1000) & 0xff;
        desc_list_entry->data[1]     = ((service_list_entry->update.interval/1000) >> 8) & 0xff;      
  
        service_list_entry->update.pending++;
        break;
      }      
      
      char_list_entry = char_list_entry->next;
    }
  }
}

ble_attr_list_entry_t * ble_find_char_desc (ble_attr_list_entry_t *attr_list_entry,
                                            uint16 uuid)
{
  while (attr_list_entry != NULL)
  {
    if ((attr_list_entry->uuid_length == BLE_GATT_UUID_LENGTH) &&
        ((BLE_PACK_GATT_UUID (attr_list_entry->uuid)) == uuid))
    {
      break;
    }
      
    attr_list_entry = attr_list_entry->next;
  }

  return attr_list_entry;
}

void ble_print_service (ble_device_list_entry_t *device_list_entry)
{
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;
  
  while (service_list_entry != NULL)
  {
    int32 i;
    uint16 uuid = BLE_PACK_GATT_UUID (service_list_entry->declaration.uuid);
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
      
    printf ("Service -- %s\n", ((uuid == BLE_GATT_PRI_SERVICE) ? "primary" : "secondary"));
    printf ("  handle range: 0x%04x to 0x%04x\n", service_list_entry->start_handle, service_list_entry->end_handle);
    printf ("          uuid: 0x");
    for (i = (service_list_entry->declaration.data_length - 1); i >= 0; i--)
    {
      printf ("%02x", service_list_entry->declaration.data[i]);
    }
    printf ("\n");

    while (char_list_entry != NULL)
    {
      ble_attr_list_entry_t *desc_list_entry = char_list_entry->desc_list;

      while (desc_list_entry != NULL)
      {
        uuid = BLE_PACK_GATT_UUID (desc_list_entry->uuid);
        if (uuid == BLE_GATT_CHAR_DECL)
        {
          printf ("    Characteristics -- declaration\n");
          printf ("      handle: 0x%04x\n", desc_list_entry->handle);
          printf ("       value: ");
          printf ("0x%02x, 0x%04x, 0x", desc_list_entry->declaration.type, desc_list_entry->declaration.handle);
          for (i = (desc_list_entry->data_length - 4); i >= 0; i--)
          {
            printf ("%02x", desc_list_entry->declaration.uuid[i]);
          }
          printf ("\n");
        }
        else if (uuid == BLE_GATT_CHAR_USER_DESC)
        {
          printf ("        Descriptor -- description\n");
          printf ("          handle: 0x%04x\n", desc_list_entry->handle);
          printf ("           value: ");
          for (i = 0; i < desc_list_entry->data_length; i++)
          {
            printf ("%c", (int8)(desc_list_entry->data[i]));
          }
          printf ("\n");
        }
        else if (uuid == BLE_GATT_CHAR_FORMAT)
        {
          printf ("        Descriptor -- format\n");
          printf ("          handle: 0x%04x\n", desc_list_entry->handle);
          printf ("           value: ");
          printf ("0x%02x, %d\n", desc_list_entry->format.bitfield, desc_list_entry->format.exponent);
        }
        else if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
        {
          printf ("        Descriptor -- client configuration\n");
          printf ("          handle: 0x%04x\n", desc_list_entry->handle);
          printf ("           value: ");
          printf ("0x%04x\n", desc_list_entry->client_config.bitfield);
        }
        
        desc_list_entry = desc_list_entry->next;
      }
      
      char_list_entry = char_list_entry->next;
    }
    
    service_list_entry = service_list_entry->next;
  }
}

int32 ble_init_service (ble_device_list_entry_t *device_list_entry)
{
  int32 status = 0;
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;
  
  while (service_list_entry != NULL)
  {
    int32 found;
    uint8 uuid_length;
    uint16 uuid;

    found       = 0;
    uuid_length = service_list_entry->declaration.data_length;
    uuid        = BLE_PACK_GATT_UUID (service_list_entry->declaration.data);

    if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_TEMPERATURE_SERVICE_UUID))
    {
      ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

      printf ("BLE Found temperature service\n");
      
      /* Temperature service; add temperature measurement, temperature type 
       * and measurement interval to update list */
      while (char_list_entry != NULL)
      {
        ble_attr_list_entry_t *desc_list_entry
          = ble_find_char_desc (char_list_entry->desc_list, BLE_GATT_CHAR_DECL);;
        
        uuid_length = desc_list_entry->data_length - 3;
        uuid        = BLE_PACK_GATT_UUID (desc_list_entry->declaration.uuid);
        
        if ((uuid_length == BLE_GATT_UUID_LENGTH) &&
            ((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
             (uuid == BLE_TEMPERATURE_TYPE_UUID) ||
             (uuid == BLE_MEAS_INTERVAL_UUID)))
        {
          ble_char_list_entry_t *update_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*update_list_entry));
          ble_attr_list_entry_t *char_value;

          char_value = (ble_attr_list_entry_t *)malloc (sizeof (*char_value));
          char_value->handle      = desc_list_entry->declaration.handle;
          char_value->uuid_length = uuid_length;
          memcpy (char_value->uuid, desc_list_entry->declaration.uuid, uuid_length);
          char_value->data_length = 0;
          char_value->data        = NULL;
          list_add ((list_entry_t **)(&(char_list_entry->desc_list)), (list_entry_t *)char_value);

          *update_list_entry = *char_list_entry;
          list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry);

          if ((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
              (uuid == BLE_TEMPERATURE_TYPE_UUID))
          {
            ble_update_char_type (desc_list_entry, BLE_CHAR_TYPE_READ);         
          }
          else
          {
            ble_update_char_type (desc_list_entry, BLE_CHAR_TYPE_WRITE);          

            char_value->data_length  = BLE_MEAS_INTERVAL_LENGTH;
            char_value->data         = malloc (desc_list_entry->data_length);
            
            if ((service_list_entry->update.interval < BLE_MIN_TEMPERATURE_MEAS_INTERVAL) &&
                (service_list_entry->update.interval > BLE_MAX_TEMPERATURE_MEAS_INTERVAL))
            {
              service_list_entry->update.interval = BLE_TEMPERATURE_MEAS_INTERVAL;
            }
            char_value->data[0]      = (service_list_entry->update.interval/1000) & 0xff;
            char_value->data[1]      = ((service_list_entry->update.interval/1000) >> 8) & 0xff;
          }

          found++;
        }

        char_list_entry = char_list_entry->next;
      }

      if (found > 0)
      {
        service_list_entry->update.pending  = found;
        service_list_entry->update.callback = ble_update_temperature;
      }
    }

    if (found > 0)
    {
      db_column_value_t column_value;
 
      column_value.blob.data   = device_list_entry->address.byte;
      column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 1, &column_value);
      column_value.blob.data   = service_list_entry->declaration.data;
      column_value.blob.length = service_list_entry->declaration.data_length;
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 3, &column_value);
      column_value.text = "Active";
      db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 5, &column_value);
      db_write_table (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0);
    }    

    status             += found;
    service_list_entry  = service_list_entry->next;
  }

  return status;  
}

ble_service_list_entry_t * ble_find_service (ble_device_list_entry_t *device_list_entry,
                                             uint8 *uuid, uint8 uuid_length)
{
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

  while (service_list_entry != NULL)
  {
    if ((memcmp (uuid, service_list_entry->declaration.data, uuid_length)) == 0)
    {
      break;
    }
    
    service_list_entry = service_list_entry->next;
  }

  return service_list_entry;  
}

void ble_clear_service (ble_device_list_entry_t *device_list_entry)
{
  ble_service_list_entry_t *service_list_entry = device_list_entry->service_list;

  while (service_list_entry != NULL)
  {
    while (service_list_entry->char_list != NULL)
    {
      ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
  
      while (char_list_entry->desc_list != NULL)
      {
        ble_attr_list_entry_t *desc_list_entry = char_list_entry->desc_list;
        list_remove ((list_entry_t **)(&(char_list_entry->desc_list)), (list_entry_t *)desc_list_entry);
        free (desc_list_entry);
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
    for (i = 0; i < service_list_entry->declaration.data_length; i++)
    {
      printf ("%02x", service_list_entry->declaration.data[i]);
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
  
  if (db_info.handle == NULL)
  {
    status = db_open (&db_info);
    if (status > 0)
    {
      status = db_create_table (&db_info, &(db_static_tables[DB_DEVICE_LIST_TABLE]));
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

      db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 1, &column_value);
      memcpy (address.byte, column_value.blob.data, BLE_DEVICE_ADDRESS_LENGTH);
      address.type = BLE_ADDR_PUBLIC;
      device_list_entry = ble_find_device (*device_list, &address);
      
      if (device_list_entry == NULL)
      {
        device_list_entry = (ble_device_list_entry_t *)malloc (sizeof (*device_list_entry));
        
        device_list_entry->address      = address;
        device_list_entry->service_list = NULL;
        device_list_entry->status       = BLE_DEVICE_DISCOVER;
        device_list_entry->setup_time   = 0;

        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 2, &column_value);
        device_list_entry->name = strdup (column_value.text);
        
        list_add ((list_entry_t **)(&device_list), (list_entry_t *)device_list_entry);
      }

      db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 3, &column_value);
      service_list_entry = ble_find_service (device_list_entry, column_value.blob.data, column_value.blob.length);
      
      if (service_list_entry == NULL)
      {
        service_list_entry = (ble_service_list_entry_t *)malloc (sizeof (*service_list_entry));
        
        service_list_entry->declaration.handle       = BLE_INVALID_GATT_HANDLE;
        service_list_entry->declaration.uuid_length  = 0;
        service_list_entry->declaration.data_length  = column_value.blob.length;
        service_list_entry->declaration.data         = malloc (column_value.blob.length);
        memcpy (service_list_entry->declaration.data, column_value.blob.data, column_value.blob.length);

        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->start_handle = BLE_INVALID_GATT_HANDLE;
        service_list_entry->include_list = NULL;
        service_list_entry->char_list    = NULL;

        db_read_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 4, &column_value);
        service_list_entry->update.char_list        = NULL;
        service_list_entry->update.pending          = 0;
        service_list_entry->update.init             = 1;
        service_list_entry->update.expected_time    = 0;
        service_list_entry->update.timer            = 0;
        service_list_entry->update.timer_correction = 0;
        service_list_entry->update.interval         = (column_value.integer * 60 * 1000);
        service_list_entry->update.callback         = NULL;

        list_add ((list_entry_t **)(&(device_list_entry->service_list)), (list_entry_t *)service_list_entry);  

        column_value.blob.data   = device_list_entry->address.byte;
        column_value.blob.length = BLE_DEVICE_ADDRESS_LENGTH;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 1, &column_value);
        column_value.blob.data   = service_list_entry->declaration.data;
        column_value.blob.length = service_list_entry->declaration.data_length;
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 3, &column_value);
        column_value.text = "Inactive";
        db_write_column (&(db_static_tables[DB_DEVICE_LIST_TABLE]), 0, 5, &column_value);
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

