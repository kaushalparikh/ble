#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

#include "types.h"
#include "profile.h"

#define BLE_TEMPERATURE_SERVICE_UUID   (0x1809)

extern void ble_update_temperature (ble_service_list_entry_t *service_list_entry, int8 *device_name,
                                    void *device_data);

extern int32 ble_init_temperature (ble_service_list_entry_t *service_list_entry, int8 *device_name,
                                   db_info_t *db_info, void **device_data);
#endif

