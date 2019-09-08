/* Handling of percent-attributes.
 */

#include "percent.h"
#include "../common.h"
#include "../util.h"

static const u8 PERCENT_MSG_HEADER[] = {
	0x02, 0x4d,
};

static void percent_msg_init(struct percent_msg *msg,
                             enum percent_msg_which which)
{
	memcpy(msg->msg, PERCENT_MSG_HEADER, sizeof(PERCENT_MSG_HEADER));
	msg->msg[2] = (u8) which;
}

static u8 percent_msg_get(struct percent_msg *msg)
{
	return msg->msg[4];
}

static void percent_msg_set(struct percent_msg *msg, u8 percent)
{
	msg->msg[4] = percent;
}

static void percent_data_set(struct percent_data *data, u8 percent)
{
	percent_msg_set(&data->msg, percent);
}

void percent_data_init(struct percent_data *data, enum percent_msg_which which)
{
	switch (which) {
	case PERCENT_MSG_WHICH_FAN:
		data->percent_min = 35;
		data->percent_max = 100;
		break;
	case PERCENT_MSG_WHICH_PUMP:
		data->percent_min = 50;
		data->percent_max = 100;
		break;
	}

	percent_msg_init(&data->msg, which);
	// this will never be confused for a real percentage
	data->prev = U8_MAX;
	data->update = false;

	mutex_init(&data->mutex);
}

int percent_data_parse(struct percent_data *data, struct device *dev,
                       const char *attr, const char *buf)
{
	char percent_str[WORD_LEN_MAX];
	unsigned int percent_ui;
	u8 percent;

	int ret = str_scan_word(&buf, percent_str);
	if (ret) {
		dev_warn(dev, "%s: missing percent\n", attr);
		goto error;
	}
	ret = kstrtouint(percent_str, 0, &percent_ui);
	if (ret) {
		dev_warn(dev, "%s: invalid percent %s\n", attr, percent_str);
		goto error;
	}
	if (buf[0] != '\0') {
		dev_warn(dev, "%s: unrecognized data left in buffer: `%s'\n",
		         attr, buf);
		ret = 1;
		goto error;
	}

	mutex_lock(&data->mutex);

	if (percent_ui < data->percent_min) {
		percent = data->percent_min;
	} else if (percent_ui > data->percent_max) {
		percent = data->percent_max;
	} else {
		percent = percent_ui;
	}
	percent_data_set(data, percent);

	data->update = true;
	mutex_unlock(&data->mutex);
	return 0;

error:
	mutex_lock(&data->mutex);
	data->update = false;
	mutex_unlock(&data->mutex);
	return ret;
}

int kraken_x62_update_percent(struct kraken_data *kdata,
                              struct percent_data *data)
{
	struct percent_msg *usb_msg;
	int sent;
	u8 curr;
	int ret = 0;

	mutex_lock(&data->mutex);

	if (!data->update ||
	    (curr = percent_msg_get(&data->msg)) == data->prev)
		goto error_usb_msg;

	ret = kraken_usb_data(kdata, (u8 **)&usb_msg, sizeof(*usb_msg));
	if (ret)
		goto error_usb_msg;
	memcpy(usb_msg, &data->msg, sizeof(*usb_msg));

	data->prev = curr;
	data->update = false;

	mutex_unlock(&data->mutex);

	ret = usb_interrupt_msg(
		kdata->udev, usb_sndctrlpipe(kdata->udev, 1),
		usb_msg->msg, sizeof(usb_msg->msg), &sent, 1000);
	if (ret || sent != sizeof(usb_msg->msg)) {
		dev_err(&kdata->udev->dev,
		        "failed to set speed percent: I/O error\n");
		return ret ? ret : 1;
	}

	return 0;
error_usb_msg:
	mutex_unlock(&data->mutex);
	return ret;
}
