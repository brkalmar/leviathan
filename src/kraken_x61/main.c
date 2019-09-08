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

size_t kraken_driver_data_size(void)
{
	return sizeof(struct kraken_driver_data);
}

static int kraken_start_transaction(struct kraken_data *kdata)
{
	return usb_control_msg(kdata->udev, usb_sndctrlpipe(kdata->udev, 0), 2,
	                       0x40, 0x0001, 0, NULL, 0, 1000);
}

static int kraken_send_message(struct kraken_data *kdata, u8 *message,
                               int length)
{
	int sent;
	u8 *data;
	int ret = kraken_usb_data(kdata, &data, length);
	if (ret)
		return ret;
	memcpy(data, message, length);
	ret = usb_bulk_msg(kdata->udev, usb_sndbulkpipe(kdata->udev, 2),
	                      data, length, &sent, 3000);
	if (ret)
		return ret;
	if (sent != length)
		return -EIO;
	return 0;
}

static int kraken_receive_message(struct kraken_data *kdata, u8 *message,
                                  int expected_length)
{
	int received;
	u8 *data;
	int ret = kraken_usb_data(kdata, &data, expected_length);
	if (ret)
		return ret;
	ret = usb_bulk_msg(kdata->udev, usb_rcvbulkpipe(kdata->udev, 2),
	                      data, expected_length, &received, 3000);
	if (ret)
		return ret;
	if (received != expected_length)
		return -EIO;
	memcpy(message, data, expected_length);
	return 0;
}

int kraken_driver_update(struct kraken_data *kdata)
{
	int ret = 0;
	struct kraken_driver_data *data = kdata->data;
	if (data->send_color) {
		if ((ret = kraken_start_transaction(kdata)) ||
		    (ret = kraken_send_message(
			    kdata, data->color_message, 19)) ||
		    (ret = kraken_receive_message(
			    kdata, data->status_message, 32)))
			;
	} else {
		if ((ret = kraken_start_transaction(kdata)) ||
		    (ret = kraken_send_message(kdata, data->pump_message, 2)) ||
		    (ret = kraken_send_message(kdata, data->fan_message, 2)) ||
		    (ret = kraken_receive_message(
			    kdata, data->status_message, 32)))
			;
	}
	data->send_color = false;
	return ret;
}

u32 kraken_driver_get_temp(struct kraken_data *kdata)
{
	return kdata->data->status_message[10];
}

u32 kraken_driver_get_fan_rpm(struct kraken_data *kdata)
{
	struct kraken_driver_data *data = kdata->data;
	return 256 * data->status_message[0] + data->status_message[1];
}

static ssize_t show_speed(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->pump_message[1]);
}

static ssize_t set_speed(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	u8 speed;
	if (sscanf(buf, "%hhu", &speed) != 1 || speed < 30 || speed > 100)
		return -EINVAL;

	data->pump_message[1] = speed;
	data->fan_message[1] = speed;

	return count;
}

static DEVICE_ATTR(speed, S_IRUGO | S_IWUSR | S_IWGRP, show_speed, set_speed);

static ssize_t show_color(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n",
	                 data->color_message[1], data->color_message[2],
	                 data->color_message[3]);
}

static ssize_t set_color(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

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

static ssize_t show_alternate_color(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n",
	                 data->color_message[4], data->color_message[5],
	                 data->color_message[6]);
}

static ssize_t set_alternate_color(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	u8 r, g, b;
	if (sscanf(buf, "%02hhx%02hhx%02hhx", &r, &g, &b) != 3)
		return -EINVAL;

	data->color_message[4] = r;
	data->color_message[5] = g;
	data->color_message[6] = b;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(alternate_color, S_IRUGO | S_IWUSR | S_IWGRP,
                   show_alternate_color, set_alternate_color);

static ssize_t show_interval(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->color_message[11]);
}

static ssize_t set_interval(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	u8 interval;
	if (sscanf(buf, "%hhu", &interval) != 1 || interval == 0)
		return -EINVAL;

	data->color_message[11] = interval; data->color_message[12] = interval;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP, show_interval,
                   set_interval);

static ssize_t show_mode(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	if (data->color_message[14] == 1)
		return scnprintf(buf, PAGE_SIZE, "alternating\n");
	else if (data->color_message[15] == 1)
		return scnprintf(buf, PAGE_SIZE, "blinking\n");
	else if (data->color_message[13] == 1)
		return scnprintf(buf, PAGE_SIZE, "normal\n");
	else
		return scnprintf(buf, PAGE_SIZE, "off\n");
}

static ssize_t set_mode(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;

	if (strncasecmp(buf, "normal", strlen("normal")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 0;
		data->color_message[15] = 0;
	} else if (strncasecmp(buf, "alternating",
	                       strlen("alternating")) == 0) {
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

static ssize_t show_pump(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kdata->data;
	const u16 rpm = 256 * data->status_message[8] + data->status_message[9];
	return scnprintf(buf, PAGE_SIZE, "%u\n", rpm);
}

static DEVICE_ATTR(pump, S_IRUGO, show_pump, NULL);

static struct attribute *kraken_x61_group_attrs[] = {
	&dev_attr_speed.attr,
	&dev_attr_color.attr,
	&dev_attr_alternate_color.attr,
	&dev_attr_interval.attr,
	&dev_attr_mode.attr,
	&dev_attr_pump.attr,
	NULL,
};

static struct attribute_group kraken_x61_group = {
	.attrs = kraken_x61_group_attrs,
};

const struct attribute_group *kraken_driver_groups[] = {
	&kraken_x61_group,
	NULL,
};

int kraken_driver_probe(struct usb_interface *interface,
                        const struct usb_device_id *id)
{
	struct kraken_data *kdata = usb_get_intfdata(interface);
	struct kraken_driver_data *data = kdata->data;
	int ret;

	data->color_message[0]  = 0x10;

	data->color_message[1]  = 0x00;
	data->color_message[2]  = 0x00;
	data->color_message[3]  = 0xff;

	data->color_message[4]  = 0x00;
	data->color_message[5]  = 0xff;
	data->color_message[6]  = 0x00;

	data->color_message[7]  = 0x00;
	data->color_message[8]  = 0x00;
	data->color_message[9]  = 0x00;
	data->color_message[10] = 0x3c;

	data->color_message[11] = 0x01;
	data->color_message[12] = 0x01;

	data->color_message[13] = 0x01;
	data->color_message[14] = 0x00;
	data->color_message[15] = 0x00;

	data->color_message[16] = 0x00;
	data->color_message[17] = 0x00;
	data->color_message[18] = 0x01;

	data->pump_message[0] = 0x13;
	data->pump_message[1] = 50;

	data->fan_message[0] = 0x12;
	data->fan_message[1] = 50;

	data->send_color = true;

	ret = usb_control_msg(kdata->udev, usb_sndctrlpipe(kdata->udev, 0), 2,
	                      0x40, 0x0002, 0, NULL, 0, 1000);
	if (ret)
		return ret;

	return 0;
}

void kraken_driver_disconnect(struct usb_interface *interface)
{
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
