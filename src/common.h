/* Common driver functionality that each device-specific driver must include and
 * specialize by defining the objects and functions.
 */

#ifndef LEVIATHAN_COMMON_H_INCLUDED
#define LEVIATHAN_COMMON_H_INCLUDED

#include <linux/hrtimer.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

/**
 * Driver-specific data.  This struct is defined by the driver.
 */
struct kraken_driver_data;

/**
 * The custom data stored in the interface, retrievable by usb_get_intfdata().
 */
struct kraken_data {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct kraken_driver_data *data;

	// any update syncs waiting for an update wait on this; updates wake
	// everything on this up
	struct wait_queue_head update_sync_waitqueue;
	// waiting update syncs set this to false; updates set it to true
	bool update_sync_condition;

	// the update work and queue
	struct workqueue_struct *update_workqueue;
	struct work_struct update_work;
	// the update interval and timer (a value of ktime_set(0, 0) means that
	// updates are halted)
	ktime_t update_interval;
	struct hrtimer update_timer;
	// the last update's success
	int update_retval;
};

/**
 * The driver's name.
 */
extern const char *kraken_driver_name;

/**
 * Driver-specific probe called from kraken_probe().  Driver-specific data must
 * be allocated here.
 */
extern int kraken_driver_probe(struct usb_interface *interface,
                               const struct usb_device_id *id);

/**
 * Driver-specific disconnect called from kraken_disconnect().  Driver-specific
 * data must be freed here.
 */
extern void kraken_driver_disconnect(struct usb_interface *interface);

/**
 * The driver's update function, called every second.
 */
extern int kraken_driver_update(struct kraken_data *kdata);

/**
 * Driver-specific device attribute file groups.  Created in kraken_probe() and
 * removed in kraken_disconnect().
 */
extern const struct attribute_group *kraken_driver_groups[];

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id);
void kraken_disconnect(struct usb_interface *interface);

#endif  /* LEVIATHAN_COMMON_H_INCLUDED */
