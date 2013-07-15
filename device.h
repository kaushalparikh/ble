#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "types.h"
#include "profile.h"
#include "sync.h"

typedef struct
{
  uint8   address[BLE_DEVICE_ADDRESS_LENGTH];
  int8   *name;
  uint8   service[BLE_MAX_UUID_LENGTH];
  int32   interval;
  int8   *status;
} ble_sync_device_data_t;

extern void ble_sync_device (ble_sync_list_entry_t *sync_list_entry);

extern void ble_print_device (ble_device_list_entry_t *device_list_entry);

extern void ble_update_device (ble_device_list_entry_t *device_list_entry);

extern ble_device_list_entry_t * ble_find_device (ble_device_list_entry_t *device_list_entry,
                                                  ble_device_address_t *address);

extern void ble_init_device_list (ble_device_list_entry_t **device_list);

extern void ble_update_device_list (ble_device_list_entry_t **device_list);

#endif

