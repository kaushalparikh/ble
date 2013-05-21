
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <libudev.h>

#include "usb.h"


static struct udev_device * usb_find_child (struct udev *udev,
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

int usb_find (char *vendor_id, char *product_id,
              char *subsystem, usb_info_t **usb_info)
{
  struct udev *udev;
  struct udev_enumerate *udev_enumerate;
  struct udev_list_entry *dev_par_list;
  int count = 0;
  
  /* Create the udev object */
  udev = udev_new ();
  if (udev != NULL)
  {
    usb_info_t *usb_list = NULL;

    /* Create a list of the devices in the 'usb' subsystem */
    udev_enumerate = udev_enumerate_new (udev);
    udev_enumerate_add_match_sysattr (udev_enumerate, "idVendor", vendor_id);
    udev_enumerate_add_match_sysattr (udev_enumerate, "idProduct", product_id);
    udev_enumerate_add_match_subsystem (udev_enumerate, "usb");

    udev_enumerate_scan_devices (udev_enumerate);
    dev_par_list = udev_enumerate_get_list_entry (udev_enumerate);

    /* For each device enumerated, find the sub-system device */
    while (dev_par_list != NULL)
    {
      struct udev_device *dev, *dev_par;
      char *dev_par_path;
      
      /* Get the filename of the /sys entry for the device */
      dev_par_path = (char *)udev_list_entry_get_name (dev_par_list);
      dev_par = udev_device_new_from_syspath (udev, dev_par_path);

      printf ("Search for '%s' subsystem under %s\n", subsystem, dev_par_path);

      /* Now search for subsystem */
      dev = usb_find_child (udev, dev_par_path, subsystem);
      if (dev != NULL)
      {
        printf ("Found %s\n", udev_device_get_devnode (dev));

        if (usb_list == NULL)
        {
          usb_list = (usb_info_t *)malloc (sizeof (usb_info_t));
          *usb_info = usb_list;
        }
        else
        {
          usb_list->next = (usb_info_t *)malloc (sizeof (usb_info_t));
          usb_list = usb_list->next;
        }

        if (((asprintf (&(usb_list->dev_node), "%s", udev_device_get_devnode (dev_par))) > 0) &&
            ((asprintf (&(usb_list->dev_sys_path), "%s", dev_par_path)) > 0) &&
            ((asprintf (&(usb_list->dev_subsystem_node), "%s", udev_device_get_devnode (dev))) > 0))
        {
          usb_list->bus_num = (unsigned char)(atoi (udev_device_get_sysattr_value (dev_par, "busnum")));
          usb_list->dev_num = (unsigned char)(atoi (udev_device_get_sysattr_value (dev_par, "devnum")));
          usb_list->next = NULL;
        }

        count++;
        udev_device_unref (dev);
      }
      
      dev_par_list = udev_list_entry_get_next (dev_par_list);
      udev_device_unref (dev_par);
    }

    /* Free the enumerator object */
    udev_enumerate_unref (udev_enumerate);
    udev_unref (udev);
  }
  else
  {
    printf ("Can't create udev context\n");
  }
  
  return count;       
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

#ifdef UTIL_USB_TEST

int main (void)
{
  usb_info_t *usb_list = NULL;
  usb_info_t *usb_list_entry = NULL;
  int count;

  printf ("\n");
  count = usb_find ("2458", "0001", "tty",
                    &usb_list);

  while (count > 0)
  {
    printf ("Device     %s\n", usb_list->dev_node);
    printf ("  Sys Path %s\n", usb_list->dev_sys_path);
    printf ("  Node     %s\n", usb_list->dev_subsystem_node);

    usb_list_entry = usb_list;
    usb_list = usb_list->next;
    free (usb_list_entry);
    count--;
  }

  printf ("\n");

  return 0;
}

#endif

