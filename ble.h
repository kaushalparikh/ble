#ifndef __BLE_H__
#define __BLE_H__

/* Maximum message data size */
#define BLE_MAX_MESSAGE  (256)

/* Convert MS to intervals of 625us */
#define MS_TO_625US(millisec)  (((millisec * 1000) + 624)/625)

/* Convert MS to intervals of 1.25ms or 1250us */
#define MS_TO_1250US(millisec)  (((millisec * 1000) + 1249)/1250)

/* Convert MS to intervals of 10ms */
#define MS_TO_10MS(millisec)  ((millisec + 9)/10)

/* Basic definitions */
#ifdef __GNUC__
#define PACKED __attribute__((packed))
#define ALIGNED __attribute__((aligned(0x4)))
#else
#define PACKED
#define ALIGNED
#endif

typedef unsigned char      uint8;
typedef signed char        int8;
typedef unsigned short int uint16;
typedef signed short int   int16;
typedef unsigned long int  uint32;

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
  BLE_DEVICE_LOST           = 0,
  BLE_DEVICE_UPDATE_PROFILE = 1,
  BLE_DEVICE_UPDATE_DATA    = 2
} ble_device_status_e;

/* BLE device */
typedef struct
{
  ble_device_address_t  address;
  char                 *name;
  ble_device_status_e   status;
} ble_device_t;

/* Message header definitions */
/* Message types */
enum
{
  BLE_COMMAND  = 0,
  BLE_RESPONSE = 0,
  BLE_EVENT    = 0x80
};

/* Command/Response/Event class */
enum
{
  BLE_CLASS_SYSTEM        = 0,
  BLE_CLASS_PARAMETER     = 1,
  BLE_CLASS_ATTR_DATABASE = 2,
  BLE_CLASS_CONNECTION    = 3,
  BLE_CLASS_ATTR_CLIENT   = 4,
  BLE_CLASS_SECURITY      = 5,
  BLE_CLASS_GAP           = 6,
  BLE_CLASS_HW            = 7,
  BLE_CLASS_TEST          = 8
};

/* System command types */
enum
{
  BLE_COMMAND_RESET            = 0,
  BLE_COMMAND_HELLO            = 1,
  BLE_COMMAND_GET_ADDRESS      = 2,
  BLE_COMMAND_WRITE_REG        = 3,
  BLE_COMMAND_READ_REG         = 4,
  BLE_COMMAND_GET_COUNTERS     = 5,
  BLE_COMMAND_GET_CONNECTIONS  = 6,
  BLE_COMMAND_READ_MEM         = 7,
  BLE_COMMAND_GET_INFO         = 8,
  BLE_COMMAND_TX_ENDPOINT      = 9,
  BLE_COMMAND_APPEND_WHITELIST = 10,
  BLE_COMMAND_REMOVE_WHITELIST = 11,
  BLE_COMMAND_CLEAR_WHITELIST  = 12
};

/* Persistant store (Parameter) command types */
enum
{
  BLE_COMMAND_DEFRAG_PARAMS    = 0,
  BLE_COMMAND_DUMP_PARAMS      = 1,
  BLE_COMMAND_ERASE_ALL_PARAMS = 2,
  BLE_COMMAND_SAVE_PARAM       = 3,
  BLE_COMMAND_LOAD_PARAM       = 4,
  BLE_COMMAND_ERASE_PARAM      = 5,
  BLE_COMMAND_ERASE_PAGE       = 6,
  BLE_COMMAND_WRITE_PARAM      = 7
};

/* Attribute database command types */
enum
{
  BLE_COMMAND_WRITE_ATTR     = 0,
  BLE_COMMAND_READ_ATTR      = 1,
  BLE_COMMAND_READ_ATTR_TYPE = 2,
  BLE_COMMAND_USER_RESPONSE  = 3
};

/* Connection command types */
enum
{
  BLE_COMMAND_DISCONNECT      = 0,
  BLE_COMMAND_GET_RSSI        = 1,
  BLE_COMMAND_UPDATE          = 2,
  BLE_COMMAND_UPDATE_VERSION  = 3,
  BLE_COMMAND_GET_CHANNEL_MAP = 4,
  BLE_COMMAND_SET_CHANNEL_MAP = 5,
  BLE_COMMAND_GET_FEATURES    = 6,
  BLE_COMMAND_GET_STATUS      = 7,
  BLE_COMMAND_RAW_TX          = 8
};

