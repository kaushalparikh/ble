
/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

/* Local/project headers */
#include "util.h"
#include "serial.h"

/* Local structures */
typedef struct
{
  char       *vendor_id;
  char       *product_id;
  int         file_desc;
  usb_info_t  usb_info;
} serial_device_t;

/* Verbose output */
#define DEBUG_VERBOSE

/* Defines, 1000 ms */
#define SERIAL_TIMEOUT  (1000)

/* File scope global variables */
static serial_device_t serial_device =
{
  .vendor_id   = "2458",
  .product_id  = "0001",
  .file_desc   = -1,
  .usb_info =
    {
      .dev_node           = NULL,
      .dev_sys_path       = NULL,
      .dev_subsystem_node = NULL,
      .bus_num            = 255,
      .dev_num            = 255,
      .next               = NULL
    }
}; 


void serial_free (void)
{
  free (serial_device.usb_info.dev_node);
  serial_device.usb_info.dev_node = NULL;
  free (serial_device.usb_info.dev_subsystem_node);
  serial_device.usb_info.dev_subsystem_node = NULL;
  serial_device.usb_info.bus_num = 255;
  serial_device.usb_info.dev_num = 255;
}

int serial_init (void)
{
  usb_info_t *usb_list = NULL;
  int status = -1;

  if ((usb_find (serial_device.vendor_id, serial_device.product_id,
                 "tty", &usb_list)) > 0)
  {
    usb_info_t *usb_list_entry = NULL;

    if (serial_device.usb_info.dev_sys_path != NULL)
    {
      usb_list_entry = usb_list;
      while (usb_list_entry != NULL)
      {
        if ((strcmp (serial_device.usb_info.dev_sys_path, usb_list_entry->dev_sys_path)) == 0)
        {
          if (((asprintf (&(serial_device.usb_info.dev_node), "%s", usb_list_entry->dev_node)) > 0) &&
              ((asprintf (&(serial_device.usb_info.dev_subsystem_node), "%s", usb_list_entry->dev_subsystem_node)) > 0))
          {
            serial_device.usb_info.bus_num = usb_list_entry->bus_num;
            serial_device.usb_info.dev_num = usb_list_entry->dev_num;
          }

          break;
        }

        usb_list_entry = usb_list_entry->next;
      }

      if (serial_device.usb_info.dev_subsystem_node != NULL)
      {
        printf ("Trying to use %s\n", serial_device.usb_info.dev_subsystem_node);
        if ((serial_open ()) >  0)
        {
          status = 0;
        }
        else
        {
          serial_free ();
        }
      }
    }
    else
    {
      usb_list_entry = usb_list;
      while (usb_list_entry != NULL)
      {
        serial_device.usb_info = *usb_list_entry;
        printf ("Trying to use %s\n", serial_device.usb_info.dev_subsystem_node);
        if ((serial_open ()) >  0)
        {
          status = 0;

          if (((asprintf (&(serial_device.usb_info.dev_node), "%s", usb_list_entry->dev_node)) > 0) &&
              ((asprintf (&(serial_device.usb_info.dev_sys_path), "%s", usb_list_entry->dev_sys_path)) > 0) &&
              ((asprintf (&(serial_device.usb_info.dev_subsystem_node), "%s", usb_list_entry->dev_subsystem_node)) > 0))
          {
            serial_device.usb_info.bus_num = usb_list_entry->bus_num;
            serial_device.usb_info.dev_num = usb_list_entry->dev_num;
          }

          break;
        }
        else
        {
          serial_device.usb_info.dev_sys_path = NULL;
        }

        usb_list_entry = usb_list_entry->next;
      }
    }

    while (usb_list != NULL)
    {
      usb_list_entry = usb_list;
      free (usb_list_entry->dev_node);
      free (usb_list_entry->dev_sys_path);
      free (usb_list_entry->dev_subsystem_node);
      usb_list = usb_list->next;
      free (usb_list_entry);
    }
  }

  return status;
}

void serial_deinit (void)
{
  serial_close ();
  serial_free ();
}

int serial_open (void)
{
  serial_device.file_desc = open (serial_device.usb_info.dev_subsystem_node,
                                  (O_RDWR | O_NOCTTY /*| O_NDELAY*/));
  if (serial_device.file_desc > 0)
  {
    if ((lockf (serial_device.file_desc, F_TLOCK, 0)) == 0)
    {
      struct termios options;
      int tiocm;

      /* Get current serial port options */
      tcgetattr (serial_device.file_desc, &options);

      /* Set read/write baud rate */
      cfsetispeed (&options, B115200);
      cfsetospeed (&options, B115200);

      /* Set parameters including enabling receiver */
      options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS /*| HUPCL*/);
      options.c_cflag |= (CS8 | CLOCAL | CREAD | CRTSCTS);
      options.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE | IEXTEN);
      options.c_iflag &= ~(INPCK | IXON | IXOFF | IXANY | ICRNL);
      options.c_oflag &= ~(OPOST | ONLCR);

      /* Timeout is in tenths of seconds */
      options.c_cc[VTIME] = (SERIAL_TIMEOUT * 10)/1000;
      options.c_cc[VMIN]  = 0;

      /* Flush buffers and apply options */
      tcsetattr (serial_device.file_desc, TCSAFLUSH, &options);

      /* Set DTR/RTS */
      ioctl (serial_device.file_desc, TIOCMGET, &tiocm);
      tiocm = TIOCM_DTR | TIOCM_RTS;
      ioctl (serial_device.file_desc, TIOCMSET, &tiocm);
    }
    else
    {
      printf ("Can't lock %s\n", serial_device.usb_info.dev_subsystem_node);
      close (serial_device.file_desc);
      serial_device.file_desc = -1;
    }
  }
  else
  {
    printf("Can't open %s\n", serial_device.usb_info.dev_subsystem_node);
  }

  return serial_device.file_desc;
}

void serial_close (void)
{
  tcdrain (serial_device.file_desc);
  close (serial_device.file_desc);
}

int serial_tx (size_t bytes, unsigned char *buffer)
{
  ssize_t bytes_written = 0;
  
  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    ssize_t i;
#endif
  
    bytes_written = write (serial_device.file_desc, buffer, bytes);

    if (bytes_written > 0)
    {
#ifdef DEBUG_VERBOSE
      printf("UART TX (%d): ", bytes_written);
      for (i = 0; i < bytes_written; i++)
      {
        printf("%02x", buffer[i]);
      }
      printf("\n");
#endif

      bytes  -= bytes_written;
      buffer += bytes_written;
    }
    else if ((bytes_written == 0) ||
             ((bytes_written < 0) && (errno != EINTR)))
    {
      break;
    }
  }

  return bytes_written;
}

int serial_rx (size_t bytes, unsigned char *buffer)
{
  ssize_t bytes_read = 0;

  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    ssize_t i;
#endif

    bytes_read = read (serial_device.file_desc, buffer, bytes);

    if (bytes_read > 0)
    {
#ifdef DEBUG_VERBOSE
      printf("UART RX (%d): ", bytes_read);
      for (i = 0; i < bytes_read; i++)
      {
        printf("%02x", buffer[i]);
      }
      printf("\n");
#endif

      bytes  -= bytes_read;
      buffer += bytes_read;
    }
    else if ((bytes_read == 0) ||
             ((bytes_read < 0) && (errno != EINTR)))
    {
      break;
    }
  }

  return bytes_read;
}

