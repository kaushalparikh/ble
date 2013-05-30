
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
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
#define BLE_TEMPERATURE_MEAS_INTERVAL  (2*60*1000)

typedef struct PACKED
{
  uint8           flags;
  float           meas_value;
  ble_char_time_t meas_time;
  uint8           type;
} ble_char_temperature_t;

static void ble_update_temperature (int32 device_id, void *data);

static FILE *temperature_file[256];


static void ble_update_temperature (int32 device_id, void *data)
{
  int32 update_failed = 0;
  ble_service_list_entry_t *service_list_entry = (ble_service_list_entry_t *)data;
  ble_char_list_entry_t *update_list_entry = service_list_entry->update.char_list;

  printf ("Device ID: %02d\n", device_id);

  service_list_entry->update.expected_time += BLE_TEMPERATURE_MEAS_INTERVAL;
  service_list_entry->update.pending        = 1;

  while (update_list_entry != NULL)
  {
    ble_char_list_entry_t *update_list_entry_tmp = NULL;
    ble_attr_list_entry_t *attribute 
      = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(update_list_entry->desc_list)));
    uint16 uuid = (attribute->uuid[1] << 8) | attribute->uuid[0];

    if (uuid == BLE_TEMPERATURE_MEAS_UUID)
    {
      if (attribute->value_length != 0)
      {
        ble_char_temperature_t *temperature = (ble_char_temperature_t *)(attribute->value);
      
        fprintf (temperature_file[device_id-1], "%.1f\n", temperature->meas_value);
        
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
      
        free (attribute->value);
        attribute->value        = NULL;
        attribute->value_length = 0;
      }
      else
      {
        update_failed = 1;

        fprintf (temperature_file[device_id-1], "NA\n");
        printf ("  Temperature not read\n");
      }
    }
    else if ((uuid == BLE_TEMPERATURE_TYPE_UUID) ||
             (uuid == BLE_MEAS_INTERVAL_UUID))
    {
      if (attribute->value_length != 0)
      {
        free (attribute->value);
        attribute->value        = NULL;
        attribute->value_length = 0;
      }

      update_list_entry_tmp = update_list_entry;
    }
    else
    {
      printf ("BLE unexpected uuid 0x%04x in temperature update list\n", uuid);
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
      ble_attr_list_entry_t *desc_list_entry = char_list_entry->desc_list;
      ble_char_decl_t *char_decl = (ble_char_decl_t *)(desc_list_entry->value);
      uint8 value_uuid_length = desc_list_entry->value_length - 3;
      uint16 uuid = (char_decl->value_uuid[1] << 8) | char_decl->value_uuid[0];

      if ((value_uuid_length == BLE_GATT_UUID_LENGTH) &&
          (uuid == BLE_MEAS_INTERVAL_UUID))
      {
        update_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*update_list_entry));
        *update_list_entry = *char_list_entry;
        list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry);

        desc_list_entry = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(char_list_entry->desc_list)));
        desc_list_entry->value_length = BLE_MEAS_INTERVAL_LENGTH;
        desc_list_entry->value = malloc (desc_list_entry->value_length);
        desc_list_entry->value[0] = (BLE_TEMPERATURE_MEAS_INTERVAL/1000) & 0xff;
        desc_list_entry->value[1] = ((BLE_TEMPERATURE_MEAS_INTERVAL/1000) >> 8) & 0xff;      
  
        service_list_entry->update.pending++;
        break;
      }      
      
      char_list_entry = char_list_entry->next;
    }
  }

  fflush (temperature_file[device_id-1]);
}

