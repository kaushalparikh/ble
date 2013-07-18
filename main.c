
#include <stdio.h>

#include "types.h"
#include "util.h"
#include "ble.h"
#include "sync.h"

typedef enum
{
  BLE_STATE_SCAN = 0,
  BLE_STATE_PROFILE,
  BLE_STATE_DATA,
  BLE_STATE_MAX
} ble_state_e;

typedef ble_state_e (*ble_state_handler_t)(ble_message_t *message);

static ble_state_e ble_scan (ble_message_t *message);
static ble_state_e ble_profile (ble_message_t *message);
static ble_state_e ble_data (ble_message_t *message);

static ble_state_e ble_state = BLE_STATE_SCAN;

static ble_state_handler_t ble_state_handler[BLE_STATE_MAX] =
{
  ble_scan,
  ble_profile,
  ble_data
};

static void *ble_sync_thread = NULL;


static ble_state_e ble_next_state (ble_state_e current_state)
{
  int32 sleep_interval;
  int32 num_scan;
  int32 num_profile;
  int32 num_data;
  int32 event;
  ble_state_e new_state;
  int32 power_save;
  timer_info_t *timer_info;

  sleep_interval = ble_get_sleep ();
  num_scan       = ble_check_scan_list ();
  num_profile    = ble_check_profile_list ();
  num_data       = ble_check_data_list ();
  power_save     = 1;
  timer_info     = NULL;

  if ((num_data > 0) &&
      ((sleep_interval < BLE_MIN_SLEEP_INTERVAL) || ((num_scan == 0) && (num_profile == 0))))
  {
    new_state = BLE_STATE_DATA;
    event     = BLE_TIMER_DATA;

    if (sleep_interval < BLE_MIN_SLEEP_INTERVAL)
    {
      power_save = 0;
    }
  }
  else if ((num_profile > 0) &&
           ((current_state == BLE_STATE_SCAN) || (current_state == BLE_STATE_DATA) || (num_scan == 0)))
  {
    new_state      = BLE_STATE_PROFILE;
    event          = BLE_TIMER_PROFILE;
    sleep_interval = BLE_MIN_TIMER_DURATION;
    power_save     = 0;
  }
  else
  {
    new_state      = BLE_STATE_SCAN;
    event          = BLE_TIMER_SCAN;
    sleep_interval = BLE_MIN_TIMER_DURATION;
    power_save     = 0;
  }

  if (power_save)
  {
    /* TODO: Power save */
    printf ("BLE Power save interval %d (ms)\n", sleep_interval);
    (void)timer_start (sleep_interval, event, ble_callback_timer, &timer_info);

  }
  else
  {
    printf ("BLE Wait interval %d (ms)\n", sleep_interval);
    (void)timer_start (sleep_interval, event, ble_callback_timer, &timer_info);
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

          ble_start_scan ();
        }
        else if (message->data[0] == BLE_TIMER_SCAN_STOP)
        {
          ble_stop_scan ();

          new_state = ble_next_state (BLE_STATE_SCAN);
        }
        else
        {
          ble_print_message (message);
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

  if (message->header.type == BLE_EVENT)
  {
    switch ((message->header.class << 8)|message->header.command)
    {
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_STATUS):
      {
        if ((ble_event_connection_status ((ble_event_connection_status_t *)message)) > 0)
        {
          ble_read_profile ();
        }
        
        break;
      }
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_PROCEDURE_COMPLETED):
      {
        ble_read_profile ();
        break;
      }      
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_DISCONNECTED):
      {
        ble_event_disconnect ((ble_event_disconnect_t *)message);
        ble_next_profile ();

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
      case ((BLE_CLASS_HW << 8)|BLE_EVENT_SOFT_TIMER):
      {
        if (message->data[0] == BLE_TIMER_PROFILE)
        {
          printf ("BLE Profile state\n");

          ble_start_profile ();
        }
        else if ((message->data[0] == BLE_TIMER_CONNECT_SETUP) ||
                 (message->data[0] == BLE_TIMER_CONNECT_DATA))
        {
          ble_event_connection_status_t *connection_status = (ble_event_connection_status_t *)message;

          connection_status->flags = (message->data[0] == BLE_TIMER_CONNECT_SETUP) ? BLE_CONNECT_SETUP_FAILED
                                                                                   : BLE_CONNECT_DATA_FAILED;
          
          (void)ble_event_connection_status ((ble_event_connection_status_t *)message);
        }
        else if (message->data[0] == BLE_TIMER_PROFILE_STOP)
        {
          new_state = ble_next_state (BLE_STATE_PROFILE);
        }
        else
        {
          ble_print_message (message);
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
  ble_state_e new_state = BLE_STATE_DATA;

  if (message->header.type == BLE_EVENT)
  {
    switch ((message->header.class << 8)|message->header.command)
    {
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_STATUS):
      {
        if ((ble_event_connection_status ((ble_event_connection_status_t *)message)) > 0)
        {
          ble_update_data ();
        }
        
        break;
      }
      case ((BLE_CLASS_CONNECTION << 8)|BLE_EVENT_DISCONNECTED):
      {
        ble_event_disconnect ((ble_event_disconnect_t *)message);
        ble_next_data ();

        break;
      }
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_PROCEDURE_COMPLETED):
      {
        ble_update_data ();
        break;        
      }
      case ((BLE_CLASS_ATTR_CLIENT << 8)|BLE_EVENT_ATTR_CLIENT_VALUE):
      {
        ble_event_attr_value ((ble_event_attr_value_t *)message);
        break;
      }      
      case ((BLE_CLASS_HW << 8)|BLE_EVENT_SOFT_TIMER):
      {
        if (message->data[0] == BLE_TIMER_DATA)
        {
          printf ("BLE Data state\n");

          ble_start_data ();
          
          if (ble_sync_thread == NULL)
          {
            os_create_thread (ble_sync, OS_THREAD_PRIORITY_NORMAL,
                              (BLE_SYNC_TIMEOUT/1000), &ble_sync_thread);
          }
        }
        else if ((message->data[0] == BLE_TIMER_CONNECT_SETUP) ||
                 (message->data[0] == BLE_TIMER_CONNECT_DATA))
        {
          ble_event_connection_status_t *connection_status = (ble_event_connection_status_t *)message;

          connection_status->flags = (message->data[0] == BLE_TIMER_CONNECT_SETUP) ? BLE_CONNECT_SETUP_FAILED
                                                                                   : BLE_CONNECT_DATA_FAILED;
          
          ble_event_connection_status ((ble_event_connection_status_t *)message);
        }
        else if (message->data[0] == BLE_TIMER_DATA_STOP)
        {
          os_destroy_thread (ble_sync_thread);
          ble_sync_thread = NULL;

          new_state = ble_next_state (BLE_STATE_DATA);
        }
        else
        {
          ble_print_message (message);
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

void master_loop (void)
{
  timer_info_t *timer_info = NULL;
  
  (void)timer_start (BLE_MIN_TIMER_DURATION, BLE_TIMER_SCAN,
                     ble_callback_timer, &timer_info);

  os_create_thread (ble_sync, OS_THREAD_PRIORITY_NORMAL,
                    ((4 * BLE_SYNC_TIMEOUT)/1000), &ble_sync_thread);

  while (1)
  {
    int pending;
    ble_message_t message;

    if ((ble_check_message_list ()) > 0)
    {
      do
      {
        pending = ble_receive_message (&message);
        ble_state = ble_state_handler[ble_state](&message);
      } while (pending > 0);
    }
  }
}

int main (int argc, char * argv[])
{
  os_init ();
  
  if ((ble_init ()) > 0)
  {
    master_loop ();
  }

  ble_deinit ();

  printf ("\n");

  return 0;
}

