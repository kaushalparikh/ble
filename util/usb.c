
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <usb.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include <libudev.h>


int usb_find (char * vendor_id, char * product_id)
{
  struct udev *udev;
  struct udev_enumerate *udev_enumerate;
  struct udev_list_entry *dev_list, *dev_list_entry;
  
  /* Create the udev object */
  udev = udev_new ();
  if (udev == NULL)
  {
    printf("Can't create udev\n");
    return 1;
  }
  
  /* Create a list of the devices in the 'tty' subsystem */
  udev_enumerate = udev_enumerate_new (udev);
  udev_enumerate_add_match_subsystem (udev_enumerate, "tty");
  udev_enumerate_scan_devices (udev_enumerate);
  dev_list = udev_enumerate_get_list_entry (udev_enumerate);

  /* For each item enumerated, print out its information.
     udev_list_entry_foreach is a macro which expands to
     a loop. The loop will be executed for each member in
     devices, setting dev_list_entry to a list entry
     which contains the device's path in /sys */
  udev_list_entry_foreach (dev_list_entry, dev_list) {
    struct udev_device *dev, *dev_par;
    const char *dev_path;
    
    /* Get the filename of the /sys entry for the device
       and create a udev_device object (dev) representing it
       usb_device_get_devnode() returns the path to the device node
       itself in /dev. */
    dev_path = udev_list_entry_get_name (dev_list_entry);
    dev = udev_device_new_from_syspath (udev, dev_path);

    /* The device pointed to by dev contains information about
       the hidraw device. In order to get information about the
       USB device, get the parent device with the
       subsystem/devtype pair of "usb"/"usb_device". This will
       be several levels up the tree, but the function will find
       it */
    dev_par = udev_device_get_parent_with_subsystem_devtype (dev,
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
         udev_device_get_sysattr_value() are UTF-8 encoded */
      if (((strcmp (vendor_id, (udev_device_get_sysattr_value (dev_par, "idVendor")))) == 0) &&
          ((strcmp (product_id, (udev_device_get_sysattr_value (dev_par, "idProduct")))) == 0))
      {
        printf ("Device VID/PID %s/%s\n", udev_device_get_sysattr_value (dev_par, "idVendor"),
                                          udev_device_get_sysattr_value (dev_par, "idProduct"));
        printf ("       Type %s\n", udev_device_get_devtype (dev_par));
        printf ("       Node %s\n", udev_device_get_devnode (dev_par));
        printf ("       Path %s\n", udev_device_get_syspath (dev_par));
        printf ("       Interface Path %s\n", udev_device_get_devpath (dev));

        while ((dev_par = udev_device_get_parent (dev_par)) != NULL)
        {
          const char *dev_class = udev_device_get_sysattr_value (dev_par, "bDeviceClass");

          if (dev_class == NULL)
          {
            continue;
          }
            
          if (atoi (dev_class) == USB_CLASS_HUB)
          {
            struct usbdevfs_ioctl ioctl_param;
            struct usbdevfs_hub_portinfo hub_portinfo;
            int fd;
            const char *dev_node = udev_device_get_devnode (dev_par);

            printf ("Hub VID/PID %s/%s\n", udev_device_get_sysattr_value (dev_par, "idVendor"),
                                              udev_device_get_sysattr_value (dev_par, "idProduct"));
            printf ("    Type %s\n", udev_device_get_devtype (dev_par));
            printf ("    Node %s\n", dev_node);
            printf ("    Path %s\n", udev_device_get_syspath (dev_par));

            ioctl_param.ifno       = 0;
            ioctl_param.ioctl_code = USBDEVFS_HUB_PORTINFO;
            ioctl_param.data       = &hub_portinfo;
  
            fd = open (dev_node, O_RDWR);
            if (fd > 0)
            {
              if ((ioctl (fd, USBDEVFS_IOCTL, &ioctl_param)) >= 0)
              {
                int i;
                printf ("    Number of Ports %d\n", hub_portinfo.nports);
                for (i = 0; i < hub_portinfo.nports; i++)
                {
                  printf ("    Port %d Device Number %d\n", (i+1), hub_portinfo.port[i]);
                }
              }
              else
              {
                printf ("    Unable to read hub port info\n");
              }
            }
            else
            {
              printf ("    Unable to open hub device node\n");
            }

            break;
            
          }
        }

      }
    }

    udev_device_unref (dev);
  }
  
  /* Free the enumerator object */
  udev_enumerate_unref (udev_enumerate);
  udev_unref (udev);

  return 0;       
}

int usb_disconnect (char *dev_node,
                    int   interface)
{
  int fd, status;

  status = 0;
  fd = open (dev_node, O_RDWR);
  if (fd > 0)
  {
    struct usbdevfs_ioctl param;

    memset (&param, 0, sizeof (param));
    
    param.ifno       = interface;
    param.ioctl_code = USBDEVFS_DISCONNECT;
    param.data       = NULL;

    status = ioctl (fd, USBDEVFS_IOCTL, &param);
    if (status < 0)
    {
      printf ("Unable to disconnect device node %s interface %d\n", dev_node, interface);
    }
    close (fd);
  }
  else
  {
    status = -1;
    printf ("Unable to open device node %s\n", dev_node);
  }

  return status;
}

int usb_connect (char *dev_node,
                 int   interface)
{
  int fd, status;

  status = 0;
  fd = open (dev_node, O_RDWR);
  if (fd > 0)
  {
    struct usbdevfs_ioctl param;

    memset (&param, 0, sizeof (param));
    
    param.ifno       = interface;
    param.ioctl_code = USBDEVFS_CONNECT;
    param.data       = NULL;

    status = ioctl (fd, USBDEVFS_IOCTL, &param);
    if (status < 0)
    {
      printf ("Unable to connect device node %s interface %d\n", dev_node, interface);
    }
    close (fd);
  }
  else
  {
    status = -1;
    printf ("Unable to open device node %s\n", dev_node);
  }
  
  return status;
}

int main (void)
{
  printf ("\n");
  usb_find ("2458", "0001");
  printf ("\n");

  return 0;
}

