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
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	const s64 interval_ms = ktime_to_ms(kdata->update_interval);
	return scnprintf(buf, PAGE_SIZE, "%lld\n", interval_ms);
}

static ssize_t
update_interval_store(struct device *dev, struct device_attribute *attr,
                      const char *buf, size_t count)
{
	ktime_t interval_old;
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	u64 interval_ms;
	int ret = kstrtoull(buf, 0, &interval_ms);
	if (ret)
		return ret;
	// interval is 0: halt updates
	if (interval_ms == 0) {
		hrtimer_cancel(&kdata->update_timer);
		kdata->update_interval = ktime_set(0, 0);
		dev_info(dev, "halting updates: interval set to 0\n");
		return count;
	}
	// interval not 0: save interval in kdata
	interval_old = kdata->update_interval;
	kdata->update_interval = ms_to_ktime(
		max(interval_ms, UPDATE_INTERVAL_MIN_MS));
	// and restart updates if they'd been halted
	if (ktime_compare(interval_old, ktime_set(0, 0)) == 0)
		dev_info(dev, "restarting updates: interval set to non-0\n");
		hrtimer_start(&kdata->update_timer, kdata->update_interval,
		              HRTIMER_MODE_REL);
	return count;
}

static DEVICE_ATTR_RW(update_interval);

/* Initial value of attribute `update_interval`, settable as a parameter.
 */
static ulong update_interval_initial = UPDATE_INTERVAL_DEFAULT_MS;
module_param_named(update_interval, update_interval_initial, ulong, 0);

static ssize_t update_sync_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret;
	kdata->update_sync_condition = false;
	ret = !wait_event_interruptible(kdata->update_sync_waitqueue,
	                                kdata->update_sync_condition);
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static DEVICE_ATTR_RO(update_sync);

static struct attribute *kraken_group_attrs[] = {
	&dev_attr_update_interval.attr,
	&dev_attr_update_sync.attr,
	NULL,
};

static struct attribute_group kraken_group = {
	.attrs = kraken_group_attrs,
};

static const struct attribute_group *kraken_groups[] = {
	&kraken_group,
	NULL,
};

static int kraken_create_groups(struct usb_interface *interface)
{
	int retval;

	retval = device_add_groups(&interface->dev, kraken_groups);
	if (retval)
		goto error_groups;
	retval = device_add_groups(&interface->dev, kraken_driver_groups);
	if (retval)
		goto error_driver_groups;

	return 0;
error_driver_groups:
	device_remove_groups(&interface->dev, kraken_groups);
error_groups:
	return retval;
}

static void kraken_remove_groups(struct usb_interface *interface)
{
	device_remove_groups(&interface->dev, kraken_driver_groups);
	device_remove_groups(&interface->dev, kraken_groups);
}

static enum hrtimer_restart kraken_update_timer(struct hrtimer *update_timer)
{
	bool retval;
	struct kraken_data *kdata
		= container_of(update_timer, struct kraken_data, update_timer);

	// last update failed: halt updates
	if (kdata->update_retval) {
		dev_err(&kdata->udev->dev,
		        "halting updates: last update failed: %d\n",
		        kdata->update_retval);
		kdata->update_retval = 0;
		kdata->update_interval = ktime_set(0, 0);
		return HRTIMER_NORESTART;
	}

	// otherwise: queue new update and restart timer
	retval = queue_work(kdata->update_workqueue, &kdata->update_work);
	if (!retval)
		dev_warn(&kdata->udev->dev, "work already on a queue\n");
	hrtimer_forward(update_timer, ktime_get(), kdata->update_interval);
	return HRTIMER_RESTART;
}

static void kraken_update_work(struct work_struct *update_work)
{
	struct kraken_data *kdata
		= container_of(update_work, struct kraken_data, update_work);
	kdata->update_retval = kraken_driver_update(kdata);
	// tell any waiting update syncs that the update has finished
	kdata->update_sync_condition = true;
	wake_up_interruptible_all(&kdata->update_sync_waitqueue);
}

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id)
{
	char workqueue_name[64];
	int retval = -ENOMEM;
	struct usb_device *udev = interface_to_usbdev(interface);

	struct kraken_data *kdata = kmalloc(sizeof(*kdata), GFP_KERNEL);
	if (kdata == NULL)
		goto error_kdata;
	kdata->udev = usb_get_dev(udev);
	usb_set_intfdata(interface, kdata);

	retval = kraken_driver_probe(interface, id);
	if (retval)
		goto error_driver_probe;
	retval = kraken_create_groups(interface);
	if (retval) {
		dev_err(&interface->dev,
		        "failed to create device files: %d\n", retval);
		goto error_create_files;
	}

	init_waitqueue_head(&kdata->update_sync_waitqueue);
	kdata->update_sync_condition = false;

	snprintf(workqueue_name, sizeof(workqueue_name),
	         "%s_up", kraken_driver_name);
	kdata->update_workqueue = create_singlethread_workqueue(workqueue_name);
	INIT_WORK(&kdata->update_work, &kraken_update_work);

	hrtimer_init(&kdata->update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kdata->update_timer.function = &kraken_update_timer;
	if (update_interval_initial == 0) {
		kdata->update_interval = ktime_set(0, 0);
		dev_info(&interface->dev,
		         "not starting updates: interval set to 0\n");
	} else {
		kdata->update_interval = ms_to_ktime(
			max((u64) update_interval_initial,
			    UPDATE_INTERVAL_MIN_MS));
		hrtimer_start(&kdata->update_timer, kdata->update_interval,
		              HRTIMER_MODE_REL);
	}

	kdata->update_retval = 0;

	return 0;
error_create_files:
	kraken_driver_disconnect(interface);
error_driver_probe:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(kdata->udev);
	kfree(kdata);
error_kdata:
	return retval;
}

void kraken_disconnect(struct usb_interface *interface)
{
	struct kraken_data *kdata = usb_get_intfdata(interface);

	hrtimer_cancel(&kdata->update_timer);
	flush_workqueue(kdata->update_workqueue);
	destroy_workqueue(kdata->update_workqueue);
	kdata->update_sync_condition = true;
	wake_up_all(&kdata->update_sync_waitqueue);

	kraken_remove_groups(interface);
	kraken_driver_disconnect(interface);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(kdata->udev);
	kfree(kdata);
}
