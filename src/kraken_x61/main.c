/* Driver for 2433:b200 devices.
 */

#include "../common.h"
#include "../util.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x61"

const char *driver_name = DRIVER_NAME;

struct driver_data {
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

size_t driver_data_size(void)
{
	return sizeof(struct driver_data);
}

static int x61_start_transaction(struct kraken_data *kdata)
{
	return usb_control_msg(kdata->udev, usb_sndctrlpipe(kdata->udev, 0), 2,
	                       0x40, 0x0001, 0, NULL, 0, 1000);
}

static int x61_message_send(struct kraken_data *kdata, u8 *message, int length)
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

static int x61_message_recv(struct kraken_data *kdata, u8 *message,
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

int driver_update(struct kraken_data *kdata)
{
	int ret = 0;
	struct driver_data *data = kdata->data;
	if (data->send_color) {
		if ((ret = x61_start_transaction(kdata)) ||
		    (ret = x61_message_send(kdata, data->color_message, 19)) ||
		    (ret = x61_message_recv(kdata, data->status_message, 32)))
			;
	} else {
		if ((ret = x61_start_transaction(kdata)) ||
		    (ret = x61_message_send(kdata, data->pump_message, 2)) ||
		    (ret = x61_message_send(kdata, data->fan_message, 2)) ||
		    (ret = x61_message_recv(kdata, data->status_message, 32)))
			;
	}
	data->send_color = false;
	return ret;
}

u32 driver_get_temp(struct kraken_data *kdata)
{
	return kdata->data->status_message[10];
}

u32 driver_get_fan_rpm(struct kraken_data *kdata)
{
	struct driver_data *data = kdata->data;
	return 256 * data->status_message[0] + data->status_message[1];
}

u32 driver_get_pump_rpm(struct kraken_data *kdata)
{
	struct driver_data *data = kdata->data;
	return 256 * data->status_message[8] + data->status_message[9];
}

int driver_set_fan_percent(struct kraken_data *kdata, u32 value)
{
	if (value < 30 || value > 100)
		return -EINVAL;
	kdata->data->fan_message[1] = value;
	return 0;
}

int driver_set_pump_percent(struct kraken_data *kdata, u32 value)
{
	if (value < 30 || value > 100)
		return -EINVAL;
	kdata->data->pump_message[1] = value;
	return 0;
}

static ssize_t color_main_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct driver_data *data = kdata->data;
	struct kraken_color color;
	int ret = kraken_parse_color(&buf, &color);
	if (ret)
		return ret;
	data->color_message[1] = color.red;
	data->color_message[2] = color.green;
	data->color_message[3] = color.blue;
	return count;
}

static DEVICE_ATTR_WO(color_main);

static ssize_t color_alternate_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct driver_data *data = kdata->data;
	struct kraken_color color;
	int ret = kraken_parse_color(&buf, &color);
	if (ret)
		return -EINVAL;
	data->color_message[4] = color.red;
	data->color_message[5] = color.green;
	data->color_message[6] = color.blue;
	return count;
}

static DEVICE_ATTR_WO(color_alternate);

static ssize_t interval_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct driver_data *data = kdata->data;
	int read;
	u8 interval;
	int ret = sscanf(buf, "%hhu%n", &interval, &read);
	if (ret != 1 || interval < 1)
		return -EINVAL;
	buf += read;
	data->color_message[11] = interval;
	data->color_message[12] = interval;
	return count;
}

static DEVICE_ATTR_WO(interval);

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct driver_data *data = kdata->data;
	struct kraken_parse_enum words[] = {
		{"normal",      1 << 16 | 0 << 8 | 0 << 0},
		{"alternating", 1 << 16 | 1 << 8 | 0 << 0},
		{"blinking",    1 << 16 | 0 << 8 | 1 << 0},
		{"off",         0 << 16 | 0 << 8 | 0 << 0},
		{NULL, 0},
	};
	u64 mode;
	int ret = kraken_parse_enum(&buf, words, &mode);
	if (ret)
		return -EINVAL;
	data->color_message[13] = mode >> 16 & 0xFF;
	data->color_message[14] = mode >>  8 & 0xFF;
	data->color_message[15] = mode >>  0 & 0xFF;
	data->send_color = true;
	return count;
}

static DEVICE_ATTR_WO(mode);

static struct attribute *x61_group_led_attrs[] = {
	&dev_attr_color_main.attr,
	&dev_attr_color_alternate.attr,
	&dev_attr_interval.attr,
	&dev_attr_mode.attr,
	NULL,
};

static struct attribute_group x61_group_led = {
	.attrs = x61_group_led_attrs,
	.name = "led",
};

const struct attribute_group *driver_groups[] = {
	&x61_group_led,
	NULL,
};

int driver_probe(struct usb_interface *interface,
                 const struct usb_device_id *id)
{
	struct kraken_data *kdata = usb_get_intfdata(interface);
	struct driver_data *data = kdata->data;
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

void driver_disconnect(struct usb_interface *interface)
{
}

static const struct usb_device_id x61_id_table[] = {
	{ USB_DEVICE(0x2433, 0xb200) },
	{ },
};

MODULE_DEVICE_TABLE(usb, x61_id_table);

static struct usb_driver x61_driver = {
	.name       = DRIVER_NAME,
	.probe      = kraken_probe,
	.disconnect = kraken_disconnect,
	.id_table   = x61_id_table,
};

module_usb_driver(x61_driver);

MODULE_DESCRIPTION("driver for 2433:b200 devices (NZXT Kraken X*1)");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.1");
