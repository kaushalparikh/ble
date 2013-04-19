#ifndef __UTIL_H__
#define __UTIL_H__

/* USB utilities */
struct usb_info
{
  struct usb_info *next;
  char            *dev_node;
  char            *dev_sys_path;
  char            *dev_subsystem_node;
  unsigned char    bus_num;
  unsigned char    dev_num;
};

typedef struct usb_info usb_info_t;

extern int usb_find (char *vendor_id, char *product_id,
                     char *subsystem, usb_info_t **usb_info);

extern int usb_disconnect (char *dev_node,
                           int   interface);

extern int usb_connect (char *dev_node,
                        int   interface);

/* Timer utilities */
typedef struct
{
  int    id;
  int    millisec;
  void (*callback)(void *);
  void  *data;
} timer_info_t;

extern int timer_start (timer_info_t *timer_info);

extern int timer_status (timer_info_t *timer_info);

extern int timer_stop (timer_info_t *timer_info);

extern int clock_current_time (void);

#endif

