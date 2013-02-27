
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ble.h"

typedef enum
{
  BLE_STATE_SCAN     = 0,
  BLE_STATE_PROFILE  = 1,
  BLE_STATE_DATA     = 2,
  BLE_STATE_MAX      = 3
} ble_state_e;

typedef ble_state_e (*ble_state_handler_t)(ble_message_t *message);

static ble_state_e ble_scan (ble_message_t *message);
static ble_state_e ble_profile (ble_message_t *message);
static ble_state_e ble_data (ble_message_t *message);

static ble_state_e ble_state = BLE_STATE_SCAN;

static ble_state_handler_t state_handler[BLE_STATE_MAX] =
{
  ble_scan,
  ble_profile,
  ble_data
};


static ble_state_e ble_scan (ble_message_t *message)
{
  ble_state_e new_state = BLE_STATE_SCAN;

  /* Only accept GAP event or timer messages */
  if (message->header.type == BLE_EVENT)
  {
    switch (message->header.command)
    {
      case BLE_EVENT_SCAN_RESPONSE:
      {
        ble_event_scan_response ((ble_event_scan_response_t *)(message->data));
        break;
      }
      case BLE_EVENT_SOFT_TIMER:
      {
        if (message->data[0] == BLE_TIMER_SCAN)
        {
          if ((ble_scan_start ()) > 0)
          {
            (void)ble_set_timer (BLE_SCAN_DURATION, BLE_TIMER_SCAN_STOP);
          }
          else
          {
            /* TODO: try again after sometime */
          }
          
          /* Flush rest of message, both from serial & timer */
          ble_flush_timer ();
          ble_flush_serial ();    
        }
        else if (message->data[0] == BLE_TIMER_SCAN_STOP)
        {
          if ((ble_end_procedure ()) > 0)
          {
            /* Setup timer to move to profile state */
            (void)ble_set_timer (10, BLE_TIMER_PROFILE);

            /* Scan stopped, change state to profile */
            new_state = BLE_STATE_PROFILE;
          }
          else
          {
            /* TODO: Scan stop failed, set timer to stop sometime later */
          }
        }
        
        break;
      }
      default:
      {
        ble_print_message (message);
        break;
      }
    }
  }
  else
  {
    ble_print_message (message);
  }

  return new_state;
}

static ble_state_e ble_profile (ble_message_t *message)
{
  (void)ble_set_timer (30000, BLE_TIMER_SCAN);

  return BLE_STATE_SCAN;
}

static ble_state_e ble_data (ble_message_t *message)
{
  return BLE_STATE_SCAN;
}

void master_loop (void)
{
  (void)ble_set_timer (10, BLE_TIMER_SCAN);

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

