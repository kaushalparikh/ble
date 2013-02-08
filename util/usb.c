
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <usb.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include <libudev.h>


struct udev_device * usb_find_child (struct udev *udev,
                                     char        *dev_par_path,
                                     char        *dev_subsystem)
{
  struct udev_device *dev = NULL;
  DIR *dev_par_dir;
  struct dirent *dev_dir;
  char *dev_path;

  dev_par_dir = opendir (dev_par_path);
  if (dev_par_dir != NULL)
  {
    for (dev_dir = readdir (dev_par_dir); ((dev_dir != NULL) && (dev == NULL));
         dev_dir = readdir (dev_par_dir))
    {
      if ((dev_dir->d_name[0] != '.') && (dev_dir->d_type == DT_DIR) &&
          (asprintf (&dev_path, "%s/%s", dev_par_path, dev_dir->d_name) > 0))
      {
        dev = udev_device_new_from_syspath (udev, dev_path);

        if ((dev == NULL) || ((udev_device_get_subsystem (dev)) == NULL) || 
            ((strcmp (dev_subsystem, udev_device_get_subsystem (dev))) != 0))
        {
          if (dev != NULL)
          {
            udev_device_unref (dev);
          }
          dev = usb_find_child (udev, dev_path, dev_subsystem);
        }
            
        free (dev_path);
      }
    }

    closedir (dev_par_dir);
  }

  return dev;
}

int usb_find (char *vendor_id, char *product_id)
{
  struct udev *udev;
  struct udev_enumerate *udev_enumerate;
  struct udev_list_entry *dev_par_list;
  
  /* Create the udev object */
  udev = udev_new ();
  if (udev == NULL)
  {
    printf("Can't create udev\n");
    return 1;
  }
  
  /* Create a list of the devices in the 'usb' subsystem */
  udev_enumerate = udev_enumerate_new (udev);
  udev_enumerate_add_match_sysattr (udev_enumerate, "idVendor", vendor_id);
  udev_enumerate_add_match_sysattr (udev_enumerate, "idProduct", product_id);
  udev_enumerate_add_match_subsystem (udev_enumerate, "usb");
  udev_enumerate_scan_devices (udev_enumerate);
  dev_par_list = udev_enumerate_get_list_entry (udev_enumerate);

  /* For each item enumerated, print out its information.
     udev_list_entry_foreach is a macro which expands to
     a loop. The loop will be executed for each member in
     devices, setting dev_list_entry to a list entry
     which contains the device's path in /sys */
  while (dev_par_list != NULL)
  {
    struct udev_device *dev, *dev_par;
    char *dev_par_path;
    
    /* Get the filename of the /sys entry for the device
       and create a udev_device object (dev) representing it
       usb_device_get_devnode() returns the path to the device node
       itself in /dev. */
    dev_par_path = (char *)udev_list_entry_get_name (dev_par_list);
    dev_par = udev_device_new_from_syspath (udev, dev_par_path);

    printf ("Device VID/PID %s/%s\n", udev_device_get_sysattr_value (dev_par, "idVendor"),
                                      udev_device_get_sysattr_value (dev_par, "idProduct"));
    printf ("       Type %s\n", udev_device_get_devtype (dev_par));
    printf ("       Node %s\n", udev_device_get_devnode (dev_par));
    printf ("       Path %s\n", udev_device_get_syspath (dev_par));

    /* Now search for 'tty' interface */
    dev = usb_find_child (udev, dev_par_path, "tty");
    if (dev != NULL)
    {
      printf ("   TTY\n");
      printf ("       Node %s\n", udev_device_get_devnode (dev));
      printf ("       Path %s\n", udev_device_get_syspath (dev));

      udev_device_unref (dev);
    }
    
    dev_par_list = udev_list_entry_get_next (dev_par_list);
    udev_device_unref (dev_par);
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

