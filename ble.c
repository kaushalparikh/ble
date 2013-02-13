
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "serial.h"
#include "cmd_def.h"
#include "ble.h"

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
  ble_message_header_t  header;
  uint8                 *data;
} ble_message_t;

/* Reset message */
typedef struct PACKED
{
  ble_message_header_t header;
} ble_command_reset_t;

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


static void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2);


int ble_init (void)
{
  int status = 0;
  int init_attempt = 0;
  
  /* Find BLE device and initialize */
  status = serial_init ();
  sleep (1);

  if (status == 0)
  {
    /* BLE TX function pointer */
    bglib_output = ble_output;

    printf ("BLE Reset request\n");

      /* Reset BLE */
    ble_cmd_system_reset (0);
    sleep (1);

    /* Close & re-open UART after reset */
    serial_deinit ();

    do
    {
      init_attempt++;
      sleep (1);
      
      status = serial_init ();
    } while ((status < 0) && (init_attempt < 2));

    if (status == 0)
    {
      /* Ping BLE */
      status = ble_hello ();
    }
    else
    {
      printf ("BLE Reset request failed\n");
      bglib_output = NULL;
    }

    if (status < 0)
    {
      serial_deinit ();
    }
  }
  else
  {
    printf ("Can't find BLE device\n");
  }

  return status;
}

void ble_deinit (void)
{
}

void ble_output (uint8 bytes1, uint8 * buffer1, uint16 bytes2, uint8 * buffer2)
{
  (void)serial_tx (bytes1, buffer1);
  (void)serial_tx (bytes2, buffer2);
}

int ble_receive_message (struct ble_header *rsp_header, unsigned char *rsp_buffer)
{
  struct ble_header header;
  unsigned char buffer[256];
  int status;

  status = serial_rx (sizeof(header), (unsigned char *)&header);
  if (status > 0)
  {
    if (header.lolen > 0)
    {
      status = serial_rx (header.lolen, buffer);
    }

    if (rsp_header != NULL)
    {
      /* Response provided, match it and if successful
         copy the response */
      if ((rsp_header->type_hilen == header.type_hilen) &&
          (rsp_header->lolen == header.lolen)           &&
          (rsp_header->cls == header.cls)               &&
          (rsp_header->command == header.command))
      {
        if (rsp_header->lolen > 0)
        {
          memcpy (rsp_buffer, buffer, rsp_header->lolen);
        }
      }
      else
      {
        status = -1;
      }
    }
    else if (status > 0)
    {
      struct ble_msg * message = (struct ble_msg *)ble_get_msg_hdr (header);
    
      if (message != NULL)
      {
        message->handler (buffer);
      }
      else
      {
        status = -1;
      }
    }
  }

  return status;
}

int ble_scan (void)
{
  int status;
  struct ble_header rsp_header;
  short int result;

  printf ("BLE Scan request\n");

  ble_cmd_gap_discover (gap_discover_observation);

  rsp_header.type_hilen = (uint8)ble_dev_type_ble|(uint8)ble_msg_type_rsp|0x0;
  rsp_header.lolen      = 2;
  rsp_header.cls        = ble_cls_gap;
  rsp_header.command    = ble_cmd_gap_discover_id;
  
  status = ble_receive_message (&rsp_header, (unsigned char *)&result);
  if (status > 0)
  {
    if (result > 0)
    {
      printf ("BLE Scan response received with failure %d\n", result);
      status = -1;
    }
  }
  else
  {
    printf ("BLE Hello response failed with %d\n", status);
  }

  return status;
}

int ble_hello (void)
{
  int status;
  ble_command_reset_t reset;
  struct ble_header rsp_header;

  printf ("BLE Hello request\n");

  BLE_CLASS_SYSTEM_HEADER (&reset, BLE_COMMAND_HELLO);
  serial_tx (sizeof (reset), (unsigned char *)(&reset));

  rsp_header.type_hilen = (uint8)ble_dev_type_ble|(uint8)ble_msg_type_rsp|0x0;
  rsp_header.lolen      = 0;
  rsp_header.cls        = ble_cls_system;
  rsp_header.command    = ble_cmd_system_hello_id;
  
  status = ble_receive_message (&rsp_header, NULL);
  if (status <= 0)
  {
    printf ("BLE Hello response failed with %d\n", status);
    status = -1;
  }

  return status;
}

