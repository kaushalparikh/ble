
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "temperature.h"

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

enum
{
  DB_TEMPERATURE_TABLE_COLUMN_NO = 0,
  DB_TEMPERATURE_TABLE_COLUMN_TIME,
  DB_TEMPERATURE_TABLE_COLUMN_TEMPERATURE,
  DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL,
  DB_TEMPERATURE_TABLE_NUM_COLUMNS
};

static db_column_entry_t db_temperature_table_columns[DB_TEMPERATURE_TABLE_NUM_COLUMNS] =
{
  {"No.",               DB_TEMPERATURE_TABLE_COLUMN_NO,          DB_COLUMN_TYPE_INT,
    DB_COLUMN_FLAG_PRIMARY_KEY,                                   NULL},
  {"Time",              DB_TEMPERATURE_TABLE_COLUMN_TIME,        DB_COLUMN_TYPE_TEXT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),        NULL},
  {"Temperature (C)",   DB_TEMPERATURE_TABLE_COLUMN_TEMPERATURE, DB_COLUMN_TYPE_FLOAT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),        NULL},
  {"Battery Level (%)", DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL,   DB_COLUMN_TYPE_INT,
    (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),        NULL},
};

static db_info_t *db_info = NULL;


void ble_update_temperature (ble_service_list_entry_t *service_list_entry,
                             ble_device_list_entry_t *device_list_entry)
{
  int32 update_failed;
  int32 current_time;
  db_table_list_entry_t *table_list_entry;
  ble_char_list_entry_t *update_list_entry;
  db_column_value_t column_value;
  ble_sync_list_entry_t *sync_list_entry;
  ble_sync_temperature_data_t *sync_temperature_data;

  sync_list_entry            = (ble_sync_list_entry_t *)malloc (sizeof (*sync_list_entry));
  sync_list_entry->type      = BLE_SYNC_PUSH;
  sync_list_entry->data_type = BLE_SYNC_TEMPERATURE;
  sync_list_entry->data      = malloc (sizeof (ble_sync_temperature_data_t));
  sync_temperature_data      = (ble_sync_temperature_data_t *)(sync_list_entry->data);

  update_failed     = 0;
  current_time      = clock_get_count ();
  table_list_entry  = (db_table_list_entry_t *)(device_list_entry->data);
  update_list_entry = service_list_entry->update.char_list;

  column_value.text = clock_get_time ();
  db_write_column (table_list_entry, DB_WRITE_INSERT, DB_TEMPERATURE_TABLE_COLUMN_TIME, &column_value);
  db_write_column (table_list_entry, DB_WRITE_INSERT, DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL, NULL);
  db_write_column (table_list_entry, DB_WRITE_INSERT, DB_TEMPERATURE_TABLE_COLUMN_BAT_LEVEL, NULL);

  sync_temperature_data->time = strdup (column_value.text);
  free (column_value.text);
  sync_temperature_data->temperature   = FLT_MAX;
  sync_temperature_data->battery_level = UINT_MAX;

  printf ("Device: %s\n", device_list_entry->name);

  if (service_list_entry->update.init)
  {
    service_list_entry->update.init        = 0;
    service_list_entry->update.time        = current_time;
    service_list_entry->update.time_offset = 0;
  }
  else
  {
    service_list_entry->update.time_offset = current_time - service_list_entry->update.time;        
  }
      
  service_list_entry->update.time += service_list_entry->update.interval;
      
  while (update_list_entry != NULL)
  {
    uint16 uuid = BLE_PACK_GATT_UUID (update_list_entry->value->uuid);
    ble_char_list_entry_t *update_list_entry_del = NULL;
  
    if (uuid == BLE_TEMPERATURE_MEAS_UUID)
    {
      if (update_list_entry->value->data != NULL)
      {
        ble_char_temperature_t *temperature = (ble_char_temperature_t *)(update_list_entry->value->data);
      
        printf ("  Temperature flags: 0x%02x\n", temperature->flags);
        printf ("              value: %.1f (C)\n", temperature->meas_value);
        printf ("               type: 0x%02x\n", temperature->type);
        printf ("               date: %02d/%02d/%04d\n", temperature->meas_time.day,
                                                         temperature->meas_time.month,
                                                         temperature->meas_time.year);
        printf ("               time: %02d:%02d:%02d\n", temperature->meas_time.hour,
                                                         temperature->meas_time.minute,
                                                         temperature->meas_time.second);
        printf ("             offset: %d (ms)\n", service_list_entry->update.time_offset);
  
        column_value.decimal = temperature->meas_value;
        (void)db_write_column (table_list_entry, DB_WRITE_INSERT, DB_TEMPERATURE_TABLE_COLUMN_TEMPERATURE, &column_value);
        sync_temperature_data->temperature = temperature->meas_value;
      }
      else
      {
        update_failed = 1;
  
        printf ("  Temperature not read\n");
      }
    }
    else
    {
      update_list_entry_del = update_list_entry;
    }

    if (((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
         (uuid == BLE_TEMPERATURE_TYPE_UUID)) &&
        (update_list_entry->value->data != NULL))
    {
      free (update_list_entry->value->data);
      update_list_entry->value->data        = NULL;
      update_list_entry->value->data_length = 0;
    }
  
    update_list_entry = update_list_entry->next;
    
    if (update_list_entry_del != NULL)
    {
      list_remove ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry_del);
      list_add ((list_entry_t **)(&(service_list_entry->char_list)), (list_entry_t *)update_list_entry_del);
      update_list_entry_del = NULL;
    }
  }
  
  if (update_failed)
  {
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
    
    /* Add measurement interval to update list */
    while (char_list_entry != NULL)
    {
      uint8 uuid_length;
      uint16 uuid;
      ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
      
      uuid_length = char_list_entry->declaration->data_length - 3;
      uuid        = BLE_PACK_GATT_UUID (char_decl->uuid);
  
      if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_MEAS_INTERVAL_UUID))
      {
        char_list_entry->value->type        = char_decl->type;
        char_list_entry->value->handle      = char_decl->handle;
        char_list_entry->value->uuid_length = uuid_length;
        memcpy (char_list_entry->value->uuid, char_decl->uuid, uuid_length);
        char_list_entry->value->data_length = BLE_MEAS_INTERVAL_LENGTH;
        char_list_entry->value->data        = malloc (char_list_entry->value->data_length);

        char_list_entry->value->data[0] = (service_list_entry->update.interval/1000) & 0xff;
        char_list_entry->value->data[1] = ((service_list_entry->update.interval/1000) >> 8) & 0xff;            

        ble_update_char_type (char_list_entry, BLE_ATTR_TYPE_WRITE);

        list_remove ((list_entry_t **)(&(service_list_entry->char_list)), (list_entry_t *)char_list_entry);
        list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)char_list_entry);

        break;
      }      
      
      char_list_entry = char_list_entry->next;
    }
  }

  db_write_table (table_list_entry, DB_WRITE_INSERT);
  ble_sync_push (sync_list_entry);
}

