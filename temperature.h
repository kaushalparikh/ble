#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

#include "types.h"
#include "profile.h"

#define BLE_TEMPERATURE_SERVICE_UUID   (0x1809)

extern void ble_update_temperature (void *device_list_entry);

extern int32 ble_init_temperature (ble_device_list_entry_t *device_list_entry, db_info_t *db_info);

#endif