/* Attribute client command types */
enum
{
  BLE_COMMAND_FIND_BY_TYPE_VALUE = 0,
  BLE_COMMAND_READ_BY_GROUP_TYPE = 1,
  BLE_COMMAND_READ_BY_TYPE       = 2,
  BLE_COMMAND_FIND_INFORMATION   = 3,
  BLE_COMMAND_READ_BY_HANDLE     = 4,
  BLE_COMMAND_WRITE_ATTR_CLIENT  = 5,
  BLE_COMMAND_WRITE_COMMAND      = 6,
  BLE_COMMAND_RESERVED           = 7,
  BLE_COMMAND_READ_LONG          = 8,
  BLE_COMMAND_PREPARE_WRITE      = 9,
  BLE_COMMAND_EXECUTE_WRITE      = 10,
  BLE_COMMAND_READ_MULTIPLE      = 11
};

/* Security manager command types */
enum
{
  BLE_COMMAND_START_ENCRYPT = 0,
  BLE_COMMAND_SET_BONDABLE_MODE = 1,
  BLE_COMMAND_DELETE_BONDING    = 2,
  BLE_COMMAND_SET_PARAMETERS    = 3,
  BLE_COMMAND_PASSKEY_ENTRY     = 4,
  BLE_COMMAND_GET_BONDS         = 5,
  BLE_COMMAND_SET_OOB_DATA      = 6
};

/* Generic access profile (GAP) command types */
enum
{
  BLE_COMMAND_SET_PRIVACY_FLAGS         = 0,
  BLE_COMMAND_SET_MODE                  = 1,
  BLE_COMMAND_DISCOVER                  = 2,
  BLE_COMMAND_CONNEC_DIRECT             = 3,
  BLE_COMMAND_END_PROCEDURE             = 4,
  BLE_COMMAND_CONNECT_SELECTIVE         = 5,
  BLE_COMMAND_SET_FILTERING             = 6,
  BLE_COMMAND_SET_SCAN_PARAMS           = 7,
  BLE_COMMAND_SET_ADVERT_PARAMS         = 8,
  BLE_COMMAND_SET_ADVDET_DATA           = 9,
  BLE_COMMAND_SET_DIRECTED_CONNECT_MODE = 10
};

/* Hardware command types */
enum
{
  BLE_COMMAND_CONFIG_IO_PORT_IRQ       = 0,
  BLE_COMMAND_SET_SOFT_TIMER           = 1,
  BLE_COMMAND_READ_ADC                 = 2,
  BLE_COMMAND_CONFIG_IO_PORT_DIRECTION = 3,
  BLE_COMMAND_CONFIG_IO_PORT_FUNCITON  = 4,
  BLE_COMMAND_CONFIG_IO_PORT_PULL      = 5,
  BLE_COMMAND_WRITE_IO_PORT            = 6,
  BLE_COMMAND_READ_IO_PORT             = 7,
  BLE_COMMAND_CONFIG_SPI               = 8,
  BLE_COMMAND_SPI_TRANSFER             = 9,
  BLE_COMMAND_READ_I2C                 = 10,
  BLE_COMMAND_WRITE_I2C                = 11,
  BLE_COMMAND_SET_TX_POWER             = 12
};

/* PHY test commmand types */
enum
{
  BLE_COMMAND_PHY_TX              = 0,
  BLE_COMMAND_PHY_RX              = 1,
  BLE_COMMAND_PHY_END             = 2,
  BLE_COMMAND_PHY_RESET           = 3,
  BLE_COMMAND_PHY_GET_CHANNEL_MAP = 4
};

/* System event types */
enum
{
  BLE_EVENT_BOOT = 0,
  BLE_EVENT_DEBUG = 1,
  BLE_EVENT_RX_ENDPOINT = 2
};

/* Persistant store (Parameter) event types */
enum
{
  BLE_EVENT_KEY = 0
};

/* Attribute database event types */
enum
{
  BLE_EVENT_ATTR_VALUE   = 0,
  BLE_EVENT_USER_REQUEST = 1
};

/* Connection event types */
enum
{
  BLE_EVENT_STATUS       = 0,
  BLE_EVENT_VERSION_IND  = 1,
  BLE_EVENT_FEATURE_IND  = 2,
  BLE_EVENT_RAW_RX       = 3,
  BLE_EVENT_DISCONNECTED = 4
};

/* Attribute client event types */
enum
{
  BLE_EVENT_INDICATED              = 0,
  BLE_EVENT_PROCEDURE_COMPLETED    = 1,
  BLE_EVENT_GROUP_FOUND            = 2,
  BLE_EVENT_ATTR_FOUND             = 3,
  BLE_EVENT_INFORMATION_FOUND      = 4,
  BLE_EVENT_ATTR_CLIENT_VALU       = 5,
  BLE_EVENT_MULTIPLE_READ_RESPONSE = 6
};

