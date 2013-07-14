
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "util.h"
#include "profile.h"
#include "temperature.h"


void ble_update_char_type (ble_char_list_entry_t * char_list_entry, uint8 type)
{
  ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
  
  if (type == BLE_ATTR_TYPE_READ)
  {
    if (char_decl->type & BLE_ATTR_TYPE_READ)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_READ;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_INDICATE)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_INDICATE;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_NOTIFY)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_NOTIFY;
    }
    else
    {
      char_list_entry->value->type = 0;
    }
  }
  else
  {
    if (char_decl->type & BLE_ATTR_TYPE_WRITE)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_WRITE_NO_RSP)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE_NO_RSP;
    }
    else if (char_decl->type & BLE_ATTR_TYPE_WRITE_SIGNED)
    {
      char_list_entry->value->type = BLE_ATTR_TYPE_WRITE_SIGNED;
    }
    else
    {
      char_list_entry->value->type = 0;
    }
  }

  if (char_list_entry->value->type & (BLE_ATTR_TYPE_INDICATE | BLE_ATTR_TYPE_NOTIFY))
  {
    ((ble_char_client_config_t *)(char_list_entry->client_config->data))->bitfield
      = (char_list_entry->value->type & BLE_ATTR_TYPE_INDICATE)
        ? BLE_CHAR_CLIENT_INDICATE : BLE_CHAR_CLIENT_NOTIFY;
  }
}

ble_attribute_t * ble_find_attribute (ble_service_list_entry_t *service_list_entry,
                                      uint16 handle)
{
  ble_attribute_t *attribute = NULL;
  
  while ((service_list_entry != NULL) && (attribute == NULL))
  {
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;

    while ((char_list_entry != NULL) && (attribute == NULL))
    {
      if ((char_list_entry->declaration != NULL) && (char_list_entry->declaration->handle == handle))
      {
        attribute = char_list_entry->declaration;
      }
      else if ((char_list_entry->value != NULL) && (char_list_entry->value->handle == handle))
      {
        attribute = char_list_entry->value;
      }
      else if ((char_list_entry->description != NULL) && (char_list_entry->description->handle == handle))
      {
        attribute = char_list_entry->description;
      }        
      else if ((char_list_entry->client_config != NULL) && (char_list_entry->client_config->handle == handle))
      {
        attribute = char_list_entry->client_config;
      }
      else if ((char_list_entry->format != NULL) && (char_list_entry->format->handle == handle))
      {
        attribute = char_list_entry->format;
      }

      char_list_entry = char_list_entry->next;
    }

    char_list_entry = service_list_entry->update.char_list;

    while ((char_list_entry != NULL) && (attribute == NULL))
    {
      if ((char_list_entry->declaration != NULL) && (char_list_entry->declaration->handle == handle))
      {
        attribute = char_list_entry->declaration;
      }
      else if ((char_list_entry->value != NULL) && (char_list_entry->value->handle == handle))
      {
        attribute = char_list_entry->value;
      }
      else if ((char_list_entry->description != NULL) && (char_list_entry->description->handle == handle))
      {
        attribute = char_list_entry->description;
      }        
      else if ((char_list_entry->client_config != NULL) && (char_list_entry->client_config->handle == handle))
      {
        attribute = char_list_entry->client_config;
      }
      else if ((char_list_entry->format != NULL) && (char_list_entry->format->handle == handle))
      {
        attribute = char_list_entry->format;
      }        
      
      char_list_entry = char_list_entry->next;
    }
    
    service_list_entry = service_list_entry->next;
  }

  return attribute;
}

void ble_clear_characteristics (ble_char_list_entry_t *char_list_entry)
{
  while (char_list_entry != NULL)
  {
    ble_char_list_entry_t *char_list_entry_del = char_list_entry;
    
    if (char_list_entry->declaration != NULL)
    {
      if (char_list_entry->declaration->data != NULL)
      {
        free (char_list_entry->declaration->data);
      }
      free (char_list_entry->declaration);
    }
    if (char_list_entry->value != NULL)
    {
      if (char_list_entry->value->data != NULL)
      {
        free (char_list_entry->value->data);
      }
      free (char_list_entry->value);
    }
    if (char_list_entry->description != NULL)
    {
      if (char_list_entry->description->data != NULL)
      {
        free (char_list_entry->description->data);
      }
      free (char_list_entry->description);
    }
    if (char_list_entry->client_config != NULL)
    {
      if (char_list_entry->client_config->data != NULL)
      {
        free (char_list_entry->client_config->data);
      }
      free (char_list_entry->client_config);
    }
    if (char_list_entry->format != NULL)
    {
      if (char_list_entry->format->data != NULL)
      {
        free (char_list_entry->format->data);
      }
      free (char_list_entry->format);
    }
    
    list_remove ((list_entry_t **)(&(char_list_entry)), (list_entry_t *)char_list_entry_del);
    free (char_list_entry_del);
  }  
}

