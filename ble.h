#ifndef __BLE_H__
#define __BLE_H__

#include "apitypes.h"

/* Maximum message data size */
#define BLE_MAX_MESSAGE  (256)

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

extern int ble_init (void);

extern void ble_deinit (void);

extern int ble_receive_message (ble_message_t *message);

extern int ble_send_message (ble_message_t *message);

extern int ble_scan (void);

extern int ble_hello (void);

extern int ble_reset (void);

#endif

