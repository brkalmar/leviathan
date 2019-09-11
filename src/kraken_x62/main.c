/* Driver for 1e71:170e devices.
 */

#include "led.h"
#include "percent.h"
#include "status.h"
#include "../common.h"
#include "../util.h"

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x62"

const char *driver_name = DRIVER_NAME;

#define DATA_SERIAL_NUMBER_SIZE ((size_t) 65)

struct driver_data {
	char serial_number[DATA_SERIAL_NUMBER_SIZE];

	struct status_data status;

	struct percent_data percent_fan;
	struct percent_data percent_pump;

	struct led_data led;
};

size_t driver_data_size(void)
{
	return sizeof(struct driver_data);
}

static void driver_data_init(struct driver_data *data)
{
	status_data_init(&data->status);
	percent_data_init(&data->percent_fan, PERCENT_MSG_WHICH_FAN);
	percent_data_init(&data->percent_pump, PERCENT_MSG_WHICH_PUMP);
	led_data_init(&data->led);
}

int driver_update(struct kraken_data *kdata)
{
	int ret;
	struct driver_data *data = kdata->data;
	if ((ret = status_data_update(kdata, &data->status)) ||
	    (ret = percent_data_update(kdata, &data->percent_fan)) ||
	    (ret = percent_data_update(kdata, &data->percent_pump)) ||
	    (ret = led_data_update(kdata, &data->led)))
		return ret;
	return 0;
}

u32 driver_get_temp(struct kraken_data *kdata)
{
	return status_data_temp_liquid(&kdata->data->status);
}

u32 driver_get_fan_rpm(struct kraken_data *kdata)
{
	return status_data_fan_rpm(&kdata->data->status);
}

u32 driver_get_pump_rpm(struct kraken_data *kdata)
{
	return status_data_pump_rpm(&kdata->data->status);
}

int driver_set_fan_percent(struct kraken_data *kdata, u32 value)
{
	if (value > U8_MAX)
		return -EINVAL;
	return percent_data_set(&kdata->data->percent_fan, value);
}

int driver_set_pump_percent(struct kraken_data *kdata, u32 value)
{
	if (value > U8_MAX)
		return -EINVAL;
	return percent_data_set(&kdata->data->percent_pump, value);
}

static ssize_t serial_no_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%s\n", kdata->data->serial_number);
}

static DEVICE_ATTR_RO(serial_no);

static ssize_t unknown_1_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kdata->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_unknown_1(status));
}

static DEVICE_ATTR_RO(unknown_1);

static ssize_t unknown_2_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kdata->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_unknown_2(status));
}

static DEVICE_ATTR_RO(unknown_2);

static ssize_t unknown_3_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kdata->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_unknown_3(status));
}

static DEVICE_ATTR_RO(unknown_3);

static struct attribute *x62_group_attrs[] = {
	&dev_attr_serial_no.attr,
	&dev_attr_unknown_1.attr,
	&dev_attr_unknown_2.attr,
	&dev_attr_unknown_3.attr,
	NULL,
};

static struct attribute_group x62_group = {
	.attrs = x62_group_attrs,
};

static ssize_t cycles_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_cycles(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(cycles);

static ssize_t preset_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_preset(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(preset);

static ssize_t moving_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_moving(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(moving);

static ssize_t direction_store(struct device *dev,
                               struct device_attribute *attr, const char *buf,
                               size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_direction(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(direction);

static ssize_t interval_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_interval(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(interval);

static ssize_t group_size_store(struct device *dev,
                                struct device_attribute *attr, const char *buf,
                                size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_group_size(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(group_size);

static ssize_t colors_logo_store(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_colors_logo(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(colors_logo);

static ssize_t colors_ring_store(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_colors_ring(&buf, &kdata->data->led);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(colors_ring);

static ssize_t which_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct kraken_data *kdata = usb_get_intfdata(to_usb_interface(dev));
	int ret = led_data_parse_which(&buf, &kdata->data->led, dev);
	if (ret)
		return -EINVAL;
	return count;
}

static DEVICE_ATTR_WO(which);

static struct attribute *x62_group_led_attrs[] = {
	&dev_attr_cycles.attr,
	&dev_attr_preset.attr,
	&dev_attr_moving.attr,
	&dev_attr_direction.attr,
	&dev_attr_interval.attr,
	&dev_attr_group_size.attr,
	&dev_attr_colors_logo.attr,
	&dev_attr_colors_ring.attr,
	&dev_attr_which.attr,
	NULL,
};

static struct attribute_group x62_group_led = {
	.attrs = x62_group_led_attrs,
	.name = "led",
};

const struct attribute_group *driver_groups[] = {
	&x62_group,
	&x62_group_led,
	NULL,
};

static int x62_initialize(struct kraken_data *kdata, char *serial_number)
{
	struct device *dev = kdata->dev;
	u8 len;
	u8 i;
	u8 *data;
	// Space for length byte, type-of-data byte, and serial number encoded
	// UTF-16.
	const size_t data_size = 2 + (DATA_SERIAL_NUMBER_SIZE - 1) * 2;
	int ret = kraken_usb_data(kdata, &data, data_size);
	if (ret)
		return ret;
	ret = usb_control_msg(
		kdata->udev, usb_rcvctrlpipe(kdata->udev, 0),
		0x06, 0x80, 0x0303, 0x0409, data, data_size, 1000);
	if (ret < 0) {
		dev_err(dev, "failed control message: %d\n", ret);
		return ret;
	}

	len = data[0] - 2;
	if (ret < 2 || data[1] != 0x03 || len % 2 != 0) {
		dev_err(dev, "data received is invalid: %d, %u, %#02x\n",
		        ret, data[0], data[1]);
		return 1;
	}
	len /= 2;
	if (len > DATA_SERIAL_NUMBER_SIZE - 1) {
		dev_err(dev, "data received is too long: %u\n", len);
		return 1;
	}
	// convert UTF-16 serial to null-terminated ASCII string
	for (i = 0; i < len; i++) {
		const u8 index_low = 2 + 2 * i;
		serial_number[i] = data[index_low];
		if (data[index_low + 1] != 0x00) {
			dev_err(dev,
			        "serial number contains non-ASCII character: "
			        "UTF-16 %#02x%02x, at index %u\n",
			        data[index_low + 1], data[index_low],
			        index_low);
			return 1;
		}
	}
	serial_number[i] = '\0';

	return 0;
}

int driver_probe(struct usb_interface *interface,
                 const struct usb_device_id *id)
{
	struct kraken_data *kdata = usb_get_intfdata(interface);
	struct driver_data *data = kdata->data;
	struct device *dev = kdata->dev;
	int ret;

	driver_data_init(data);

	ret = x62_initialize(kdata, data->serial_number);
	if (ret) {
		dev_err(dev, "failed to initialize: %d\n", ret);
		return ret;
	}

	return 0;
}

void driver_disconnect(struct usb_interface *interface)
{
}

static const struct usb_device_id x62_id_table[] = {
	{ USB_DEVICE(0x1e71, 0x170e) },
	{ },
};

MODULE_DEVICE_TABLE(usb, x62_id_table);

static struct usb_driver x62_driver = {
	.name       = DRIVER_NAME,
	.probe      = kraken_probe,
	.disconnect = kraken_disconnect,
	.id_table   = x62_id_table,
};

module_usb_driver(x62_driver);

MODULE_DESCRIPTION("driver for 1e71:170e devices (NZXT Kraken X*2)");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2.0");
