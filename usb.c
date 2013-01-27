
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <libudev.h>
#include <libusb.h>


static void print_devs (libusb_device **devs)
{
  libusb_device *dev;
  int i = 0;

  while ((dev = devs[i++]) != NULL)
  {
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor (dev, &desc);
    if (r < 0)
    {
      fprintf(stderr, "failed to get device descriptor");
      return;
    }

    printf ("%04x:%04x (bus %d, device %d)\n",
             desc.idVendor, desc.idProduct,
             libusb_get_bus_number(dev), libusb_get_device_address(dev));
  }
}

int libudev_main (char * vendor_id, char * product_id)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
  
  libusb_device **devs;
  int r;
  ssize_t cnt;

  int fd = -1;
	
	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		printf("Can't create udev\n");
		return 1;
	}
	
  r = libusb_init (NULL);
  if (r < 0)
    return r;

	/* Create a list of the devices in the 'hidraw' subsystem. */
	enumerate = udev_enumerate_new(udev);
	
  /* udev_enumerate_add_match_subsystem(enumerate, "tty");
	udev_enumerate_scan_devices(enumerate); */
  udev_enumerate_add_match_subsystem(enumerate, "tty");
  /* udev_enumerate_add_match_property(enumerate, "ID_VENDOR_ID", "2458");
  udev_enumerate_add_match_property(enumerate, "ID_MODEL_ID", "0001"); */
	udev_enumerate_scan_devices(enumerate);
	
  cnt = libusb_get_device_list(NULL, &devs);
  if (cnt < 0)
    return (int) cnt;

  devices = udev_enumerate_get_list_entry(enumerate);
	/* For each item enumerated, print out its information.
	   udev_list_entry_foreach is a macro which expands to
	   a loop. The loop will be executed for each member in
	   devices, setting dev_list_entry to a list entry
	   which contains the device's path in /sys. */
	udev_list_entry_foreach(dev_list_entry, devices) {
	  struct udev_device *dev, *dev_par;
		const char *path;
		
		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it
		   usb_device_get_devnode() returns the path to the device node
		   itself in /dev. */
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		/* The device pointed to by dev contains information about
		   the hidraw device. In order to get information about the
		   USB device, get the parent device with the
		   subsystem/devtype pair of "usb"/"usb_device". This will
		   be several levels up the tree, but the function will find
		   it.*/
		dev_par = udev_device_get_parent_with_subsystem_devtype(
		            dev,
		            "usb",
		            "usb_device");
		if (dev_par != NULL)
    {
		  /* From here, we can call get_sysattr_value() for each file
		     in the device's /sys entry. The strings passed into these
		     functions (idProduct, idVendor, serial, etc.) correspond
		     directly to the files in the directory which represents
		     the USB device. Note that USB strings are Unicode, UCS2
		     encoded, but the strings returned from
		     udev_device_get_sysattr_value() are UTF-8 encoded. */
      if (((strcmp (vendor_id, (udev_device_get_sysattr_value(dev_par, "idVendor")))) == 0) &&
          ((strcmp (product_id, (udev_device_get_sysattr_value(dev_par, "idProduct")))) == 0))
      {
        libusb_device *dev1;
        int i = 0;

        while ((dev1 = devs[i++]) != NULL)
        {
          struct libusb_device_descriptor desc;
          int r = libusb_get_device_descriptor (dev1, &desc);
          if (r < 0)
          {
            printf("Failed to get device descriptor");
            continue;
          }

          if ( ((strtol (vendor_id, NULL, 16)) == desc.idVendor) &&
               ((strtol (product_id, NULL, 16)) == desc.idProduct) &&
               ((atoi (udev_device_get_sysattr_value (dev_par, "busnum"))) == libusb_get_bus_number (dev1)) &&
               ((atoi (udev_device_get_sysattr_value (dev_par, "devnum"))) == libusb_get_device_address (dev1)) )

          {
            printf ("%04x:%04x (bus %d, device %d)\n",
                     desc.idVendor, desc.idProduct,
                     libusb_get_bus_number(dev1), libusb_get_device_address (dev1));

            printf ("Opening %s\n", udev_device_get_devnode (dev));
            fd = open ((udev_device_get_devnode (dev)), (O_RDWR | O_NOCTTY /*| O_NDELAY*/));
            if (fd >= 0)
            {
              if ((lockf (fd, F_TLOCK, 0)) < 0)
              {
                printf ("Device in use, %s\n", strerror (errno));
                close (fd);
                fd = -1;
              }
            }
            else
            {
              printf ("Unable to open\n");
            }

            break;
          }
        }
      }
		  
      udev_device_unref(dev_par);
		}
    else
    {
      udev_device_unref(dev);
    }

    if (fd > 0)
    {
      break;
    }
	}

  while (1)
  {
  }
 
  if (fd > 0)
  {
    close (fd);
  }

  libusb_free_device_list (devs, 1);
  libusb_exit (NULL);

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);

	udev_unref(udev);

	return 0;       
}

int main (void)
{
  printf ("\n");
  libudev_main ("2458", "0001");
  printf ("\n");

  return 0;
}

