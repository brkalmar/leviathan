/* Implementation of common driver functionality.
 */

#include "common.h"

#include <linux/freezer.h>
#include <linux/hrtimer.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define UPDATE_INTERVAL_DEFAULT_MS ((u64) 1000)
#define UPDATE_INTERVAL_MIN_MS     ((u64) 500)

static ssize_t update_interval_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	const s64 interval_ms = ktime_to_ms(kraken->update_interval);
	return scnprintf(buf, PAGE_SIZE, "%lld\n", interval_ms);
}

static ssize_t
update_interval_store(struct device *dev, struct device_attribute *attr,
                      const char *buf, size_t count)
{
	ktime_t interval_old;
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	u64 interval_ms;
	int ret = kstrtoull(buf, 0, &interval_ms);
	if (ret)
		return ret;
	// interval is 0: halt updates
	if (interval_ms == 0) {
		hrtimer_cancel(&kraken->update_timer);
		kraken->update_interval = ktime_set(0, 0);
		dev_info(dev, "halting updates: interval set to 0\n");
		return count;
	}
	// interval not 0: save interval in kraken
	interval_old = kraken->update_interval;
	kraken->update_interval = ms_to_ktime(
		max(interval_ms, UPDATE_INTERVAL_MIN_MS));
	// and restart updates if they'd been halted
	if (ktime_compare(interval_old, ktime_set(0, 0)) == 0)
		dev_info(dev, "restarting updates: interval set to non-0\n");
		hrtimer_start(&kraken->update_timer, kraken->update_interval,
		              HRTIMER_MODE_REL);
	return count;
}

static DEVICE_ATTR_RW(update_interval);

/* Initial value of attribute `update_interval`, settable as a parameter.
 */
static ulong update_interval_initial = UPDATE_INTERVAL_DEFAULT_MS;
module_param_named(update_interval, update_interval_initial, ulong, 0);

static ssize_t update_indicator_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	int ret;
	kraken->update_indicator_condition = false;
	ret = !wait_event_interruptible(kraken->update_indicator_waitqueue,
	                                kraken->update_indicator_condition);
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static DEVICE_ATTR_RO(update_indicator);

static int kraken_create_device_files(struct usb_interface *interface)
{
	int retval;
	if ((retval = device_create_file(&interface->dev,
	                                 &dev_attr_update_interval)))
		goto error_update_interval;
	if ((retval = device_create_file(&interface->dev,
	                                 &dev_attr_update_indicator)))
		goto error_update_indicator;
	if ((retval = kraken_driver_create_device_files(interface)))
		goto error_driver_files;

	return 0;
error_driver_files:
	device_remove_file(&interface->dev, &dev_attr_update_indicator);
error_update_indicator:
	device_remove_file(&interface->dev, &dev_attr_update_interval);
error_update_interval:
	return retval;
}

static void kraken_remove_device_files(struct usb_interface *interface)
{
	kraken_driver_remove_device_files(interface);

	device_remove_file(&interface->dev, &dev_attr_update_indicator);
	device_remove_file(&interface->dev, &dev_attr_update_interval);
}

static enum hrtimer_restart kraken_update_timer(struct hrtimer *update_timer)
{
	bool retval;
	struct usb_kraken *kraken
		= container_of(update_timer, struct usb_kraken, update_timer);

	// last update failed: halt updates
	if (kraken->update_retval) {
		dev_err(&kraken->udev->dev,
		        "halting updates: last update failed: %d\n",
		        kraken->update_retval);
		kraken->update_retval = 0;
		kraken->update_interval = ktime_set(0, 0);
		return HRTIMER_NORESTART;
	}

	// otherwise: queue new update and restart timer
	retval = queue_work(kraken->update_workqueue, &kraken->update_work);
	if (!retval)
		dev_warn(&kraken->udev->dev, "work already on a queue\n");
	hrtimer_forward(update_timer, ktime_get(), kraken->update_interval);
	return HRTIMER_RESTART;
}

static void kraken_update_work(struct work_struct *update_work)
{
	struct usb_kraken *kraken
		= container_of(update_work, struct usb_kraken, update_work);
	kraken->update_retval = kraken_driver_update(kraken);
	// tell any waiting indicators that the update has finished
	kraken->update_indicator_condition = true;
	wake_up_interruptible_all(&kraken->update_indicator_waitqueue);
}

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id)
{
	char workqueue_name[64];
	int retval = -ENOMEM;
	struct usb_device *udev = interface_to_usbdev(interface);

	struct usb_kraken *kraken = kmalloc(sizeof(*kraken), GFP_KERNEL);
	if (kraken == NULL)
		goto error_kraken;
	kraken->udev = usb_get_dev(udev);
	usb_set_intfdata(interface, kraken);

	retval = kraken_driver_probe(interface, id);
	if (retval)
		goto error_driver_probe;
	retval = kraken_create_device_files(interface);
	if (retval) {
		dev_err(&interface->dev,
		        "failed to create device files: %d\n", retval);
		goto error_create_files;
	}

	kraken->update_retval = 0;

	init_waitqueue_head(&kraken->update_indicator_waitqueue);
	kraken->update_indicator_condition = false;

	snprintf(workqueue_name, sizeof(workqueue_name),
	         "%s_up", kraken_driver_name);
	kraken->update_workqueue
		= create_singlethread_workqueue(workqueue_name);
	INIT_WORK(&kraken->update_work, &kraken_update_work);

	hrtimer_init(&kraken->update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kraken->update_timer.function = &kraken_update_timer;
	if (update_interval_initial == 0) {
		kraken->update_interval = ktime_set(0, 0);
		dev_info(&interface->dev,
		         "not starting updates: interval set to 0\n");
	} else {
		kraken->update_interval = ms_to_ktime(
			max((u64) update_interval_initial,
			    UPDATE_INTERVAL_MIN_MS));
		hrtimer_start(&kraken->update_timer, kraken->update_interval,
		              HRTIMER_MODE_REL);
	}

	return 0;
error_create_files:
	kraken_driver_disconnect(interface);
error_driver_probe:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(kraken->udev);
	kfree(kraken);
error_kraken:
	return retval;
}

void kraken_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);

	hrtimer_cancel(&kraken->update_timer);
	flush_workqueue(kraken->update_workqueue);
	destroy_workqueue(kraken->update_workqueue);
	kraken->update_indicator_condition = true;
	wake_up_all(&kraken->update_indicator_waitqueue);

	kraken_remove_device_files(interface);
	kraken_driver_disconnect(interface);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(kraken->udev);
	kfree(kraken);
}
