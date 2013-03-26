#ifndef __GATT_H__
#define __GATT_H__

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
  BLE_GATT_CHAR_AGG_FORMAT    = 0x2905
};

typedef struct PACKED
{
  uint8  bitfield;
  uint16 value_handle;
  uint8  value_uuid[BLE_MAX_UUID_LENGTH];
} ble_char_decl_t;

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

typedef struct
{
  uint16  handle;
  uint8   uuid_length;
  uint8   uuid[BLE_MAX_UUID_LENGTH];
  uint8   value_length;
  uint8  *value;
} ble_attribute_t;

enum
{
  BLE_CHAR_UPDATE_READ  = 0x01,
  BLE_CHAR_UPDATE_WRITE = 0x02
};

typedef struct
{
  uint8   type;
  int32   timer;
  void  (*callback)(void *data);  
} ble_char_update_t;

struct ble_char_list_entry
{
  struct ble_char_list_entry *next;
  ble_attribute_t             declaration;
  ble_attribute_t             description;
  ble_attribute_t             client_config;
  ble_attribute_t             format;
  ble_attribute_t             data;
  ble_char_update_t           update;
};

typedef struct ble_char_list_entry ble_char_list_entry_t;

struct ble_service_list_entry
{
  struct ble_service_list_entry *next;
  struct ble_service_list_entry *include_list;
  ble_char_list_entry_t         *char_list;
  ble_attribute_t                declaration;
  uint16                         start_handle;
  uint16                         end_handle;
};

typedef struct ble_service_list_entry ble_service_list_entry_t;

extern int ble_lookup_uuid (ble_char_list_entry_t *characteristics);

#endif

