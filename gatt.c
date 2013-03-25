
#include <stdio.h>
#include <string.h>

#include "ble.h"
#include "gatt.h"

typedef struct
{
  ble_attribute_t   attribute;
  ble_char_update_t update;
} ble_char_value_t;

/* Temperature update interval in min. */
#define BLE_TEMPERATURE_MEAS_INTERVAL  (15)

static void ble_update_temperature (ble_attribute_t *attr);

static ble_char_value_t temperature = 
{
  .attribute =
  {
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x01, 0xe1},
    .value_length = 0,
    .value        = NULL,
  },
  .update =
  {
    .type     = BLE_ATTR_UPDATE_READ,
    .timer    = 0,
    .callback = ble_update_temperature,
  },
};


static void ble_update_temperature (ble_attribute_t *attribute)
{
}

int ble_lookup_uuid (ble_char_list_entry_t *characteristics)
{
  int found = 0;
  ble_attribute_t *value = &(characteristics->value);

  if ((value->uuid_length == temperature.attribute.uuid_length) &&
      ((memcmp (value->uuid, temperature.attribute.uuid, value->uuid_length)) == 0))
  {
    temperature.attribute.handle = value->handle;
    characteristics->update = temperature.update;
    found = 1;
  }

  return found;
}

