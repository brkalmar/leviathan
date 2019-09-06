/* Driver for 2433:b200 devices.
 */

#include "../common.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x61"

const char *kraken_driver_name = DRIVER_NAME;

struct kraken_driver_data {
	// TODO: it would be nice to protect these messages from data races by
	// mutexes, like in kraken_x62.  They shouldn't happen frequently, and
	// it's not exactly a huge problem if they happen, but lack of data
	// races ought to be an objective for all programs, especially drivers.
	bool send_color;
	u8 color_message[19];
	u8 pump_message[2];
	u8 fan_message[2];
	u8 status_message[32];
};

static int kraken_start_transaction(struct usb_kraken *kraken)
{
	return usb_control_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 0), 2, 0x40, 0x0001, 0, NULL, 0, 1000);
}

static int kraken_send_message(struct usb_kraken *kraken, u8 *message, int length)
{
	int sent;
	int retval = usb_bulk_msg(kraken->udev, usb_sndbulkpipe(kraken->udev, 2), message, length, &sent, 3000);
	if (retval != 0)
		return retval;
	if (sent != length)
		return -EIO;
	return 0;
}

static int kraken_receive_message(struct usb_kraken *kraken, u8 message[], int expected_length)
{
	int received;
	int retval = usb_bulk_msg(kraken->udev, usb_rcvbulkpipe(kraken->udev, 2), message, expected_length, &received, 3000);
	if (retval != 0)
		return retval;
	if (received != expected_length)
		return -EIO;
	return 0;
}

int kraken_driver_update(struct usb_kraken *kraken)
{
	int retval = 0;
	struct kraken_driver_data *data = kraken->data;
	if (data->send_color) {
		if (
			(retval = kraken_start_transaction(kraken)) ||
			(retval = kraken_send_message(kraken, data->color_message, 19)) ||
			(retval = kraken_receive_message(kraken, data->status_message, 32))
		   )
			dev_err(&kraken->udev->dev, "Failed to update: %d\n", retval);
		data->send_color = false;
	} else {
		if (
			(retval = kraken_start_transaction(kraken)) ||
			(retval = kraken_send_message(kraken, data->pump_message, 2)) ||
			(retval = kraken_send_message(kraken, data->fan_message, 2)) ||
			(retval = kraken_receive_message(kraken, data->status_message, 32))
		   )
			dev_err(&kraken->udev->dev, "Failed to update: %d\n", retval);
	}
	return retval;
}

static ssize_t show_speed(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->pump_message[1]);
}

static ssize_t set_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 speed;
	if (sscanf(buf, "%hhu", &speed) != 1 || speed < 30 || speed > 100)
		return -EINVAL;

	data->pump_message[1] = speed;
	data->fan_message[1] = speed;

	return count;
}

static DEVICE_ATTR(speed, S_IRUGO | S_IWUSR | S_IWGRP, show_speed, set_speed);

static ssize_t show_color(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n", data->color_message[1], data->color_message[2], data->color_message[3]);
}

static ssize_t set_color(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 r, g, b;
	if (sscanf(buf, "%02hhx%02hhx%02hhx", &r, &g, &b) != 3)
		return -EINVAL;

	data->color_message[1] = r;
	data->color_message[2] = g;
	data->color_message[3] = b;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(color, S_IRUGO | S_IWUSR | S_IWGRP, show_color, set_color);

static ssize_t show_alternate_color(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n", data->color_message[4], data->color_message[5], data->color_message[6]);
}

static ssize_t set_alternate_color(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 r, g, b;
	if (sscanf(buf, "%02hhx%02hhx%02hhx", &r, &g, &b) != 3)
		return -EINVAL;

	data->color_message[4] = r;
	data->color_message[5] = g;
	data->color_message[6] = b;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(alternate_color, S_IRUGO | S_IWUSR | S_IWGRP, show_alternate_color, set_alternate_color);

static ssize_t show_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->color_message[11]);
}

static ssize_t set_interval(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 interval;
	if (sscanf(buf, "%hhu", &interval) != 1 || interval == 0)
		return -EINVAL;

	data->color_message[11] = interval; data->color_message[12] = interval;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP, show_interval, set_interval);

