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

static ssize_t update_show(struct device *dev, struct device_attribute *attr,
                           char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%s\n", kdata->update ? "on" : "off");
}

static ssize_t update_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	bool update;
	int retval = kstrtobool(buf, &update);
	if (retval)
		return retval;
	// update is false: halt updates
	if (!update) {
		kdata->update = update;
		flush_workqueue(kdata->update_workqueue);
		return count;
	}
	// if updates are not already halted, nothing needs to be done
	if (kdata->update)
		return count;
	// otherwise: restart updates
	dev_info(dev, "restarting updates: turned on\n");
	kdata->update = update;
	retval = queue_work(kdata->update_workqueue, &kdata->update_work);
	if (!retval) {
		dev_err(dev, "update work already on a queue\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR_RW(update);

/* Initial value of attribute `update`, settable as a parameter.
 */
static bool update_initial = true;
module_param_named(update, update_initial, bool, 0);

static ssize_t update_sync_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int retval;
	kdata->update_sync_condition = false;
	retval = !wait_event_interruptible(kdata->update_sync_waitqueue,
	                                   kdata->update_sync_condition);
	return scnprintf(buf, PAGE_SIZE, "%d\n", retval);
}

static DEVICE_ATTR_RO(update_sync);

static void kraken_update_work(struct work_struct *update_work)
{
	struct kraken_data *kdata
		= container_of(update_work, struct kraken_data, update_work);
	int retval = kraken_driver_update(kdata);

	// tell any waiting update syncs that the update has finished
	kdata->update_sync_condition = true;
	wake_up_interruptible_all(&kdata->update_sync_waitqueue);

	// last update failed: halt updates
	if (retval) {
		dev_err(&kdata->udev->dev, "last update failed: %d\n", retval);
		kdata->update = false;
	}

	// re-queue this work if updates are still on
	if (!kdata->update) {
		dev_info(&kdata->udev->dev, "halting updates: turned off\n");
		return;
	}
	retval = queue_work(kdata->update_workqueue, &kdata->update_work);
	if (!retval) {
		dev_err(&kdata->udev->dev, "update work already on a queue\n");
		kdata->update = false;
	}
}

static struct attribute *kraken_group_attrs[] = {
	&dev_attr_update.attr,
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

	init_waitqueue_head(&kdata->update_sync_waitqueue);
	kdata->update_sync_condition = false;

	snprintf(workqueue_name, sizeof(workqueue_name),
	         "%s_up", kraken_driver_name);
	kdata->update_workqueue = create_singlethread_workqueue(workqueue_name);
	INIT_WORK(&kdata->update_work, &kraken_update_work);
	kdata->update = update_initial;

	if (kdata->update) {
		retval = queue_work(kdata->update_workqueue,
		                    &kdata->update_work);
		if (!retval) {
			dev_err(&kdata->udev->dev,
			        "update work already on a queue\n");
			retval = 1;
			goto error_queue_work;
		}
	} else {
		dev_info(&interface->dev, "not starting updates: turned off\n");
	}

	retval = kraken_create_groups(interface);
	if (retval) {
		dev_err(&interface->dev,
		        "failed to create device files: %d\n", retval);
		goto error_create_groups;
	}

	return 0;
error_create_groups:
	kdata->update = false;
	flush_workqueue(kdata->update_workqueue);
error_queue_work:
	destroy_workqueue(kdata->update_workqueue);
	kdata->update_sync_condition = true;
	wake_up_all(&kdata->update_sync_waitqueue);
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

	kraken_remove_groups(interface);

	kdata->update = false;
	flush_workqueue(kdata->update_workqueue);
	destroy_workqueue(kdata->update_workqueue);
	kdata->update_sync_condition = true;
	wake_up_all(&kdata->update_sync_waitqueue);

	kraken_driver_disconnect(interface);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(kdata->udev);
	kfree(kdata);
}
