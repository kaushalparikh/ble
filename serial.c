
/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
        status = serial_open ();
        if (status < 0)
        {
          /* Earlier device not valid, find new */
          free (serial_device.usb_info.dev_sys_path);
          serial_device.usb_info.dev_sys_path = NULL;
          serial_free ();
        }
      }
    }

    if (serial_device.usb_info.dev_subsystem_node == NULL)
    {
      usb_list_entry = usb_list;
      while (usb_list_entry != NULL)
      {
        serial_device.usb_info = *usb_list_entry;
        status = serial_open ();
        if (status > 0)
        {
          if (((asprintf (&(serial_device.usb_info.dev_node), "%s", usb_list_entry->dev_node)) > 0) &&
              ((asprintf (&(serial_device.usb_info.dev_sys_path), "%s", usb_list_entry->dev_sys_path)) > 0) &&
              ((asprintf (&(serial_device.usb_info.dev_subsystem_node), "%s", usb_list_entry->dev_subsystem_node)) > 0))
          {
            serial_device.usb_info.bus_num = usb_list_entry->bus_num;
            serial_device.usb_info.dev_num = usb_list_entry->dev_num;
          }

          break;
        }

        usb_list_entry = usb_list_entry->next;
      }
    }

    usb_list_entry = usb_list;
    while (usb_list != NULL)
    {
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

int serial_tx (size_t bytes, unsigned char * buffer)
{
  ssize_t bytes_written = 0;
  
  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    size_t i;
#endif
  
    bytes_written = write (serial_device.file_desc, buffer, bytes);

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

int serial_rx (size_t bytes, unsigned char * buffer)
{
  ssize_t bytes_read = 0;

  while (bytes > 0)
  {
#ifdef DEBUG_VERBOSE
    size_t i;
#endif

    bytes_read = read (serial_device.file_desc, buffer, bytes);

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

