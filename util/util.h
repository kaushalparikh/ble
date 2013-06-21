#ifndef __UTIL_H__
#define __UTIL_H__

#include "types.h"

/* Serial API */
extern int32 serial_init (void);

extern void serial_deinit (void);

extern int32 serial_open (void);

extern void serial_close (void);

extern int32 serial_tx (uint32 bytes, uint8 *buffer);

extern int32 serial_rx (uint32 bytes, uint8 *buffer);

/* Timer API */
typedef struct
{
  void    *handle;
  int32    millisec;
  int32    event;
  void   (*callback)(void *);
} timer_info_t;

extern int32 timer_start (int32 millisec, int32 event,
                          void (*callback)(void *), timer_info_t **timer_info);

extern int32 timer_status (timer_info_t *timer_info);

extern int32 timer_stop (timer_info_t *timer_info);

extern int32 clock_current_time (void);

/* Database API */
enum
{
  DB_COLUMN_TYPE_TEXT  = 1,
  DB_COLUMN_TYPE_INT   = 2,
  DB_COLUMN_TYPE_FLOAT = 3,
  DB_COLUMN_TYPE_BLOB  = 4 ,
  DB_COLUMN_TYPE_NULL  = 5
};

enum
{
  DB_COLUMN_FLAG_PRIMARY_KEY       = 0x00000001,
  DB_COLUMN_FLAG_NOT_NULL          = 0x00000002,
  DB_COLUMN_FLAG_DEFAULT_TIMESTAMP = 0x00000004,
  DB_COLUMN_FLAG_DEFAULT_NA        = 0x00000008
};

typedef union
{
  int32  integer;
  float  decimal;
  int8  *text;
  struct
  {
    uint32  length;
    uint8  *data;
  } blob;
} db_column_value_t;

struct db_column_list_entry
{
  struct db_column_list_entry *next;
  int8                        *title;
  uint32                       index;
  uint8                        type;
  uint32                       flags;
  int8                        *tag;
};

typedef struct db_column_list_entry db_column_list_entry_t;

struct db_table_list_entry
{
  struct db_table_list_entry *next;
  db_column_list_entry_t     *column_list;
  int8                       *title;
  uint32                      index;
  void                       *insert;
  void                       *select;
};

typedef struct db_table_list_entry db_table_list_entry_t;

typedef struct
{
  int32                  handle;
  db_table_list_entry_t *table_list;
} db_info_t;

extern int32 db_write_column (db_info_t *db_info, int8 *table_title, int8 *column_title,
                              db_column_value_t *column_value);

extern int32 db_write_table (db_info_t *db_info, int8 *title);

extern int32 db_clear_table (db_info_t *db_info, int8 *title);

extern int32 db_create_table (db_info_t *db_info, int8 *title, uint32 index,
                              db_column_list_entry_t *column_list);

extern int32 db_open (int8 *file_name, db_info_t **db_info);

extern int32 db_close (db_info_t *db_info);

#endif

