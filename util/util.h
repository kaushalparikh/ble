#ifndef __UTIL_H__
#define __UTIL_H__

#include "types.h"

/* OS API */
enum
{
  OS_THREAD_PRIORITY_MIN = 1,
  OS_THREAD_PRIORITY_NORMAL,
  OS_THREAD_PRIORITY_MAX
};

extern void os_init (void);

extern int32 os_create_thread (void * (*start_function)(void *), uint8 priority,
                               int32 duration, void **handle);

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

extern int32 clock_get_count (void);

extern int8 * clock_get_time (void);

/* Database API */
enum
{
  DB_COLUMN_TYPE_TEXT = 0,
  DB_COLUMN_TYPE_INT,
  DB_COLUMN_TYPE_FLOAT,
  DB_COLUMN_TYPE_BLOB
};

enum
{
  DB_COLUMN_FLAG_PRIMARY_KEY       = 0x00000001,
  DB_COLUMN_FLAG_NOT_NULL          = 0x00000002,
  DB_COLUMN_FLAG_DEFAULT_TIMESTAMP = 0x00000004,
  DB_COLUMN_FLAG_DEFAULT_NA        = 0x00000008,
  DB_COLUMN_FLAG_UPDATE_KEY        = 0x00000010,
  DB_COLUMN_FLAG_UPDATE_VALUE      = 0x00000020
};

enum
{
  DB_WRITE_INSERT = 0,
  DB_WRITE_UPDATE,
  DB_WRITE_DELETE
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

typedef struct
{
  int8                        *title;
  uint32                       index;
  uint8                        type;
  uint32                       flags;
  int8                        *tag;
} db_column_entry_t;

struct db_table_list_entry
{
  struct db_table_list_entry *next;
  int8                       *title;
  uint32                      num_columns;
  db_column_entry_t          *column;
  void                       *insert;
  void                       *update;
  void                       *delete;
  void                       *select;
};

typedef struct db_table_list_entry db_table_list_entry_t;

typedef struct
{
  void                  *handle;
  db_table_list_entry_t *table_list;
} db_info_t;

extern int32 db_read_column (db_table_list_entry_t *table_list_entry,
                             uint32 index, db_column_value_t *column_value);

extern int32 db_write_column (db_table_list_entry_t *table_list_entry, uint8 type,
                              uint32 index, db_column_value_t *column_value);

extern int32 db_read_table (db_table_list_entry_t *table_list_entry);

extern int32 db_write_table (db_table_list_entry_t *table_list_entry, uint8 type);

extern int32 db_delete_table (db_info_t *db_info, db_table_list_entry_t *table_list_entry);

extern int32 db_create_table (db_info_t *db_info, db_table_list_entry_t *table_list_entry);

extern int32 db_open (int8 *file_name, db_info_t **db_info);

extern int32 db_close (db_info_t *db_info);

/* String/Binary API */

#define STRING_CONCAT(dest, src)                                    \
  {                                                                 \
    dest = realloc (dest, ((strlen (dest)) + (strlen (src)) + 1));  \
    dest = strcat (dest, src);                                      \
  } 

static inline void string_replace_char (int8 *dest, int8 search, int8 replace)
{
  int32 index = 0;
  
  while (dest[index] != '\0')
  {
    if (dest[index] == search)
    {
      dest[index] = replace;
    }
    
    index++;
  }
}

static inline void bin_to_string (int8 *dest, uint8 *src, int32 length)
{
  int32 count;

  for (count = 0; count < length; count++)
  {
    uint8 nibble = ((src[count] & 0xf0) >> 4);
    nibble += 0x30;
    if (nibble > 0x39)
    {
      nibble += 0x07;
    }
    *dest = (int8)nibble;
    dest++;

    nibble = src[count] & 0x0f;
    nibble += 0x30;
    if (nibble > 0x39)
    {
      nibble += 0x07;
    }    
    *dest = (int8)nibble;
    dest++;
  }

  *dest = '\0';
}

static inline void string_to_bin (uint8 *dest, int8 *src, int32 length)
{
  int32 count;  

  for (count = 0; count < length; count++)
  {
    uint8 nibble = (uint8)(src[count]);

    if (nibble > 0x60)
    {
      nibble -= 0x20;
    }
    if (nibble > 0x40)
    {
      nibble -= 0x07;
    }

    if (count & 0x1)
    {
      dest[count/2] |= (nibble & 0x0f);
    }
    else
    {
      dest[count/2] = ((nibble & 0x0f) << 4);
    }
  }
}

static inline void bin_reverse (uint8 *src, int32 length)
{
  int32 count;
  uint8 *src_end = src + length - 1;

  for (count = 0; count < ((length + 1)/2); count++)
  {
    uint8 tmp = *src;

    *src     = *src_end;
    *src_end = tmp;
    src++;
    src_end--;
  }
}

#endif

