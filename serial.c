
/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <fcntl.h>

/* Local/project headers */
#include "serial.h"

/* Verbose output */
#define DEBUG_VERBOSE

/* Defines, 1000 ms */
#define SERIAL_TIMEOUT  (1000)

/* Global variables */
char * serial_device = NULL;
int serial_device_handle = -1;


void uart_device (char * device)
{
  if (serial_device != NULL)
  {
    free (serial_device);
    serial_device = NULL;
  }
  
  serial_device = (char *)malloc ((strlen (device)) + 1);
  strcpy (serial_device, device);
}

int uart_open (void)
{
  struct termios options;
  int i;

  serial_device_handle = open (serial_device, (O_RDWR | O_NOCTTY /*| O_NDELAY*/));

  if (serial_device_handle > 0)
  {
    /* Get current serial port options */
    tcgetattr (serial_device_handle, &options);

    /* Set read/write baud rate */
    cfsetispeed (&options, B115200);
    cfsetospeed (&options, B115200);

    /* Set parameters including enabling receiver */
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS | HUPCL);
    options.c_cflag |= (CS8 | CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE | IEXTEN);
    options.c_iflag &= ~(INPCK | IXON | IXOFF | IXANY | ICRNL);
    options.c_oflag &= ~(OPOST | ONLCR);

    for (i = 0; i < sizeof (options.c_cc); i++)
    {
      options.c_cc[i] = _POSIX_VDISABLE;
    }

    /* Timeout is in tenths of seconds */
    options.c_cc[VTIME] = (SERIAL_TIMEOUT * 10)/1000;
    options.c_cc[VMIN]  = 0;

    /* Flush buffers and apply options */
    tcsetattr(serial_device_handle, TCSAFLUSH, &options);
  }
  else
  {
    printf("Unable to open serial device %s\n", serial_device);
  }

  return serial_device_handle;
}

void uart_close (void)
{
  if (serial_device_handle > 0)
  {
    close (serial_device_handle);
    serial_device_handle = -1;
  }
}

int uart_tx (size_t bytes, unsigned char * buffer)
{
  ssize_t bytes_written = 0;
  
  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    size_t i;
#endif
  
    bytes_written = write (serial_device_handle, buffer, bytes);

    if (bytes_written <= 0)
    {
      break;
    }

#ifdef DEBUG_VERBOSE
    printf("UART TX: ");
    for (i = 0; i < bytes_written; i++)
    {
      printf("%02d", buffer[i]);
    }
    printf("\n");
#endif

    bytes  -= bytes_written;
    buffer += bytes_written;
    
    bytes_written = 0;
  }

  return bytes_written;
}

int uart_rx (size_t bytes, unsigned char * buffer)
{
  ssize_t bytes_read = 0;

  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    size_t i;
#endif

    bytes_read = read (serial_device_handle, buffer, bytes);

    if (bytes_read <= 0)
    {
      break;
    }

#ifdef DEBUG_VERBOSE
    printf("UART RX: ");
    for (i = 0; i < bytes; i++)
    {
      printf("%02d", buffer[i]);
    }
    printf("\n");
#endif

    bytes  -= bytes_read;
    buffer += bytes_read;
  }

  return bytes_read;
}