void ble_print_service (ble_service_list_entry_t *service_list_entry)
{
  while (service_list_entry != NULL)
  {
    int32 i;
    uint16 uuid = BLE_PACK_GATT_UUID (service_list_entry->declaration->uuid);
    ble_char_list_entry_t *char_list_entry = service_list_entry->char_list;
      
    printf ("Service -- %s\n", ((uuid == BLE_GATT_PRI_SERVICE) ? "primary" : "secondary"));
    printf ("  handle range: 0x%04x to 0x%04x\n", service_list_entry->start_handle, service_list_entry->end_handle);
    printf ("          uuid: 0x");
    for (i = (service_list_entry->declaration->data_length - 1); i >= 0; i--)
    {
      printf ("%02x", service_list_entry->declaration->data[i]);
    }
    printf ("\n");

    while (char_list_entry != NULL)
    {
      if (char_list_entry->declaration != NULL)
      {
        ble_char_decl_t *char_decl = (ble_char_decl_t *)(char_list_entry->declaration->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->declaration->uuid);

        printf ("    Characteristics -- declaration\n");
        printf ("      handle: 0x%04x\n", char_list_entry->declaration->handle);
        printf ("       value: ");
        printf ("0x%02x, 0x%04x, 0x", char_decl->type, char_decl->handle);
        for (i = (char_list_entry->declaration->data_length - 4); i >= 0; i--)
        {
          printf ("%02x", char_decl->uuid[i]);
        }
        printf ("\n");        
      }

      if (char_list_entry->description != NULL)
      {
        uuid = BLE_PACK_GATT_UUID (char_list_entry->description->uuid);

        printf ("        Descriptor -- description\n");
        printf ("          handle: 0x%04x\n", char_list_entry->description->handle);
        printf ("           value: ");
        for (i = 0; i < char_list_entry->description->data_length; i++)
        {
          printf ("%c", (int8)(char_list_entry->description->data[i]));
        }
        printf ("\n");       
      }

      if (char_list_entry->client_config != NULL)
      {
        ble_char_client_config_t *client_config = (ble_char_client_config_t *)(char_list_entry->client_config->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->client_config->uuid);

        printf ("        Descriptor -- client configuration\n");
        printf ("          handle: 0x%04x\n", char_list_entry->client_config->handle);
        printf ("           value: ");
        printf ("0x%04x\n", client_config->bitfield);      
      }

      if (char_list_entry->format != NULL)
      {
        ble_char_format_t *format = (ble_char_format_t *)(char_list_entry->format->data);
        
        uuid = BLE_PACK_GATT_UUID (char_list_entry->client_config->uuid);
        
        printf ("        Descriptor -- format\n");
        printf ("          handle: 0x%04x\n", char_list_entry->format->handle);
        printf ("           value: ");
        printf ("0x%02x, %d\n", format->bitfield, format->exponent);
      }
      
      char_list_entry = char_list_entry->next;
    }
    
    service_list_entry = service_list_entry->next;
  }
}

void ble_update_service (ble_service_list_entry_t *service_list_entry,
                         ble_device_list_entry_t *device_list_entry)
{
  while (service_list_entry != NULL)
  {
    if ((service_list_entry->update.char_list != NULL) &&
        (service_list_entry->update.wait <= 0))
    {
      uint8 uuid_length;
      uint16 uuid;
  
      uuid_length = service_list_entry->declaration->data_length;
      uuid        = BLE_PACK_GATT_UUID (service_list_entry->declaration->data);
  
      if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_TEMPERATURE_SERVICE_UUID))
      {
        ble_update_temperature (service_list_entry, device_list_entry);
      }
    }

    service_list_entry = service_list_entry->next;
  }
}

int32 ble_init_service (ble_service_list_entry_t *service_list_entry,
                        ble_device_list_entry_t *device_list_entry)
{
  int32 status = 0;
  
  while (service_list_entry != NULL)
  {
    uint8 uuid_length;
    uint16 uuid;

    uuid_length = service_list_entry->declaration->data_length;
    uuid        = BLE_PACK_GATT_UUID (service_list_entry->declaration->data);

    if ((uuid_length == BLE_GATT_UUID_LENGTH) && (uuid == BLE_TEMPERATURE_SERVICE_UUID))
    {
      status += (ble_init_temperature (service_list_entry, device_list_entry));
    }

    service_list_entry = service_list_entry->next;
  }

  return status;  
}

ble_service_list_entry_t * ble_find_service (ble_service_list_entry_t *service_list_entry,
                                             uint8 *uuid, uint8 uuid_length)
{
  while (service_list_entry != NULL)
  {
    if ((memcmp (uuid, service_list_entry->declaration->data, uuid_length)) == 0)
    {
      break;
    }
    
    service_list_entry = service_list_entry->next;
  }

  return service_list_entry;  
}

void ble_clear_service (ble_service_list_entry_t *service_list_entry)
{
  while (service_list_entry != NULL)
  {
    ble_clear_characteristics (service_list_entry->char_list);
    service_list_entry->char_list = NULL;
    ble_clear_characteristics (service_list_entry->update.char_list);
    service_list_entry->update.char_list = NULL;    
    service_list_entry = service_list_entry->next;
  }
}

