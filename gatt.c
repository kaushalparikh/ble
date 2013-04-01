
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
#define BLE_TEMPERATURE_MEAS_INTERVAL  (1)
#define BLE_TEMPERATURE_VALUE_LENGTH   (20)

static void ble_update_temperature (void *data);

static ble_char_value_t temperature = 
{
  .attribute =
  {
    .handle       = BLE_INVALID_GATT_HANDLE,
    .uuid_length  = BLE_GATT_UUID_LENGTH,
    .uuid         = {0x01, 0xe1},
    .value_length = BLE_TEMPERATURE_VALUE_LENGTH,
    .value        = NULL,
  },
  .update =
  {
    .type     = BLE_CHAR_UPDATE_READ,
    .timer    = 0,
    .callback = ble_update_temperature,
  },
};


static void ble_update_temperature (void *data)
{
  int i;
  ble_char_list_entry_t *characteristics = (ble_char_list_entry_t *)data;

  printf ("Temperature value ");
  for (i = (characteristics->data.value_length - 1); i >= 0; i--)
  {
    printf ("%02x", characteristics->data.value[i]);
  }
  printf ("\n");

  characteristics->update.timer = BLE_TEMPERATURE_MEAS_INTERVAL;
}

int ble_lookup_uuid (ble_char_list_entry_t *characteristics)
{
  int found = 0;
  ble_attribute_t *data = &(characteristics->data);

  if ((data->uuid_length == temperature.attribute.uuid_length) &&
      ((memcmp (data->uuid, temperature.attribute.uuid, data->uuid_length)) == 0))
  {
    temperature.attribute.handle = data->handle;
    
    data->value_length = temperature.attribute.value_length;
    data->value        = malloc (data->value_length);
    
    characteristics->update = temperature.update;
    found = 1;
  }

  return found;
}

