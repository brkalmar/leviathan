/* Handling of device status attributes.
 */

#include "status.h"
#include "../common.h"

#include <linux/printk.h>
#include <linux/string.h>
#include <linux/usb.h>

static const u8 MSG_HEADER_1[] = {
	0x04,
};
static const u8 MSG_FOOTER_1[] = {
	0x02, 0x00, 0x01, 0x08,
};

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

int kraken_x62_update_status(struct usb_kraken *kraken,
                             struct status_data *data)
{
	bool invalid;
	int received;
	int ret;
	mutex_lock(&data->mutex);
	ret = usb_interrupt_msg(kraken->udev, usb_rcvctrlpipe(kraken->udev, 1),
	                        data->msg, sizeof(data->msg), &received, 1000);
	mutex_unlock(&data->mutex);

	if (ret || received != sizeof(data->msg)) {
		dev_err(&kraken->udev->dev,
		        "failed status update: I/O error\n");
		return ret ? ret : 1;
	}
	// check header #1 & footer #1
	invalid = false;
	if (memcmp(data->msg + 0, MSG_HEADER_1, sizeof(MSG_HEADER_1)) != 0 ||
	    memcmp(data->msg + 11, MSG_FOOTER_1, sizeof(MSG_FOOTER_1)) != 0) {
		char status_hex[sizeof(data->msg) * 3 + 1];
		hex_dump_to_buffer(data->msg, sizeof(data->msg), 32, 1,
		                   status_hex, sizeof(status_hex), false);
		dev_err(&kraken->udev->dev,
		        "received invalid status message: %s\n", status_hex);
		return 1;
	}
	return 0;
}
