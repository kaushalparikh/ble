#ifndef __SERIAL_H__
#define __SERIAL_H__

extern int uart_reset (void);

extern int uart_init (void);

extern void uart_deinit (void);

extern int uart_open (void);

extern void uart_close (void);

extern int uart_tx (size_t bytes, unsigned char * buffer);

extern int uart_rx (size_t bytes, unsigned char * buffer);

#endif

