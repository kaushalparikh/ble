#ifndef __SERIAL_H__
#define __SERIAL_H__

extern int serial_init (void);

extern void serial_deinit (void);

extern int serial_open (void);

extern void serial_close (void);

extern int serial_tx (size_t bytes, unsigned char * buffer);

extern int serial_rx (size_t bytes, unsigned char * buffer);

#endif

