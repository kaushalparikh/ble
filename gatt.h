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

typedef struct
{
  uint16  handle;
  uint8   uuid_length;
  uint8   uuid[BLE_MAX_UUID_LENGTH];
  uint8   value_length;
  uint8  *value;
  uint8   permission;
} ble_attribute_t;

struct ble_characteristics
{
  struct ble_characteristics *next;
  ble_attribute_t             declaration;
  ble_attribute_t             description;
  ble_attribute_t             client_config;
  ble_attribute_t             value;
  ble_attribute_t             format;
};

typedef struct ble_characteristics ble_characteristics_t;

struct ble_service
{
  struct ble_service    *next;
  struct ble_service    *include_list;
  ble_characteristics_t *char_list;
  uint16                 start_handle;
  uint16                 end_handle;
  ble_attribute_t        attribute;
};

typedef struct ble_service ble_service_t;

#endif

