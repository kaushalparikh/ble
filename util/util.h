#ifndef __UTIL_H__
#define __UTIL_H__

/* USB utilities */
struct usb_info
{
  char            *dev_node;
  char            *dev_sys_path;
  char            *dev_subsystem_node;
  unsigned char    bus_num;
  unsigned char    dev_num;
  struct usb_info *next;
};

typedef struct usb_info usb_info_t;

extern int usb_find (char *vendor_id, char *product_id,
                     char *subsystem, usb_info_t **usb_info);

extern int usb_disconnect (char *dev_node,
                           int   interface);

extern int usb_connect (char *dev_node,
                        int   interface);

#endif