static ssize_t show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	if (data->color_message[14] == 1)
		return scnprintf(buf, PAGE_SIZE, "alternating\n");
	else if (data->color_message[15] == 1)
		return scnprintf(buf, PAGE_SIZE, "blinking\n");
	else if (data->color_message[13] == 1)
		return scnprintf(buf, PAGE_SIZE, "normal\n");
	else
		return scnprintf(buf, PAGE_SIZE, "off\n");
}

static ssize_t set_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	if (strncasecmp(buf, "normal", strlen("normal")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 0;
		data->color_message[15] = 0;
	} else if (strncasecmp(buf, "alternating", strlen("alternating")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 1;
		data->color_message[15] = 0;
	} else if (strncasecmp(buf, "blinking", strlen("blinking")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 0;
		data->color_message[15] = 1;
	} else if (strncasecmp(buf, "off", strlen("off")) == 0) {
		data->color_message[13] = 0;
		data->color_message[14] = 0;
		data->color_message[15] = 0;
	} else
		return -EINVAL;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP, show_mode, set_mode);

static ssize_t show_temp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->status_message[10]);
}

static DEVICE_ATTR(temp, S_IRUGO, show_temp, NULL);

static ssize_t show_pump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", 256 * data->status_message[8] + data->status_message[9]);
}

static DEVICE_ATTR(pump, S_IRUGO, show_pump, NULL);

static ssize_t show_fan(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", 256 * data->status_message[0] + data->status_message[1]);
}

static DEVICE_ATTR(fan, S_IRUGO, show_fan, NULL);

static struct attribute *kraken_x61_group_attrs[] = {
	&dev_attr_speed.attr,
	&dev_attr_color.attr,
	&dev_attr_alternate_color.attr,
	&dev_attr_interval.attr,
	&dev_attr_mode.attr,
	&dev_attr_temp.attr,
	&dev_attr_pump.attr,
	&dev_attr_fan.attr,
	NULL,
};

static struct attribute_group kraken_x61_group = {
	.attrs = kraken_x61_group_attrs,
};

const struct attribute_group *kraken_driver_groups[] = {
	&kraken_x61_group,
	NULL,
};

int kraken_driver_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct kraken_driver_data *data;
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	int retval = -ENOMEM;
	kraken->data = kmalloc(sizeof(*kraken->data), GFP_KERNEL | GFP_DMA);
	if (!kraken->data)
		goto error_data;
	data = kraken->data;

	data->color_message[0] = 0x10;
	data->color_message[1] = 0x00; data->color_message[2] = 0x00; data->color_message[3] = 0xff;
	data->color_message[4] = 0x00; data->color_message[5] = 0xff; data->color_message[6] = 0x00;
	data->color_message[7] = 0x00; data->color_message[8] = 0x00; data->color_message[9] = 0x00; data->color_message[10] = 0x3c;
	data->color_message[11] = 1; data->color_message[12] = 1;
	data->color_message[13] = 1; data->color_message[14] = 0; data->color_message[15] = 0;
	data->color_message[16] = 0x00; data->color_message[17] = 0x00; data->color_message[18] = 0x01;

	data->pump_message[0] = 0x13;
	data->pump_message[1] = 50;

	data->fan_message[0] = 0x12;
	data->fan_message[1] = 50;

	retval = usb_control_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 0), 2, 0x40, 0x0002, 0, NULL, 0, 1000);
	if (retval)
		goto error;

	dev_info(&interface->dev, "Kraken connected\n");
	data->send_color = true;

	return 0;
error:
	kfree(data);
error_data:
	return retval;
}

void kraken_driver_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	struct kraken_driver_data *data = kraken->data;

	kfree(data);

	dev_info(&interface->dev, "Kraken disconnected\n");
}

static const struct usb_device_id kraken_x61_id_table[] = {
	{ USB_DEVICE(0x2433, 0xb200) },
	{ },
};

MODULE_DEVICE_TABLE(usb, kraken_x61_id_table);

static struct usb_driver kraken_x61_driver = {
	.name       = DRIVER_NAME,
	.probe      = kraken_probe,
	.disconnect = kraken_disconnect,
	.id_table   = kraken_x61_id_table,
};

module_usb_driver(kraken_x61_driver);

MODULE_DESCRIPTION("driver for 2433:b200 devices (NZXT Kraken X*1)");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.1");
