
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

static ble_state_e ble_profile_new_state (void)
{
  ble_state_e new_state;
  
  /* Exit profile state */
  if ((ble_check_data_list ()) > 0)
  {
    (void)ble_set_timer (10, BLE_TIMER_DATA);
    new_state = BLE_STATE_DATA;
  }
  else
  {
    (void)ble_set_timer (3000, BLE_TIMER_SCAN);
    new_state = BLE_STATE_SCAN;
  }

  return new_state;
}

static ble_state_e ble_scan (ble_message_t *message)
{
  ble_state_e new_state = BLE_STATE_SCAN;

  /* Only accept GAP event or timer messages */
  if (message->header.type == BLE_EVENT)
  {
    switch ((message->header.class << 8)|message->header.command)
    {
      case ((BLE_CLASS_GAP << 8)|BLE_EVENT_SCAN_RESPONSE):
      {
        ble_event_scan_response ((ble_event_scan_response_t *)message);
        break;
      }
      case ((BLE_CLASS_HW << 8)|BLE_EVENT_SOFT_TIMER):
      {
        if (message->data[0] == BLE_TIMER_SCAN)
        {
          printf ("BLE Scan state\n");

          if ((ble_start_scan ()) <= 0)
          {
            /* TODO: try again after sometime */
          }
          
          /* Flush rest of message, both from serial & timer */
          ble_flush_timer ();
          ble_flush_serial ();    
        }
        else if (message->data[0] == BLE_TIMER_SCAN_STOP)
        {
          if ((ble_end_procedure ()) <= 0)
          {
            /* TODO: Scan stop failed, set timer to stop sometime later */
          }
          else
          {
            /* Check if some devices need service discovery/update */
            if ((ble_check_profile_list ()) > 0)
            {
              (void)ble_set_timer (10, BLE_TIMER_PROFILE);
              new_state = BLE_STATE_PROFILE;
            }
            else if ((ble_check_data_list ()) > 0)
            {
              (void)ble_set_timer (10, BLE_TIMER_DATA);
              new_state = BLE_STATE_DATA;
            }
            else
            {
              (void)ble_set_timer (10, BLE_TIMER_SCAN);
            }
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
  ble_state_e new_state = BLE_STATE_PROFILE;

  /* Only accept GAP event or timer messages */
  if (message->header.type == BLE_EVENT)
  {
    switch ((message->header.class << 8)|message->header.command)
    {
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_STATUS):
      {
        if ((ble_event_connection_status ((ble_event_connection_status_t *)message)) <= 0)
        {
          new_state = ble_profile_new_state ();
        }
        
        break;
      }
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_DISCONNECTED):
      {
        if ((ble_next_profile ((ble_event_disconnect_t *)message)) <= 0)
        {
          new_state = ble_profile_new_state ();
        }
        
        break;
      }
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_GROUP_FOUND):
      {
        ble_event_read_group ((ble_event_read_group_t *)message);
        break;
      }
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_INFORMATION_FOUND):
      {
        ble_event_find_information ((ble_event_find_information_t *)message);
        break;
      }      
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_ATTR_CLIENT_VALUE):
      {
        ble_event_attr_value ((ble_event_attr_value_t *)message);
        break;
      }      

      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_PROCEDURE_COMPLETED):
      {
        if ((ble_event_procedure_completed ((ble_event_procedure_completed_t *)message)) <= 0)
        {
          new_state = ble_profile_new_state ();
        }
        
        break;
      }
      case ((BLE_CLASS_HW << 8)|BLE_EVENT_SOFT_TIMER):
      {
        if (message->data[0] == BLE_TIMER_PROFILE)
        {
          printf ("BLE Profile state\n");
          if ((ble_start_profile ()) <= 0)
          {
            new_state = ble_profile_new_state ();
          }
          
          /* Flush rest of message, both from serial & timer */
          ble_flush_timer ();
          ble_flush_serial ();    
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

static ble_state_e ble_data (ble_message_t *message)
{
  ble_flush_timer ();
  ble_flush_serial ();    
  printf ("BLE Data state\n");
  (void)ble_set_timer (3000, BLE_TIMER_SCAN);
  return BLE_STATE_SCAN;
}

void master_loop (void)
{
  (void)ble_set_timer (10, BLE_TIMER_SCAN);

  while (1)
  {
    int pending;
    ble_message_t message;

    if ((ble_check_timer ()) > 0)
    {
      do
      {
        pending = ble_receive_timer (&message);
        ble_state = state_handler[ble_state](&message);
      } while (pending > 0);
    }
    
    if ((ble_check_serial ()) > 0)
    {
      do
      {
        pending = ble_receive_serial (&message);
        ble_state = state_handler[ble_state](&message);
      } while (pending > 0);
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

