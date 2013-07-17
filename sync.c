
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>

#include "types.h"
#include "list.h"
#include "sync.h"
#include "device.h"
#include "temperature.h"

LIST_HEAD_INIT (ble_sync_list_entry_t, sync_list);


static void ble_print_sync (void)
{
  ble_sync_list_entry_t *sync_list_entry = sync_list;
  
  while (sync_list_entry != NULL)
  {
    printf ("Sync -- %s\n", (sync_list_entry->type == BLE_SYNC_PUSH) ? "Push" : "Pull");

    if (sync_list_entry->data_type == BLE_SYNC_DEVICE)
    {
      ble_sync_device_data_t *sync_device_data
        = (ble_sync_device_data_t *)(sync_list_entry->data);
        
      printf ("  type    : %s\n", "Device");
      printf ("  address : 0x%s\n", sync_device_data->address);
      printf ("  name    : %s\n", sync_device_data->name);
      printf ("  service : 0x%s\n", sync_device_data->service);
      printf ("  interval: %d (min)\n", sync_device_data->interval);
      printf ("  status  : %s\n", sync_device_data->status);
    }
    else if (sync_list_entry->data_type == BLE_SYNC_TEMPERATURE)
    {
      ble_sync_temperature_data_t *sync_temperature_data
        = (ble_sync_temperature_data_t *)(sync_list_entry->data);
      
      printf ("  type       : %s\n", "Temperature");
      printf ("  time       : %s\n", sync_temperature_data->time);
      if (sync_temperature_data->temperature != FLT_MAX)
      {
        printf ("  temperature: %.1f (C)\n", sync_temperature_data->temperature);
      }
      else
      {
        printf ("  temperature: %s\n", "NA");
      }
      if (sync_temperature_data->battery_level != UINT_MAX)
      {
        printf ("  battery    : %u\n", sync_temperature_data->battery_level);
      }
      else
      {
        printf ("  battery    : %s\n", "NA");
      }
    }

    sync_list_entry = sync_list_entry->next;
  }  
}

void ble_sync_push (ble_sync_list_entry_t *push_list_entry)
{
  list_add ((list_entry_t **)(&sync_list), (list_entry_t *)push_list_entry);
}

void ble_sync_pull (ble_sync_list_entry_t **pull_list, uint8 data_type)
{
  ble_sync_list_entry_t *sync_list_entry = sync_list;

  while (sync_list_entry != NULL)
  {
    ble_sync_list_entry_t *pull_list_entry = NULL;

    if ((sync_list_entry->type == BLE_SYNC_PULL) &&
        (sync_list_entry->data_type == data_type))
    {
      pull_list_entry = sync_list_entry;
    }
    
    sync_list_entry = sync_list_entry->next;

    if (pull_list_entry != NULL)
    {
      list_remove ((list_entry_t **)(&sync_list), (list_entry_t *)pull_list_entry);
      list_add ((list_entry_t **)pull_list, (list_entry_t *)pull_list_entry);
      pull_list_entry = NULL;
    }
  }
}
  
void * ble_sync (void *timeout)
{
  ble_sync_list_entry_t *sync_list_entry;
  
  printf ("Sync thread start with timeout %d (sec)\n", (int32)timeout);

  sleep ((int32)timeout);
  ble_print_sync ();

  sync_list_entry = sync_list;
  while (sync_list_entry != NULL)
  {
    ble_sync_list_entry_t *sync_list_entry_del = sync_list_entry;
    
    if (sync_list_entry->data_type == BLE_SYNC_DEVICE)
    {
      ble_sync_device_data_t *sync_device_data
        = (ble_sync_device_data_t *)(sync_list_entry->data);
        
      free (sync_device_data->address);
      free (sync_device_data->name);
      free (sync_device_data->service);
      free (sync_device_data->status);
    }
    else if (sync_list_entry->data_type == BLE_SYNC_TEMPERATURE)
    {
      ble_sync_temperature_data_t *sync_temperature_data
        = (ble_sync_temperature_data_t *)(sync_list_entry->data);
      
      free (sync_temperature_data->time);
    }

    free (sync_list_entry->data);
    
    sync_list_entry = sync_list_entry->next;
    list_remove ((list_entry_t **)(&sync_list), (list_entry_t *)sync_list_entry_del);
    free (sync_list_entry_del);
  }
  
  printf ("Sync thread end\n");

  return NULL;
}

