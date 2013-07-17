#ifndef __SYNC_H__
#define __SYNC_H__

#include "types.h"

enum
{
  BLE_SYNC_PUSH = 0,
  BLE_SYNC_PULL
};

enum
{
  BLE_SYNC_DEVICE = 0,
  BLE_SYNC_TEMPERATURE
};

struct ble_sync_list_entry
{
  struct ble_sync_list_entry *next;
  uint8                       type;
  uint8                       data_type;
  void                       *data;
};

typedef struct ble_sync_list_entry ble_sync_list_entry_t;

extern void * ble_sync (void *timeout);

#endif

