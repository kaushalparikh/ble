
/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <fcntl.h>

#include <libudev.h>
#include <libusb.h>

/* Local/project headers */
#include "serial.h"

/* Local structures */
typedef struct
{
  char *vendor_id;
  char *product_id;
  char *node;
  int  usb_bus_num;
  int  usb_dev_num;
  int  file_desc;
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
  .node        = NULL,
  .usb_bus_num = -1,
  .usb_dev_num = -1,
  .file_desc   = -1
}; 


int uart_reset (void)
{
  int status;

  status = libusb_init (NULL);
  if (status == 0)
  {
    libusb_device **libusb_list;

    status = libusb_get_device_list (NULL, &libusb_list);
    if (status >= 0)
    {
      libusb_device *libusb_dev;
      int i = 0;

      status = -1;
      while ((libusb_dev = libusb_list[i++]) != NULL)
      {
        struct libusb_device_descriptor libusb_dev_desc;
        status = libusb_get_device_descriptor (libusb_dev, &libusb_dev_desc);

        if (((strtol (serial_device.vendor_id, NULL, 16)) == libusb_dev_desc.idVendor) &&
            ((strtol (serial_device.product_id, NULL, 16)) == libusb_dev_desc.idProduct) &&
            (serial_device.usb_bus_num == libusb_get_bus_number (libusb_dev)) &&
            (serial_device.usb_dev_num == libusb_get_device_address (libusb_dev)) &&
            (status == 0))
        {
          libusb_device_handle *libusb_dev_handle;

          printf ("libusb: Open device\n");
          status = libusb_open (libusb_dev, &libusb_dev_handle);
          if (status == 0)
          {
            printf ("libusb: Reset device\n");
            status = libusb_reset_device (libusb_dev_handle);
            if (status < 0)
            {
              printf ("Can't reset libusb dev\n");
            }

            printf ("libusb: Close device\n");
            libusb_close (libusb_dev_handle);

            sleep (2);
          }
          else
          {
            printf ("Can't open libusb dev\n");
          }

          break;
        }

        status = -1;
      }

      libusb_free_device_list (libusb_list, 1);
    }

    libusb_exit (NULL);
  }

  return status;
}

int uart_init (void)
{
  struct udev *udev = NULL;
  struct udev_enumerate *udev_enumerate = NULL;
  struct udev_list_entry *udev_list;
  int status = -1;

  /* Create the udev context */
  udev = udev_new ();
  if (udev != NULL)
  {
    /* Create the udev enumerate object */
    udev_enumerate = udev_enumerate_new (udev);
    if (udev_enumerate != NULL)
    {
      /* Create a list of the devices in the 'tty' subsystem */
      udev_enumerate_add_match_subsystem (udev_enumerate, "tty");
      udev_enumerate_scan_devices (udev_enumerate);

      udev_list = udev_enumerate_get_list_entry (udev_enumerate);
      
      /* For each enumerated tty device, check if it is
         serial device with given VID/PID. Also check
         if the device can be opened and lock it if it can
         be opened */
      while ((udev_list != NULL) && (status < 0))
      {
        struct udev_device *udev_dev, *udev_dev_parent;
        const char *udev_node, *dev_node;
    
        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it
           usb_device_get_devnode() returns the path to the device node
           itself in /dev */
        udev_node = udev_list_entry_get_name (udev_list);
        udev_dev = udev_device_new_from_syspath (udev, udev_node);
    
        /* The device pointed to by dev contains information about
           the tty device. In order to get information about the
           USB device, get the parent device with the
           subsystem/devtype pair of "usb"/"usb_device". This will
           be several levels up the tree, but the function will find
           it */
        udev_dev_parent = udev_device_get_parent_with_subsystem_devtype (udev_dev,
                                                                         "usb",
                                                                         "usb_device");
    
        if (udev_dev_parent != NULL)
        {
          /* From here, we can call get_sysattr_value() for each file
             in the device's /sys entry. The strings passed into these
             functions (idProduct, idVendor, serial, etc.) correspond
             directly to the files in the directory which represents
             the USB device. Note that USB strings are Unicode, UCS2
             encoded, but the strings returned from
             udev_device_get_sysattr_value() are UTF-8 encoded */
          if (((strcmp (serial_device.vendor_id, (udev_device_get_sysattr_value (udev_dev_parent, "idVendor")))) == 0) &&
              ((strcmp (serial_device.product_id, (udev_device_get_sysattr_value (udev_dev_parent, "idProduct")))) == 0))
          {
            /* Found what we were looking for, get it's node (under /dev)
               and try to open & lock it */
            dev_node = udev_device_get_devnode (udev_dev);
            printf ("Found %s\n", dev_node);

            serial_device.node        = (char *)dev_node;
            serial_device.usb_bus_num = atoi (udev_device_get_sysattr_value (udev_dev_parent,
                                                                             "busnum"));
            serial_device.usb_dev_num = atoi (udev_device_get_sysattr_value (udev_dev_parent,
                                                                             "devnum"));

            serial_device.file_desc = uart_open ();
            if (serial_device.file_desc > 0)
            {
              serial_device.node = (char *)malloc ((strlen (dev_node)) + 1);
              strcpy (serial_device.node, dev_node);

              status = 0;
            }
            else
            {
              uart_reset ();
              serial_device.node = NULL;
              serial_device.file_desc = -1;
            }
          }
          
          /* Unreference parent, it will internally unreference child,
             i.e. udev_dev as well */
          udev_device_unref (udev_dev_parent);
        }
        else
        {
          /* Device parent not found, so the device has to be
             unreferenced explicitly */
          udev_device_unref (udev_dev);
        }

        /* Next device in list */
        udev_list = udev_list_entry_get_next (udev_list);
      }

      udev_enumerate_unref (udev_enumerate);
    }
    else
    {
      printf ("Can't create udev enumerate object\n");
    }
    
    udev_unref (udev);
  }
  else
  {
    printf ("Can't create udev context\n");
  }

  return status;
}

void uart_deinit (void)
{
  serial_device.usb_dev_num = -1;
  serial_device.usb_bus_num = -1;

  uart_close ();
  
  if (serial_device.node != NULL)
  {
    free (serial_device.node);
    serial_device.node = NULL;
  }
}

int uart_open (void)
{
  struct termios options;

  if (serial_device.file_desc < 0)
  {
    serial_device.file_desc = open (serial_device.node, (O_RDWR | O_NOCTTY /*| O_NDELAY*/));
  }

  if (serial_device.file_desc > 0)
  {
    if ((lockf (serial_device.file_desc, F_TLOCK, 0)) == 0)
    {
      int i;

      /* Get current serial port options */
      tcgetattr (serial_device.file_desc, &options);

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
      tcsetattr(serial_device.file_desc, TCSAFLUSH, &options);
    }
    else
    {
      printf ("Can't lock %s\n", serial_device.node);
      close (serial_device.file_desc);
      serial_device.file_desc = -1;
    }
  }
  else
  {
    printf("Can't open %s\n", serial_device.node);
  }

  return serial_device.file_desc;
}

void uart_close (void)
{
  if (serial_device.file_desc > 0)
  {
    close (serial_device.file_desc);
    serial_device.file_desc = -1;
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

int uart_rx (size_t bytes, unsigned char * buffer)
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

