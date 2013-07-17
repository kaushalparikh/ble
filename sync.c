
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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
      ble_sync_device_data_t *sync_device_data = (ble_sync_device_data_t *)(sync_list_entry->data);
        
      printf ("  type    : %s\n", "Device");
      printf ("  address : 0x%s\n", sync_device_data->address);
      printf ("  name    : %s\n", sync_device_data->name);
      printf ("  service : 0x%s\n", sync_device_data->service);
      printf ("  interval: %d (min)\n", sync_device_data->interval);
      printf ("  status  : %s\n", sync_device_data->status);
    }
    else if (sync_list_entry->data_type == BLE_SYNC_TEMPERATURE)
    {
      printf ("  type    : %s\n", "Temperature");
    }

    sync_list_entry = sync_list_entry->next;
  }  
}

void * ble_sync (void *timeout)
{
  ble_sync_list_entry_t *sync_list_entry;
  
  printf ("Sync thread start with timeout %d (sec)\n", (int32)timeout);

  ble_sync_device (&sync_list);
  ble_print_sync ();

  sync_list_entry = sync_list;
  while (sync_list_entry != NULL)
  {
    ble_sync_list_entry_t *sync_list_entry_del = sync_list_entry;
    
    if (sync_list_entry->data_type == BLE_SYNC_DEVICE)
    {
      ble_sync_device_data_t *sync_device_data = (ble_sync_device_data_t *)(sync_list_entry->data);
        
      free (sync_device_data->address);
      free (sync_device_data->name);
      free (sync_device_data->service);
      free (sync_device_data->status);
    }
    else if (sync_list_entry->data_type == BLE_SYNC_TEMPERATURE)
    {
    }

    free (sync_list_entry->data);
    
    sync_list_entry = sync_list_entry->next;
    list_remove ((list_entry_t **)(&sync_list), (list_entry_t *)sync_list_entry_del);
    free (sync_list_entry_del);
  }
  
  printf ("Sync thread end\n");

  return NULL;
}

