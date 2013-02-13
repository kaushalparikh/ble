#ifndef __BLE_H__
#define __BLE_H__

#include "cmd_def.h"

extern int ble_init (void);

extern void ble_deinit (void);

extern int ble_receive_message (struct ble_header *rsp_header, unsigned char *rsp_buffer);

extern int ble_scan (void);

extern int ble_hello (void);

#endif

