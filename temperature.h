#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

#include "types.h"
#include "profile.h"
#include "sync.h"

typedef struct
{
  int8   *time;
  float   temperature;
  uint32  battery_level;
} ble_sync_temperature_data_t;

#define BLE_TEMPERATURE_SERVICE_UUID   (0x1809)

extern void ble_sync_temperature (ble_sync_list_entry_t **sync_list);

extern void ble_update_temperature (ble_service_list_entry_t *service_list_entry,
                                    ble_device_list_entry_t *device_list_entry);

extern int32 ble_init_temperature (ble_service_list_entry_t *service_list_entry,
                                   ble_device_list_entry_t *device_list_entry);

#endif

