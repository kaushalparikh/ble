#ifndef __PROFILE_H__
#define __PROFILE_H__

#include "types.h"

#define BLE_PACK_GATT_UUID(byte)  (((byte)[1] << 8) | (byte)[0])
#define BLE_UNPACK_GATT_UUID(uuid, byte)  { (byte)[0] = ((uuid) & 0xff); (byte)[1] = (((uuid) & 0xff00) >> 8); } 

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
  BLE_ATTR_TYPE_BROADCAST    = 0x01,
  BLE_ATTR_TYPE_READ         = 0x02,
  BLE_ATTR_TYPE_WRITE_NO_RSP = 0x04,
  BLE_ATTR_TYPE_WRITE        = 0x08,
  BLE_ATTR_TYPE_NOTIFY       = 0x10,
  BLE_ATTR_TYPE_INDICATE     = 0x20,
  BLE_ATTR_TYPE_WRITE_SIGNED = 0x40,
  BLE_ATTR_TYPE_EXT          = 0x80
};

typedef struct PACKED
{
  uint8  type;
  uint16 handle;
  uint8  uuid[BLE_MAX_UUID_LENGTH];
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

typedef struct
{
  uint8   type;
  uint16  handle;
  uint8   uuid_length;
  uint8   uuid[BLE_MAX_UUID_LENGTH];
  uint8   data_length;
  uint8  *data;  
} ble_attribute_t;

struct ble_char_list_entry
{
  struct ble_char_list_entry *next;
  ble_attribute_t            *declaration;
  ble_attribute_t            *value;
  ble_attribute_t            *description;
  ble_attribute_t            *client_config;
  ble_attribute_t            *format;
};

typedef struct ble_char_list_entry ble_char_list_entry_t;

typedef struct
{
  ble_char_list_entry_t  *char_list;
  uint8                   init;
  int32                   time;
  int32                   time_offset;
  int32                   wait;
  int32                   interval;
} ble_service_update_t;

struct ble_service_list_entry
{
  struct ble_service_list_entry *next;
  ble_attribute_t               *declaration;
  uint16                         start_handle;
  uint16                         end_handle;
  struct ble_service_list_entry *include_list;
  ble_char_list_entry_t         *char_list;
  ble_service_update_t           update;
};

typedef struct ble_service_list_entry ble_service_list_entry_t;

/* BLE device address */
#define BLE_DEVICE_ADDRESS_LENGTH  (6)

/* BLE device address type */
enum
{
  BLE_ADDR_PUBLIC = 0,
  BLE_ADDR_RANDOM = 1
};

typedef struct PACKED
{
  uint8 byte[BLE_DEVICE_ADDRESS_LENGTH];
  uint8 type;  
} ble_device_address_t;

/* BLE device status */
typedef enum
{
  BLE_DEVICE_DISCOVER           = 0,
  BLE_DEVICE_DISCOVER_SERVICE   = 1,
  BLE_DEVICE_DISCOVER_CHAR_DESC = 2,
  BLE_DEVICE_DISCOVER_CHAR      = 3,
  BLE_DEVICE_CONFIGURE_CHAR     = 4,
  BLE_DEVICE_DATA               = 5,
  BLE_DEVICE_IGNORE             = 6
} ble_device_status_e;

/* BLE device */
typedef struct
{
} ble_device_t;

struct ble_device_list_entry
{
  struct ble_device_list_entry  *next;
  ble_device_address_t           address;
  int8                          *name;
  ble_service_list_entry_t      *service_list;
  ble_device_status_e            status;
  void                          *data;
};

typedef struct ble_device_list_entry ble_device_list_entry_t;

extern void ble_update_char_type (ble_char_list_entry_t * char_list_entry, uint8 type);

extern ble_attribute_t * ble_find_attribute (ble_service_list_entry_t *service_list_entry,
                                             uint16 handle);

extern void ble_print_service (ble_service_list_entry_t *service_list_entry);

extern void ble_update_service (ble_service_list_entry_t *service_list_entry,
                                ble_device_list_entry_t *device_list_entry);

extern int32 ble_init_service (ble_service_list_entry_t *service_list_entry,
                               ble_device_list_entry_t *device_list_entry);

extern ble_service_list_entry_t * ble_find_service (ble_service_list_entry_t *service_list_entry,
                                                    uint8 *uuid, uint8 uuid_length);

extern void ble_clear_service (ble_service_list_entry_t *service_list_entry);

#endif

