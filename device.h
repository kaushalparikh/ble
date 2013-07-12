#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "profile.h"

extern void ble_print_device (ble_device_list_entry_t *device_list_entry);

extern void ble_update_device (ble_device_list_entry_t *device_list_entry);

extern ble_device_list_entry_t * ble_find_device (ble_device_list_entry_t *device_list_entry,
                                                  ble_device_address_t *address);

extern void ble_get_device (ble_device_list_entry_t **device_list);

#endif