int32 ble_init_temperature (ble_service_list_entry_t *service_list_entry,
                            ble_device_list_entry_t *device_list_entry)
{
  int32 found = 0;
  uint8 uuid_length;
  uint16 uuid;
  ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

  /* Temperature service; add temperature measurement, temperature type 
   * and measurement interval to update list */
  while (char_list_entry != NULL)
  {
    ble_char_list_entry_t *update_list_entry = NULL;
    ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
    
    uuid_length = char_list_entry->declaration->data_length - 3;
    uuid        = BLE_PACK_GATT_UUID (char_decl->uuid);       
    
    if ((uuid_length == BLE_GATT_UUID_LENGTH) &&
        ((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
         (uuid == BLE_TEMPERATURE_TYPE_UUID) ||
         (uuid == BLE_MEAS_INTERVAL_UUID)))
    {
      char_list_entry->value = (ble_attribute_t *)malloc (sizeof (ble_attribute_t));
        
      char_list_entry->value->type        = char_decl->type;
      char_list_entry->value->handle      = char_decl->handle;
      char_list_entry->value->uuid_length = uuid_length;
      memcpy (char_list_entry->value->uuid, char_decl->uuid, uuid_length);
      char_list_entry->value->data_length = 0;
      char_list_entry->value->data        = NULL;

      if ((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
          (uuid == BLE_TEMPERATURE_TYPE_UUID))
      {
        ble_update_char_type (char_list_entry, BLE_ATTR_TYPE_READ);
      }
      else
      {
        char_list_entry->value->data_length = BLE_MEAS_INTERVAL_LENGTH;
        char_list_entry->value->data        = malloc (char_list_entry->value->data_length);
        
        if ((service_list_entry->update.interval < BLE_MIN_TEMPERATURE_MEAS_INTERVAL) &&
            (service_list_entry->update.interval > BLE_MAX_TEMPERATURE_MEAS_INTERVAL))
        {
          service_list_entry->update.interval = BLE_TEMPERATURE_MEAS_INTERVAL;
        }
        char_list_entry->value->data[0] = (service_list_entry->update.interval/1000) & 0xff;
        char_list_entry->value->data[1] = ((service_list_entry->update.interval/1000) >> 8) & 0xff;

        ble_update_char_type (char_list_entry, BLE_ATTR_TYPE_WRITE);
      }

      update_list_entry = char_list_entry;
      found++;
    }

    char_list_entry = char_list_entry->next;
    if (update_list_entry != NULL)
    {
      list_remove ((list_entry_t **)(&(service_list_entry->char_list)), (list_entry_t *)update_list_entry);
      list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry);
      update_list_entry = NULL;
    }
  }

  if (found > 0)
  {
    if (db_info == NULL)
    {
      db_open ("gateway.db", &db_info);
    }
        
    if (db_info != NULL)
    {
      db_table_list_entry_t *table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));
      
      table_list_entry->title       = strdup (device_list_entry->name);
      table_list_entry->num_columns = DB_TEMPERATURE_TABLE_NUM_COLUMNS;
      table_list_entry->column      = db_temperature_table_columns;
      table_list_entry->insert      = NULL;
      table_list_entry->update      = NULL;
      table_list_entry->delete      = NULL;
      table_list_entry->select      = NULL;

      if ((db_create_table (db_info, table_list_entry)) > 0)
      {
        device_list_entry->data = table_list_entry;
      }
    }
  }

  return found;
}

