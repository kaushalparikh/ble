#ifndef __GATT_H__
#define __GATT_H__

#include "ble_types.h"

#define BLE_MAX_UUID_LENGTH  (16)

#define BLE_GATT_UUID_LENGTH  (2)

#define BLE_INVALID_GATT_HANDLE (0x0000)
#define BLE_MIN_GATT_HANDLE     (0x0001)
#define BLE_MAX_GATT_HANDLE     (0xffff)

enum
{
  BLE_GATT_PRI_SERVICE        = 0x2800,
  BLE_GATT_SEC_SERVICE        = 0x2801,
  BLE_GATT_INCLUDE            = 0x2802,
  BLE_GATT_CHAR_DECL          = 0x2803,
  BLE_GATT_CHAR_EXT           = 0x2900,
  BLE_GATT_CHAR_USER_DESC     = 0x2901,
  BLE_GATT_CHAR_CLIENT_CONFIG = 0x2902,
  BLE_GATT_CHAR_SERVER_CONFIG = 0x2903,
  BLE_GATT_CHAR_FORMAT        = 0x2904,
  BLE_GATT_CHAR_AGG_FORMAT    = 0x2905,
  BLE_GATT_CHAR_VALID_RANGE   = 0x2906
};

enum
{
  BLE_CHAR_PROPERTY_BROADCAST    = 0x01,
  BLE_CHAR_PROPERTY_READ         = 0x02,
  BLE_CHAR_PROPERTY_WRITE_NO_RSP = 0x04,
  BLE_CHAR_PROPERTY_WRITE        = 0x08,
  BLE_CHAR_PROPERTY_NOTIFY       = 0x10,
  BLE_CHAR_PROPERTY_INDICATE     = 0x20,
  BLE_CHAR_PROPERTY_WRITE_SIGNED = 0x40,
  BLE_CHAR_PROPERTY_EXT          = 0x80
};

typedef struct PACKED
{
  uint8  properties;
  uint16 value_handle;
  uint8  value_uuid[BLE_MAX_UUID_LENGTH];
} ble_char_decl_t;

enum
{
  BLE_CHAR_CLIENT_NOTIFY   = 0x0001,
  BLE_CHAR_CLIENT_INDICATE = 0x0002
};

typedef struct PACKED
{
  uint16 bitfield;
} ble_char_client_config_t;

typedef struct PACKED
{
  uint8  bitfield;
  uint8  exponent;
  uint16 uint;
  uint8  namespace;
  uint16 description;
} ble_char_format_t;

enum
{
  BLE_CHAR_READ_DATA           = 0x01,
  BLE_CHAR_NOTIFY_DATA         = 0x02,
  BLE_CHAR_INDICATE_DATA       = 0x04,
  BLE_CHAR_WRITE_DATA          = 0x08
};

typedef struct
{
  uint8   type;
  int32   timer;
  void  (*callback)(int32 ble_device_id, void *data);
} ble_char_update_t;

struct ble_attr_list_entry
{
  struct ble_attr_list_entry *next;
  uint16                      handle;
  uint8                       uuid_length;
  uint8                       uuid[BLE_MAX_UUID_LENGTH];
  uint8                       value_length;
  uint8                       *value;
};

typedef struct ble_attr_list_entry ble_attr_list_entry_t;

struct ble_char_list_entry
{
  struct ble_char_list_entry *next;
  ble_attr_list_entry_t      *desc_list;
  ble_char_update_t           update;
};

typedef struct ble_char_list_entry ble_char_list_entry_t;

struct ble_service_list_entry
{
  struct ble_service_list_entry *next;
  struct ble_service_list_entry *include_list;
  ble_char_list_entry_t         *char_list;
  ble_attr_list_entry_t          declaration;
  uint16                         start_handle;
  uint16                         end_handle;
};

typedef struct ble_service_list_entry ble_service_list_entry_t;

extern int32 ble_lookup_uuid (ble_char_list_entry_t *characteristics);

extern uint32 ble_identify_device (uint8 *address, ble_char_list_entry_t *update_list);

#endif

