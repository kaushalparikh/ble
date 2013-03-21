
#include <stdio.h>
#include <string.h>

#include "ble.h"
#include "gatt.h"

/* Temperature update interval in min. */
#define BLE_TEMPERATURE_MEAS_INTERVAL  (15)

static void ble_update_temperature (ble_attribute_t *attr);

static ble_attr_list_entry_t temperature = 
{
  .next            = NULL,
  .attribute =
  {
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x01, 0xe1},
    .value_length = 0,
    .value        = NULL,
  },
  .update_type     = BLE_ATTR_UPDATE_READ,
  .update_timer    = 0,
  .update_callback = ble_update_temperature,
};


static void ble_update_temperature (ble_attribute_t *attribute)
{
}

ble_attr_list_entry_t * ble_lookup_uuid (uint8 uuid_length, uint8 *uuid, uint16 handle)
{
  ble_attr_list_entry_t *attr_list_entry = NULL;

  if ((memcmp (uuid, temperature.attribute.uuid, uuid_length)) == 0)
  {
    temperature.attribute.handle = handle;
    attr_list_entry = &temperature;
  }

  return attr_list_entry;
}

