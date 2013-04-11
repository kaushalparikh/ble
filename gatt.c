
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

/* Temperature update interval in min. */
#define BLE_TEMPERATURE_MEAS_INTERVAL  (1)
#define BLE_TEMPERATURE_VALUE_LENGTH   (20)

static void ble_update_temperature (void *data);

static ble_char_value_t temperature = 
{
  .attribute =
  {
    .next         = NULL,
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x01, 0xe1},
    .value_length = BLE_TEMPERATURE_VALUE_LENGTH,
    .value        = NULL,
  },
  .update =
  {
    .type     = 0,
    .timer    = 0,
    .callback = ble_update_temperature,
  },
};


static void ble_update_temperature (void *data)
{
  int i;
  ble_char_list_entry_t *characteristics = (ble_char_list_entry_t *)data;
  ble_attr_list_entry_t *attribute = (ble_attr_list_entry_t *)list_tail ((list_entry_t **)(&(characteristics->desc_list)));

  printf ("Temperature value ");
  for (i = (attribute->value_length - 1); i >= 0; i--)
  {
    printf ("%02x", attribute->value[i]);
  }
  printf ("\n");

  characteristics->update.timer = BLE_TEMPERATURE_MEAS_INTERVAL;
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
    desc_list_entry->value_length = temperature.attribute.value_length;
    desc_list_entry->value        = malloc (temperature.attribute.value_length);
    list_add ((list_entry_t **)(&(characteristics->desc_list)), (list_entry_t *)desc_list_entry);

    characteristics->update = temperature.update;
    found = 1;
  }

  if (found == 1)
  {
    if (char_decl->properties & BLE_CHAR_PROPERTY_READ)
    {
      characteristics->update.type |= BLE_CHAR_READ_DATA;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_INDICATE)
    {
      characteristics->update.type |= BLE_CHAR_INDICATE_DATA;
    }
    else if (char_decl->properties & BLE_CHAR_PROPERTY_NOTIFY)
    {
      characteristics->update.type |= BLE_CHAR_NOTIFY_DATA;
    }

    if (char_decl->properties & BLE_CHAR_PROPERTY_WRITE)
    {
      characteristics->update.type |= BLE_CHAR_WRITE_DATA;
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
