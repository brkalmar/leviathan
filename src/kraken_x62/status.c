/* Handling of device status attributes.
 */

#include "status.h"
#include "../common.h"

#include <linux/printk.h>
#include <linux/string.h>
#include <linux/usb.h>

void status_data_init(struct status_data *data)
{
	mutex_init(&data->mutex);
}

u8 status_data_temp_liquid(struct status_data *data)
{
	u8 temp;
	mutex_lock(&data->mutex);
	temp = data->msg[1];
	mutex_unlock(&data->mutex);

	return temp;
}

u16 status_data_fan_rpm(struct status_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->mutex);
	rpm_be = *((u16 *) (data->msg + 3));
	mutex_unlock(&data->mutex);

	return be16_to_cpu(rpm_be);
}

u16 status_data_pump_rpm(struct status_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->mutex);
	rpm_be = *((u16 *) (data->msg + 5));
	mutex_unlock(&data->mutex);

	return be16_to_cpu(rpm_be);
}

// TODO: [undocumented] figure out what this is
u8 status_data_unknown_1(struct status_data *data)
{
	u8 unknown_1;
	mutex_lock(&data->mutex);
	unknown_1 = data->msg[2];
	mutex_unlock(&data->mutex);

	return unknown_1;
}

// TODO: [undocumented] figure out what this is
u32 status_data_unknown_2(struct status_data *data)
{
	u32 unknown_2_be;
	mutex_lock(&data->mutex);
	unknown_2_be = *((u32 *) (data->msg + 7));
	mutex_unlock(&data->mutex);

	return be32_to_cpu(unknown_2_be);
}

// TODO: [undocumented] figure out what this is
u16 status_data_unknown_3(struct status_data *data)
{
	u16 unknown_3_be;
	mutex_lock(&data->mutex);
	unknown_3_be = *((u16 *) (data->msg + 15));
	mutex_unlock(&data->mutex);

	return be16_to_cpu(unknown_3_be);
}

int kraken_x62_update_status(struct kraken_data *kdata,
                             struct status_data *data)
{
	struct device *dev = kdata->dev;
	u8 *usb_msg;
	int received;

	int ret = kraken_usb_data(kdata, &usb_msg, sizeof(data->msg));
	if (ret)
		return ret;
	ret = usb_interrupt_msg(kdata->udev, usb_rcvctrlpipe(kdata->udev, 1),
	                        usb_msg, sizeof(data->msg), &received, 1000);
	if (ret || received != sizeof(data->msg)) {
		dev_err(dev, "failed status update: I/O error\n");
		return ret ? ret : 1;
	}
	// NOTE: We do not check the header or footer to ensure best possible
	// compatiblity, as we do not know their purpose.

	mutex_lock(&data->mutex);
	memcpy(data->msg, usb_msg, sizeof(data->msg));
	mutex_unlock(&data->mutex);

	return 0;
}