/* Security manager event types */
enum
{
  BLE_EVENT_SMP_DATA        = 0,
  BLE_EVENT_BONDING_FAIL    = 1,
  BLE_EVENT_PASSKEY_DISPLAY = 2,
  BLE_EVENT_PASSKEY_REQUEST = 3,
  BLE_EVENT_BOND_STATUS     = 4
};

/* Generic access profile (GAP) event types */
enum
{
  BLE_EVENT_SCAN_RESPONSE = 0,
  BLE_EVENT_MODE_CHANGED  = 1
};

/* Hardware event types */
enum
{
  BLE_EVENT_IO_PORT_STATUS = 0,
  BLE_EVENT_SOFT_TIMER     = 1,
  BLE_EVENT_ADC_RESULT     = 2
};

/* Timers */
enum
{
  BLE_TIMER_SCAN         = 0,
  BLE_TIMER_SCAN_STOP    = 1,
  BLE_TIMER_PROFILE      = 2,
  BLE_TIMER_PROFILE_STOP = 3,
  BLE_TIMER_DATA         = 4
};

/* Message header */
typedef struct PACKED
{
  uint8 type;
  uint8 length;
  uint8 class;
  uint8 command;
} ble_message_header_t;

/* Message */
typedef struct PACKED
{
  ble_message_header_t header;
  uint8                data[BLE_MAX_MESSAGE];
} ble_message_t;

/* Command header macros */
#define BLE_COMMAND_HEADER(message, class_id, command_id)                            \
  (message)->header.type    = BLE_COMMAND;                                           \
  (message)->header.length  = (sizeof (*(message))) - (sizeof ((message)->header));  \
  (message)->header.class   = class_id;                                              \
  (message)->header.command = command_id;

#define BLE_CLASS_SYSTEM_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_SYSTEM, command_id)

#define BLE_CLASS_PARAMETER_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_PARAMETER, command_id)

#define BLE_CLASS_ATTR_DATABASE_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_ATTR_DATABASE, command_id)

#define BLE_CLASS_CONNECTION_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_CONNECTION, command_id)

#define BLE_CLASS_ATTR_CLIENT_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_ATTR_CLIENT, command_id)

#define BLE_CLASS_SECURITY_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_SECURITY, command_id)

#define BLE_CLASS_GAP_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_GAP, command_id)

#define BLE_CLASS_HW_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_HW, command_id)

#define BLE_CLASS_TEST_HEADER(message, command_id)  \
  BLE_COMMAND_HEADER(message, BLE_CLASS_TEST, command_id)

/* System reset command definitions */
/* Reset modes */
enum
{
  BLE_RESET_NORMAL = 0,
  BLE_RESET_DFU    = 1
};

/* Reset message */
typedef struct PACKED
{
  ble_message_header_t header;
  uint8                mode;
} ble_command_reset_t;

/* System hello command definitions */
/* Hello message */
typedef struct PACKED
{
  ble_message_header_t header;
} ble_command_hello_t;

/* GAP discover/scan start command definitions */
/* Scan window/interval */
#define BLE_SCAN_WINDOW    MS_TO_625US(200)
#define BLE_SCAN_INTERVAL  MS_TO_625US(1000)

#define BLE_SCAN_DURATION  (10000)

/* Scan modes */
enum
{
  BLE_SCAN_PASSIVE = 0,
  BLE_SCAN_ACTIVE  = 1
};

/* Scan policy */
enum
{
  BLE_SCAN_POLICY_ALL       = 0,
  BLE_SCAN_POLICY_WHITELIST = 1
};

/* Scan duplicate */
enum
{
  BLE_SCAN_DUPLICATE_ALL    = 0,
  BLE_SCAN_DUPLICATE_FILTER = 1
};

/* Advertise policy */
enum
{
  BLE_ADV_POLICY_ALL               = 0,
  BLE_ADV_POLICY_WHITELIST_SCAN    = 1,
  BLE_ADV_POLICY_WHITELIST_CONNECT = 2,
  BLE_ADV_POLICY_WHITELIST_ALL     = 3
};

/* Discover modes */
enum
{
  BLE_DISCOVER_LIMITED     = 0,
  BLE_DISCOVER_GENERIC     = 1,
  BLE_DISCOVER_OBSERVATION = 2
};

/* Advertise packet type */
enum
{
  BLE_ADV_IND             = 0,
  BLE_ADV_DIRECT_IND      = 1,
  BLE_ADV_NON_CONNECT_IND = 2,
  BLE_SCAN_REQ            = 3,
  BLE_SCAN_RSP            = 4,
  BLE_CONNECT_REQ         = 5,
  BLE_ADV_SCAN_IND        = 6
};

