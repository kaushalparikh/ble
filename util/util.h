#ifndef __UTIL_H__
#define __UTIL_H__

#include "basic_types.h"

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
  int32    id;
  int32    millisec;
  void   (*callback)(void *);
  void    *data;
} timer_info_t;

extern int32 timer_start (timer_info_t *timer_info);

extern int32 timer_status (timer_info_t *timer_info);

extern int32 timer_stop (timer_info_t *timer_info);

extern int32 clock_current_time (void);

#endif

