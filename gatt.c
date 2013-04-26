
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "gatt.h"
#include "ble.h"

typedef struct
{
  ble_attr_list_entry_t attribute;
  ble_char_update_t     update;
} ble_char_value_t;

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

/* Temperature service definitions */
/* Update interval in millisec */
#define BLE_TEMPERATURE_MEAS_INTERVAL  (2*60*1000)
#define BLE_MEAS_INTERVAL_LENGTH       (2)
#define BLE_INVALID_MEAS_INTERVAL      (0x7fffffff)

typedef struct PACKED
{
  uint8           flags;
  float           meas_value;
  ble_char_time_t meas_time;
  uint8           type;
} ble_char_temperature_t;

static void ble_update_temperature (void *data);
static void ble_update_temperature_interval (void *data);

static ble_char_value_t temperature = 
{
  .attribute =
  {
    .next         = NULL,
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x1c, 0x2a},
    .value_length = 0,
    .value        = NULL,
  },
  .update =
  {
    .type             = 0,
    .timer            = 0,
    .callback         = ble_update_temperature,
  },
};

static ble_char_value_t temperature_interval = 
{
  .attribute =
  {
    .next         = NULL,
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x21, 0x2a},
    .value_length = 0,
    .value        = NULL,
  },
  .update =
  {
    .type             = 0,
    .timer            = 0,
    .callback         = ble_update_temperature_interval,
  },
};


static void ble_update_temperature (void *data)
{
  ble_char_list_entry_t *characteristics = (ble_char_list_entry_t *)data;
  ble_attr_list_entry_t *attribute = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(characteristics->desc_list)));
  
  if (attribute->value_length != 0)
  {
    ble_char_temperature_t *temperature = (ble_char_temperature_t *)(attribute->value);
    
    printf ("Temperature flags: 0x%02x\n", temperature->flags);
    printf ("            value: %.1f\n", temperature->meas_value);
    printf ("             date: %02d/%02d/%04d\n", temperature->meas_time.day,
                                                  temperature->meas_time.month,
                                                  temperature->meas_time.year);
    printf ("             time: %02d:%02d:%02d\n", temperature->meas_time.hour,
                                                  temperature->meas_time.minute,
                                                  temperature->meas_time.second);
    printf ("             type: 0x%02x\n", temperature->type);

    free (attribute->value);
    attribute->value        = NULL;
    attribute->value_length = 0;
  }
  else
  {
    printf ("Temperature value not read\n");
  }
    
  characteristics->update.timer = BLE_TEMPERATURE_MEAS_INTERVAL;
}

static void ble_update_temperature_interval (void *data)
{
  ble_char_list_entry_t *characteristics = (ble_char_list_entry_t *)data;
  ble_attr_list_entry_t *attribute = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(characteristics->desc_list)));

  if (attribute->value_length != 0)
  {
    free (attribute->value);
    attribute->value        = NULL;
    attribute->value_length = 0;
  }
 
  characteristics->update.timer = BLE_INVALID_MEAS_INTERVAL;
}

int ble_lookup_uuid (ble_char_list_entry_t *characteristics)
{
  int found = 0;
  ble_attr_list_entry_t *desc_list = characteristics->desc_list;
  ble_char_decl_t *char_decl = (ble_char_decl_t *)(desc_list->value);
  uint8 value_uuid_length = desc_list->value_length - 3;

  if ((value_uuid_length == temperature.attribute.uuid_length) &&
      ((memcmp (char_decl->value_uuid, temperature.attribute.uuid, value_uuid_length)) == 0))
  {
    ble_attr_list_entry_t *desc_list_entry = (ble_attr_list_entry_t *)malloc (sizeof (*desc_list_entry));

    desc_list_entry->handle       = char_decl->value_handle;
    desc_list_entry->uuid_length  = value_uuid_length;
    memcpy (desc_list_entry->uuid, char_decl->value_uuid, value_uuid_length);
    desc_list_entry->value_length = 0;
    desc_list_entry->value        = NULL;
    list_add ((list_entry_t **)(&(characteristics->desc_list)), (list_entry_t *)desc_list_entry);

    characteristics->update = temperature.update;
    found = 1;
  }
  else if ((value_uuid_length == temperature_interval.attribute.uuid_length) &&
           ((memcmp (char_decl->value_uuid, temperature_interval.attribute.uuid, value_uuid_length)) == 0))
  {
    ble_attr_list_entry_t *desc_list_entry = (ble_attr_list_entry_t *)malloc (sizeof (*desc_list_entry));

    desc_list_entry->handle       = char_decl->value_handle;
    desc_list_entry->uuid_length  = value_uuid_length;
    memcpy (desc_list_entry->uuid, char_decl->value_uuid, value_uuid_length);
    desc_list_entry->value_length = BLE_MEAS_INTERVAL_LENGTH;
    desc_list_entry->value        = malloc (desc_list_entry->value_length);
    desc_list_entry->value[0]     = (BLE_TEMPERATURE_MEAS_INTERVAL/1000) & 0xff;
    desc_list_entry->value[1]     = ((BLE_TEMPERATURE_MEAS_INTERVAL/1000) >> 8) & 0xff;
    list_add ((list_entry_t **)(&(characteristics->desc_list)), (list_entry_t *)desc_list_entry);

    characteristics->update = temperature_interval.update;
    found = 1;
  }


  if (found == 1)
  {
    if (char_decl->properties & BLE_CHAR_PROPERTY_READ)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_READ;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_INDICATE)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_INDICATE;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_NOTIFY)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_NOTIFY;
    }

    if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_WRITE;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE_NO_RSP)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_WRITE_NO_RSP;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE_SIGNED)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_WRITE_SIGNED;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_BROADCAST)
    {
      characteristics->update.type = BLE_CHAR_PROPERTY_BROADCAST;
    }
  
    if (characteristics->update.type & (BLE_CHAR_PROPERTY_NOTIFY | BLE_CHAR_PROPERTY_INDICATE))
    {
      desc_list = characteristics->desc_list;
      
      while (desc_list != NULL)
      {
        uint16 uuid = ((desc_list->uuid[1]) << 8) | desc_list->uuid[0];
        
        if (uuid == BLE_GATT_CHAR_CLIENT_CONFIG)
        {
          ble_char_client_config_t *client_config = (ble_char_client_config_t *)malloc (sizeof (*client_config));

          client_config->bitfield = (characteristics->update.type & BLE_CHAR_PROPERTY_INDICATE)
                                    ? BLE_CHAR_CLIENT_INDICATE : BLE_CHAR_CLIENT_NOTIFY;
          desc_list->value_length = sizeof (*client_config);
          desc_list->value        = (uint8 *)client_config;
          break;
        }

        desc_list = desc_list->next;
      }
    }
  }

  return found;
}

uint32 ble_identify_device (ble_char_list_entry_t *update_list)
{
  uint32 id = 0;
  
  if (update_list != NULL)
  {
    id = 1;
  }

  return id;
}
