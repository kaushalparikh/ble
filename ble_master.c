
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ble.h"

typedef enum
{
  BLE_STATE_WAKEUP   = 0,
  BLE_STATE_SCAN     = 1,
  BLE_STATE_PROFILE  = 2,
  BLE_STATE_DATA     = 3,
  BLE_STATE_MAX      = 4
} ble_state_e;

typedef ble_state_e (*ble_state_handler_t)(ble_message_t *message);

static ble_state_e ble_wakeup (ble_message_t *message);
static ble_state_e ble_scan (ble_message_t *message);
static ble_state_e ble_profile (ble_message_t *message);
static ble_state_e ble_data (ble_message_t *message);

static ble_state_e ble_state = BLE_STATE_WAKEUP;

static ble_state_handler_t state_handler[BLE_STATE_MAX] =
{
  ble_wakeup,
  ble_scan,
  ble_profile,
  ble_data
};

static ble_state_e ble_wakeup (ble_message_t *message)
{
  ble_state_e new_state = BLE_STATE_SCAN;
  
  /* Only accept wakeup timer expiry messages */
  if ((message->header.type == BLE_EVENT) &&
      (message->header.command == BLE_EVENT_SOFT_TIMER))
  {
    if ((ble_scan_start ()) <= 0)
    {
      /* Scan start failed, try again after sometime */
      new_state = BLE_STATE_WAKEUP;
    }
    
    /* Flush rest of message, both from serial & timer */
    ble_flush_timer ();
    ble_flush_serial ();    
  }

  return new_state;
}

static ble_state_e ble_scan (ble_message_t *message)
{
  return BLE_STATE_PROFILE;
}

static ble_state_e ble_profile (ble_message_t *message)
{
  (void)ble_set_timer (30000, BLE_TIMER_WAKEUP);
  return BLE_STATE_WAKEUP;
}

static ble_state_e ble_data (ble_message_t *message)
{
  return BLE_STATE_WAKEUP;
}

void master_loop (void)
{
  (void)ble_set_timer (10, BLE_TIMER_WAKEUP);

  while (1)
  {
    if (((ble_check_serial ()) > 0) || ((ble_check_timer ()) > 0))
    {
      int pending;
      ble_message_t message;

      while ((pending = ble_receive_timer (&message)) > 0)
      {
        ble_state = state_handler[ble_state](&message);
      }

      while ((pending = ble_receive_serial (&message)) > 0)
      {
        ble_state = state_handler[ble_state](&message);
      }
    }
  }
}

int main (int argc, char * argv[])
{
  int status = -1;
  int init_attempt = 0;

  do
  {
    init_attempt++;
    printf ("BLE init attempt %d\n", init_attempt);
    
    status = ble_init ();
  } while ((status <= 0) && (init_attempt < 2));

  master_loop ();

  ble_deinit ();

  return 0;
}

