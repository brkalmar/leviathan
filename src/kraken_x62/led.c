/* Handling of LED attributes.
 */

#include "led.h"
#include "../util.h"

#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb.h>

enum led_which {
	LED_WHICH_SYNC = 0b000,
	LED_WHICH_LOGO = 0b001,
	LED_WHICH_RING = 0b010,
};

static void led_msg_which(struct led_msg *msg, enum led_which which)
{
	msg->msg[2] &= ~(0b111 << 0);
	msg->msg[2] |= ((u8) which) << 0;
}

static enum led_which led_msg_which_get(const struct led_msg *msg)
{
	const enum led_which which = (msg->msg[2] >> 0) & 0b111;
	return which;
}

enum led_preset {
	LED_PRESET_FIXED            = 0x00,
	LED_PRESET_FADING           = 0x01,
	LED_PRESET_SPECTRUM_WAVE    = 0x02,
	LED_PRESET_MARQUEE          = 0x03,
	LED_PRESET_COVERING_MARQUEE = 0x04,
	LED_PRESET_ALTERNATING      = 0x05,
	LED_PRESET_BREATHING        = 0x06,
	LED_PRESET_PULSE            = 0x07,
	LED_PRESET_TAI_CHI          = 0x08,
	LED_PRESET_WATER_COOLER     = 0x09,
	LED_PRESET_LOAD             = 0x0a,
};

#define LED_PRESET_DEFAULT LED_PRESET_LOAD

static void led_msg_preset(struct led_msg *msg, enum led_preset preset)
{
	msg->msg[3] = (u8) preset;
}

static enum led_preset led_msg_preset_get(const struct led_msg *msg)
{
	const enum led_preset preset = msg->msg[3];
	return preset;
}

static bool led_msg_preset_is_legal(const struct led_msg *msg)
{
	const enum led_preset preset = led_msg_preset_get(msg);
	// ring leds accept any preset
	if (led_msg_which_get(msg) == LED_WHICH_RING)
		return true;
	// logo/sync leds accepts only the following presets:
	switch (preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		return true;
	default:
		return false;
	}
}

#define LED_MOVING_DEFAULT false

static void led_msg_moving(struct led_msg *msg, bool moving)
{
	msg->msg[2] &= ~(0b1 << 3);
	msg->msg[2] |= ((u8) moving) << 3;
}

static bool led_msg_moving_get(const struct led_msg *msg)
{
	const bool moving = (msg->msg[2] >> 3) & 0b1;
	return moving;
}

static bool led_msg_moving_is_legal(const struct led_msg *msg) {
	const bool moving = led_msg_moving_get(msg);
	if (moving == LED_MOVING_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_ALTERNATING:
		return true;
	default:
		return false;
	}
}

