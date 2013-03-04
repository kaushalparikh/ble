#ifndef __GATT_H__
#define __GATT_H__

typedef struct
{
  uint16  handle;
  uint8   type_length;
  uint8  *type;
  uint8   value_length;
  uint8  *value;
  uint8   permission;
} ble_attribute_t;

struct ble_characteristics
{
  struct ble_characteristics *next;
  ble_attribute_t            *declaration;
  ble_attribute_t            *description;
  ble_attribute_t            *value;
  ble_attribute_t            *format;
};

typedef struct ble_characteristics ble_characteristics_t;

struct ble_service
{
  struct ble_service    *include;
  ble_attribute_t        attribute;
  ble_characteristics_t  characteristic;
};

#endif

