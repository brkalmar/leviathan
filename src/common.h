/* Common driver functionality that each device-specific driver must include and
 * specialize by defining the objects and functions.
 */

#ifndef LEVIATHAN_COMMON_H_INCLUDED
#define LEVIATHAN_COMMON_H_INCLUDED

#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

/**
 * The driver's name.
 */
extern const char *kraken_driver_name;

/**
 * Driver-specific data.  This struct is defined by the driver.
 */
struct kraken_driver_data;

/**
 * Return size of struct `kraken_driver_data`.
 */
extern size_t kraken_driver_data_size(void);

/**
 * The custom data stored in the interface, retrievable by usb_get_intfdata().
 */
struct kraken_data {
	// Driver-specific data.
	struct kraken_driver_data *data;

	// Handle to the usb device.
	struct usb_device *udev;
	// Handle to the driver device.
	struct device *dev;

	// Shared USB message data buffer.  Expanded each time a bigger buffer
	// than already allocated is requested.
	u8 *usb_data;
	size_t usb_data_size;

	// Any update syncs waiting for an update wait on this; updates wake
	// everything on this up.
	struct wait_queue_head update_sync_waitqueue;
	// Waiting update syncs set this to false; updates set it to true.
	bool update_sync_condition;

	// The update queue and work.
	struct workqueue_struct *update_workqueue;
	struct work_struct update_work;
	// Checked by each update work to see if it should queue another work
	// after itself.
	bool update;
};

/**
 * The driver's update function, called every update cycle.
 */
extern int kraken_driver_update(struct kraken_data *kdata);

/**
 * The driver's liquid temperature attribute [°C].
 */
extern u32 kraken_driver_get_temp(struct kraken_data *kdata);

/**
 * Driver-specific device attribute file groups.  Created in kraken_probe() and
 * removed in kraken_disconnect().
 */
extern const struct attribute_group *kraken_driver_groups[];

/**
 * Driver-specific probe called from kraken_probe().  Driver-specific data is
 * allocated before calling this.
 */
extern int kraken_driver_probe(struct usb_interface *interface,
                               const struct usb_device_id *id);

/**
 * Driver-specific disconnect called from kraken_disconnect().  Driver-specific
 * data is freed after calling this.
 */
extern void kraken_driver_disconnect(struct usb_interface *interface);

/**
 * Get DMA-capable buffer for USB messages, shared for all messages under a
 * given USB interface.  Used to avoid allocating a new data buffer for every
 * message.
 *
 * This should not lead to any races as long as all USB messages are sent
 * synchronously.
 *
 * Return 0 on success, -ENOMEM if kmalloc fails.
 */
int kraken_usb_data(struct kraken_data *kdata, u8 **data, size_t size);

/**
 * The main probe function.
 */
int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id);

/**
 * The main disconnect function.
 */
void kraken_disconnect(struct usb_interface *interface);

#endif  /* LEVIATHAN_COMMON_H_INCLUDED */