enum led_direction {
	LED_DIRECTION_CLOCKWISE        = 0b0000,
	LED_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

#define LED_DIRECTION_DEFAULT LED_DIRECTION_CLOCKWISE

static void led_msg_direction(struct led_msg *msg, enum led_direction direction)
{
	msg->msg[2] &= ~(0b1111 << 4);
	msg->msg[2] |= ((u8) direction) << 4;
}

static enum led_direction led_msg_direction_get(const struct led_msg *msg)
{
	const enum led_direction direction = (msg->msg[2] >> 4) & 0b1111;
	return direction;
}

static bool led_msg_direction_is_legal(const struct led_msg *msg)
{
	const enum led_direction direction = led_msg_direction_get(msg);
	if (direction == LED_DIRECTION_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
		return true;
	default:
		return false;
	}
}

enum led_interval {
	LED_INTERVAL_SLOWEST = 0b000,
	LED_INTERVAL_SLOWER  = 0b001,
	LED_INTERVAL_NORMAL  = 0b010,
	LED_INTERVAL_FASTER  = 0b011,
	LED_INTERVAL_FASTEST = 0b100,
};

#define LED_INTERVAL_DEFAULT LED_INTERVAL_NORMAL

static void led_msg_interval(struct led_msg *msg, enum led_interval interval)
{
	msg->msg[4] &= ~(0b111 << 0);
	msg->msg[4] |= ((u8) interval) << 0;
}

static enum led_interval led_msg_interval_get(const struct led_msg *msg)
{
	const enum led_interval interval = (msg->msg[4] >> 0) & 0b111;
	return interval;
}

static bool led_msg_interval_is_legal(const struct led_msg *msg)
{
	const enum led_interval interval = led_msg_interval_get(msg);
	if (interval == LED_INTERVAL_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
	case LED_PRESET_TAI_CHI:
	case LED_PRESET_WATER_COOLER:
		return true;
	default:
		return false;
	}
}

#define LED_GROUP_SIZE_MIN     ((u8) 3)
#define LED_GROUP_SIZE_MAX     ((u8) 6)
#define LED_GROUP_SIZE_DEFAULT ((u8) 3)

static void led_msg_group_size(struct led_msg *msg, u8 group_size)
{
	group_size -= LED_GROUP_SIZE_MIN;
	msg->msg[4] &= ~(0b11 << 3);
	msg->msg[4] |= (group_size & 0b11) << 3;
}

static u8 led_msg_group_size_get(const struct led_msg *msg)
{
	const u8 group_size = (msg->msg[4] >> 3) & 0b11;
	return group_size + LED_GROUP_SIZE_MIN;
}

static bool led_msg_group_size_is_legal(const struct led_msg *msg)
{
	const u8 group_size = led_msg_group_size_get(msg);
	if (group_size == LED_GROUP_SIZE_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_MARQUEE:
		return true;
	default:
		return false;
	}
}

static void led_msg_cycle(struct led_msg *msg, u8 cycle)
{
	cycle &= 0b111;
	msg->msg[4] &= ~(0b111 << 5);
	msg->msg[4] |= cycle << 5;
}

static void led_msg_color_logo(struct led_msg *msg,
                               const struct kraken_color *color)
{
	// NOTE: the logo color is in GRB format
	msg->msg[5] = color->green;
	msg->msg[6] = color->red;
	msg->msg[7] = color->blue;
}

#define LED_MSG_COLORS_RING ((size_t) 8)

static void led_msg_colors_ring(struct led_msg *msg,
                                const struct kraken_color *colors)
{
	size_t i;
	for (i = 0; i < LED_MSG_COLORS_RING; i++) {
		u8 *start = msg->msg + 8 + i * 3;
		start[0] = colors[i].red;
		start[1] = colors[i].green;
		start[2] = colors[i].blue;
	}
}

static const u8 LED_MSG_HEADER[] = {
	0x02, 0x4c,
};

static void led_msg_init(struct led_msg *msg)
{
	memcpy(msg->msg, LED_MSG_HEADER, sizeof(LED_MSG_HEADER));
	led_msg_preset(msg, LED_PRESET_DEFAULT);
	led_msg_moving(msg, LED_MOVING_DEFAULT);
	led_msg_direction(msg, LED_DIRECTION_DEFAULT);
	led_msg_interval(msg, LED_INTERVAL_DEFAULT);
	led_msg_group_size(msg, LED_GROUP_SIZE_DEFAULT);
}

static bool led_batch_preset_is_legal(const struct led_batch *batch)
{
	const enum led_preset preset = led_msg_preset_get(&batch->cycles[0]);
	switch (preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_WATER_COOLER:
	case LED_PRESET_LOAD:
		return batch->len == 1;
		break;
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_TAI_CHI:
		return batch->len == 2;
		break;
	case LED_PRESET_FADING:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		return batch->len >= 1 && batch->len <= LED_BATCH_CYCLES_SIZE;
		break;
	}
	return false;
}

static void led_batch_init(struct led_batch *batch)
{
	u8 i;
	for (i = 0; i < ARRAY_SIZE(batch->cycles); i++) {
		struct led_msg *msg = &batch->cycles[i];
		led_msg_init(msg);
		led_msg_cycle(msg, i);
	}
	batch->len = 1;
}

static bool led_data_colors_logo_is_legal(const struct led_data *data)
{
	switch (led_msg_which_get(&data->batch.cycles[0])) {
	case LED_WHICH_RING:
		return true;
		break;
	case LED_WHICH_LOGO:
	case LED_WHICH_SYNC:
		return data->batch.len <= data->colors_logo;
		break;
	}
	return false;
}

static bool led_data_colors_ring_is_legal(const struct led_data *data)
{
	switch (led_msg_which_get(&data->batch.cycles[0])) {
	case LED_WHICH_LOGO:
		return true;
		break;
	case LED_WHICH_RING:
	case LED_WHICH_SYNC:
		return data->batch.len <= data->colors_ring;
		break;
	}
	return false;
}

static int led_data_check(struct led_data *data, struct device *dev)
{
	if (!led_batch_preset_is_legal(&data->batch)) {
		dev_warn(dev, "illegal preset for specified %u cycles\n",
		         data->batch.len);
		return -EINVAL;
	}
	if (!led_msg_preset_is_legal(&data->batch.cycles[0])) {
		dev_warn(dev, "illegal preset for specified leds\n");
		return -EINVAL;
	}
	if (!led_msg_moving_is_legal(&data->batch.cycles[0])) {
		dev_warn(dev, "illegal moving for specified preset\n");
		return -EINVAL;
	}
	if (!led_msg_direction_is_legal(&data->batch.cycles[0])) {
		dev_warn(dev, "illegal direction for specified preset\n");
		return -EINVAL;
	}
	if (!led_msg_interval_is_legal(&data->batch.cycles[0])) {
		dev_warn(dev, "illegal interval for specified preset\n");
		return -EINVAL;
	}
	if (!led_msg_group_size_is_legal(&data->batch.cycles[0])) {
		dev_warn(dev, "illegal group size for specified preset\n");
		return -EINVAL;
	}
	if (!led_data_colors_logo_is_legal(data)) {
		dev_warn(dev, "only %u logo colors set for %u cycles\n",
		         data->colors_logo, data->batch.len);
		return -EINVAL;
	}
	if (!led_data_colors_ring_is_legal(data)) {
		dev_warn(dev, "only %u ring colors set for %u cycles\n",
		         data->colors_ring, data->batch.len);
		return -EINVAL;
	}
	return 0;
}

int led_data_parse_cycles(const char **buf, struct led_data *data)
{
	int read;
	u8 cycles;
	int ret = sscanf(*buf, "%hhu%n", &cycles, &read);
	if (ret != 1 || cycles < 1 || cycles > LED_BATCH_CYCLES_SIZE)
		return -EINVAL;
	*buf += read;

	mutex_lock(&data->mutex);
	data->batch.len = cycles;
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_preset(const char **buf, struct led_data *data)
{
	size_t i;
	struct kraken_parse_enum words[] = {
		{"fixed",            LED_PRESET_FIXED},
		{"fading",           LED_PRESET_FADING},
		{"spectrum_wave",    LED_PRESET_SPECTRUM_WAVE},
		{"marquee",          LED_PRESET_MARQUEE},
		{"covering_marquee", LED_PRESET_COVERING_MARQUEE},
		{"alternating",      LED_PRESET_ALTERNATING},
		{"breathing",        LED_PRESET_BREATHING},
		{"pulse",            LED_PRESET_PULSE},
		{"tai_chi",          LED_PRESET_TAI_CHI},
		{"water_cooler",     LED_PRESET_WATER_COOLER},
		{"load",             LED_PRESET_LOAD},
		{NULL, 0},
	};
	enum led_preset preset;
	int ret = kraken_parse_enum(buf, words, &preset);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->mutex);
	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_preset(&data->batch.cycles[i], preset);
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_moving(const char **buf, struct led_data *data)
{
	size_t i;
	bool moving;
	int ret = kraken_parse_bool(buf, &moving);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->mutex);
	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_moving(&data->batch.cycles[i], moving);
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_direction(const char **buf, struct led_data *data)
{
	size_t i;
	struct kraken_parse_enum words[] = {
		{"forward",  LED_DIRECTION_CLOCKWISE},
		{"backward", LED_DIRECTION_COUNTERCLOCKWISE},
		{NULL, 0},
	};
	enum led_direction direction;
	int ret = kraken_parse_enum(buf, words, &direction);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->mutex);
	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_direction(&data->batch.cycles[i], direction);
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_interval(const char **buf, struct led_data *data)
{
	size_t i;
	struct kraken_parse_enum words[] = {
		{"slowest", LED_INTERVAL_SLOWEST},
		{"slower",  LED_INTERVAL_SLOWER},
		{"normal",  LED_INTERVAL_NORMAL},
		{"faster",  LED_INTERVAL_FASTER},
		{"fastest", LED_INTERVAL_FASTEST},
		{NULL, 0},
	};
	enum led_interval interval;
	int ret = kraken_parse_enum(buf, words, &interval);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->mutex);
	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_interval(&data->batch.cycles[i], interval);
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_group_size(const char **buf, struct led_data *data)
{
	size_t i;
	int read;
	u8 group_size;
	int ret = sscanf(*buf, "%hhu%n", &group_size, &read);
	if (ret != 1 ||
	    group_size < LED_GROUP_SIZE_MIN || group_size > LED_GROUP_SIZE_MAX)
		return -EINVAL;

	mutex_lock(&data->mutex);
	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_group_size(&data->batch.cycles[i], group_size);
	mutex_unlock(&data->mutex);
	return 0;
}

int led_data_parse_colors_logo(const char **buf, struct led_data *data)
{
	u8 i;
	struct kraken_color color;

	// first color: must be there
	int ret = kraken_parse_color(buf, &color);

	mutex_lock(&data->mutex);

	if (ret) {
		data->colors_logo = 0;
		ret = -EINVAL;
		goto error;
	}
	led_msg_color_logo(&data->batch.cycles[0], &color);

	// remaining colors: may not be there
	for (i = 1; i < ARRAY_SIZE(data->batch.cycles); i++) {
		ret = kraken_parse_color(buf, &color);
		if (ret)
			break;
		led_msg_color_logo(&data->batch.cycles[i], &color);
	}

	data->colors_logo = i;

	ret = 0;
error:
	mutex_unlock(&data->mutex);
	return ret;
}

static int parse_ring(const char **buf, struct kraken_color *colors)
{
	u8 i;
	int ret = kraken_parse_color(buf, &colors[0]);
	if (ret)
		return 1;
	for (i = 1; i < LED_MSG_COLORS_RING; i++) {
		ret = kraken_parse_color(buf, &colors[i]);
		if (ret)
			return -EINVAL;
	}
	return 0;
}

int led_data_parse_colors_ring(const char **buf, struct led_data *data)
{
	u8 i;
	struct kraken_color colors[LED_MSG_COLORS_RING];

	// first set of colors
	int ret = parse_ring(buf, colors);

	mutex_lock(&data->mutex);

	if (ret) {
		data->colors_ring = 0;
		ret = -EINVAL;
		goto error;
	}
	led_msg_colors_ring(&data->batch.cycles[0], colors);

	// remaining sets of colors
	for (i = 1; i < ARRAY_SIZE(data->batch.cycles); i++) {
		ret = parse_ring(buf, colors);
		if (ret == 1) {
			break;
		} else if (ret < 0) {
			data->colors_ring = 0;
			ret = -EINVAL;
			goto error;
		}
		led_msg_colors_ring(&data->batch.cycles[i], colors);
	}

	data->colors_ring = i;

	ret = 0;
error:
	mutex_unlock(&data->mutex);
	return ret;
}

int led_data_parse_which(const char **buf, struct led_data *data,
                         struct device *dev)
{
	size_t i;
	struct kraken_parse_enum words[] = {
		{"logo", LED_WHICH_LOGO},
		{"ring", LED_WHICH_RING},
		{"sync", LED_WHICH_SYNC},
		{NULL, 0},
	};
	enum led_which which;
	int ret = kraken_parse_enum(buf, words, &which);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->mutex);

	for (i = 0; i < ARRAY_SIZE(data->batch.cycles); i++)
		led_msg_which(&data->batch.cycles[i], which);

	ret = led_data_check(data, dev);
	if (ret)
		goto error;
	data->update = true;

	ret = 0;
error:
	mutex_unlock(&data->mutex);
	return ret;
}

void led_data_init(struct led_data *data)
{
	led_batch_init(&data->batch);
	// this will never be confused for a real batch
	data->prev.len = 0;
	data->update = false;
	data->colors_logo = 0;
	data->colors_ring = 0;

	mutex_init(&data->mutex);
}

int kraken_x62_update_led(struct kraken_data *kdata, struct led_data *data)
{
	struct device *dev = kdata->dev;
	struct led_batch *usb_batch;
	int sent;
	u8 i;
	int ret = 0;

	mutex_lock(&data->mutex);

	if (!data->update ||
	    memcmp(&data->batch, &data->prev, sizeof(data->batch)) == 0)
		goto error_usb_batch;

	ret = kraken_usb_data(kdata, (u8 **)&usb_batch, sizeof(*usb_batch));
	if (ret)
		goto error_usb_batch;
	memcpy(usb_batch, &data->batch, sizeof(*usb_batch));

	memcpy(&data->prev, &data->batch, sizeof(data->prev));
	data->update = false;

	mutex_unlock(&data->mutex);

	for (i = 0; i < usb_batch->len; i++) {
		ret = usb_interrupt_msg(
			kdata->udev, usb_sndctrlpipe(kdata->udev, 1),
			usb_batch->cycles[i].msg,
			sizeof(usb_batch->cycles[i].msg), &sent, 1000);
		if (ret || sent != sizeof(usb_batch->cycles[i].msg)) {
			dev_err(dev, "failed to set LED cycle %u\n", i);
			return ret ? ret : 1;
		}
	}

	return 0;
error_usb_batch:
	mutex_unlock(&data->mutex);
	return ret;
}