/* Advertise data types */
enum
{
  BLE_ADV_FLAGS               = 0x01,
  BLE_ADV_16BIT_UUID_INCOMP   = 0x02,
  BLE_ADV_16BIT_UUID          = 0x03,
  BLE_ADV_32BIT_UUID_INCOMP   = 0x04,
  BLE_ADV_32BIT_UUID          = 0x05,
  BLE_ADV_128BIT_UUID_INCOMP  = 0x06,
  BLE_ADV_128BIT_UUID         = 0x07,
  BLE_ADV_LOCAL_NAME_SHORT    = 0x08,
  BLE_ADV_LOCAL_NAME          = 0x09,
  BLE_ADV_TX_POWER            = 0x0A,
  BLE_ADV_DEVICE_CLASS        = 0x0D,
  BLE_ADV_PAIRING_HASH_C      = 0x0E,
  BLE_ADV_PAIRING_RAND_R      = 0x0F,
  BLE_ADV_SM_TK_VALUE         = 0x10,
  BLE_ADV_SM_OOB_FLAGS        = 0x11,
  BLE_ADV_CONNECT_INTERVAL    = 0x12,
  BLE_ADV_16BIT_SERVICE_UUID  = 0x14,
  BLE_ADV_128BIT_SERVICE_UUID = 0x15,
  BLE_ADV_SERVICE_DATA        = 0x16,
  BLE_ADV_PUBLIC_TARGET_ADDR  = 0x17,
  BLE_ADV_RANDOM_TARGET_ADDR  = 0x18,
  BLE_ADV_APPEARANCE          = 0x19,
  BLE_ADV_INTERVAL            = 0x1A,
  BLE_ADV_MANUFACTURER_DATA   = 0xFF
};

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               interval;
  uint16               window;
  uint8                mode;
} ble_command_scan_params_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               result;  
} ble_response_scan_params_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                scan_policy;
  uint8                adv_policy;
  uint8                scan_duplicate;
} ble_command_set_filtering_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               result;
} ble_response_set_filtering_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                mode;
} ble_command_discover_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               result;  
} ble_response_discover_t;

typedef struct PACKED
{
  uint8 length;
  uint8 type;
  uint8 value[];
} ble_adv_data_t;

typedef struct PACKED
{
  int8                 rssi;
  uint8                packet_type;
  ble_device_address_t device_address;
  uint8                bond;
  uint8                length;
  uint8                data[];
} ble_event_scan_response_t;

/* GAP end procedure definitions */
typedef struct PACKED
{
  ble_message_header_t header;
} ble_command_end_procedure_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               result;
} ble_response_end_procedure_t;

/* GAP connect command definitions */
/* Connection interval, timeout, latency */
#define BLE_MIN_CONNECT_INTERVAL  MS_TO_1250US(10)
#define BLE_MAX_CONNECT_INTERVAL  MS_TO_1250US(20)

#define BLE_CONNECT_TIMEOUT  MS_TO_10MS(1000)

#define BLE_CONNECT_LATENCY  (0)

/* Connection status flags */
enum
{
  BLE_CONNECT_STARTED   = 1,
  BLE_CONNECT_ENCRYPTED = 2,
  BLE_CONNECT_COMPLETED = 4,
  BLE_CONNECT_UPDATED   = 8
};

/* Disconnection cause */
enum
{
  BLE_DISCONNECT_USER = 0
};

/* Connection definitions */
typedef struct PACKED
{
  ble_message_header_t header;
  ble_device_address_t address;
  uint16               min_interval;
  uint16               max_interval;
  uint16               timeout;
  uint16               latency;
} ble_command_connect_direct_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint16               result;
  uint8                handle;
} ble_response_connect_direct_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                handle;
  uint8                flags;
  ble_device_address_t address;
  uint16               interval;
  uint16               timeout;
  uint16               latency;
  uint8                bonding;
} ble_event_connection_status_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                handle;
} ble_command_disconnect_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                handle;
  uint16               result;
} ble_response_disconnect_t;

typedef struct PACKED
{
  ble_message_header_t header;
  uint8                handle;
  uint16               cause;
} ble_event_connection_disconnect_t;

/* Function declarations */

extern int ble_set_timer (int millisec, int cause);

extern int ble_check_timer (void);

extern int ble_receive_timer (ble_message_t *message);

extern void ble_flush_timer (void);
  
extern int ble_init (void);

extern void ble_deinit (void);

extern int ble_check_device_status (ble_device_status_e status);

extern void ble_print_message (ble_message_t *message);

extern int ble_check_serial (void);

extern int ble_receive_serial (ble_message_t *message);

extern void ble_flush_serial (void);

extern int ble_start_scan (void);

extern void ble_event_scan_response (ble_event_scan_response_t *scan_response);

extern int ble_end_procedure (void);

extern int ble_start_profile (void);

#endif