int32 ble_lookup_service (ble_service_list_entry_t *service_list_entry)
{
  int32 found = 0;
  uint16 uuid = (service_list_entry->declaration.value[1] << 8) | service_list_entry->declaration.value[0];

  service_list_entry->update.char_list        = NULL;
  service_list_entry->update.pending          = 0;
  service_list_entry->update.init             = 1;
  service_list_entry->update.expected_time    = 0;
  service_list_entry->update.timer            = 0;
  service_list_entry->update.timer_correction = 0;
  service_list_entry->update.callback         = NULL;

  if ((service_list_entry->declaration.value_length == BLE_GATT_UUID_LENGTH) &&
      (uuid == BLE_TEMPERATURE_SERVICE_UUID))
  {
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

    printf ("BLE Found temperature service\n");
    
    /* Temperature service; add temperature measurement, temperature type 
     * and measurement interval to update list */
    while (char_list_entry != NULL)
    {
      ble_attr_list_entry_t *desc_list_entry = char_list_entry->desc_list;
      ble_char_decl_t *char_decl = (ble_char_decl_t *)(desc_list_entry->value);
      uint8 value_uuid_length = desc_list_entry->value_length - 3;

      uuid = (char_decl->value_uuid[1] << 8) | char_decl->value_uuid[0];
      if ((value_uuid_length == BLE_GATT_UUID_LENGTH) &&
          ((uuid == BLE_TEMPERATURE_MEAS_UUID) ||
           (uuid == BLE_TEMPERATURE_TYPE_UUID) ||
           (uuid == BLE_MEAS_INTERVAL_UUID)))
      {
        ble_char_list_entry_t *update_list_entry = (ble_char_list_entry_t *)malloc (sizeof (*update_list_entry));

        desc_list_entry = (ble_attr_list_entry_t *)malloc (sizeof (*desc_list_entry));
        desc_list_entry->handle = char_decl->value_handle;
        desc_list_entry->uuid_length = value_uuid_length;
        memcpy (desc_list_entry->uuid, char_decl->value_uuid, value_uuid_length);
        desc_list_entry->value_length = 0;
        desc_list_entry->value = NULL;
        list_add ((list_entry_t **)(&(char_list_entry->desc_list)), (list_entry_t *)desc_list_entry);

        *update_list_entry = *char_list_entry;
        list_add ((list_entry_t **)(&(service_list_entry->update.char_list)), (list_entry_t *)update_list_entry);

        if (uuid == BLE_TEMPERATURE_MEAS_UUID)
        {
          printf ("BLE Found temperature measurement characteristics\n");

          if (char_decl->properties & BLE_CHAR_PROPERTY_READ)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_READ;
          }
          else if (char_decl->properties & BLE_CHAR_PROPERTY_INDICATE)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_INDICATE;
          }
          else if (char_decl->properties & BLE_CHAR_PROPERTY_NOTIFY)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_NOTIFY;
          }
          else
          {
            printf ("BLE temperature measurement property not handled\n");
          }

          desc_list_entry = char_list_entry->desc_list;
          while (desc_list_entry != NULL)
          {
            if ((((desc_list_entry->uuid[1]) << 8) | desc_list_entry->uuid[0]) == BLE_GATT_CHAR_CLIENT_CONFIG)
            {
              ble_char_client_config_t *client_config = (ble_char_client_config_t *)malloc (sizeof (*client_config));
    
              client_config->bitfield = (char_decl->properties == BLE_CHAR_PROPERTY_INDICATE)
                                        ? BLE_CHAR_CLIENT_INDICATE : BLE_CHAR_CLIENT_NOTIFY;
              
              desc_list_entry->value_length = sizeof (*client_config);
              desc_list_entry->value = (uint8 *)client_config;
              break;
            }
    
            desc_list_entry = desc_list_entry->next;
          }          
        }
        else if (uuid == BLE_TEMPERATURE_TYPE_UUID)
        {
          printf ("BLE Found temperature type characteristics\n");

          if (char_decl->properties & BLE_CHAR_PROPERTY_READ)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_READ;
          }
          else
          {
            printf ("BLE temperature type property not handled\n");
          }          
        }
        else
        {
          printf ("BLE Found temperature measurement interval characteristics\n");

          if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_WRITE;
          }
          else if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE_NO_RSP)
          {
            char_decl->properties = BLE_CHAR_PROPERTY_WRITE_NO_RSP;
          }
          else
          {
            printf ("BLE temperature interval property not handled\n");
          }          
          
          desc_list_entry->value_length = BLE_MEAS_INTERVAL_LENGTH;
          desc_list_entry->value = malloc (desc_list_entry->value_length);
          desc_list_entry->value[0] = (BLE_TEMPERATURE_MEAS_INTERVAL/1000) & 0xff;
          desc_list_entry->value[1] = ((BLE_TEMPERATURE_MEAS_INTERVAL/1000) >> 8) & 0xff;
        }

        found++;
      }

      char_list_entry = char_list_entry->next;
    }

    if (found > 0)
    {
      service_list_entry->update.pending  = 3;
      service_list_entry->update.callback = ble_update_temperature;
    }
  }

  return found;  
}

uint32 ble_identify_device (uint8 *address, ble_service_list_entry_t *service_list_entry)
{
  int32 id = 0;
  int8 *file_name;
  
  if (service_list_entry != NULL)
  {
    id = address[0];

    if ((asprintf (&file_name, "temperature.%03d", id)) > 0)
    {
      temperature_file[id-1] = fopen (file_name, "w");
      printf ("Storing temperature in file %s\n", file_name);
      free (file_name);
    }
  }

  return id;
}

