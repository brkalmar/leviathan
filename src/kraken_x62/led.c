/* Handling of LED attributes.
 */

#include "led.h"
#include "../util.h"

#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb.h>

static const u8 LED_MSG_HEADER[] = {
	0x02, 0x4c,
};

static void led_msg_init(struct led_msg *msg)
{
	memcpy(msg->msg, LED_MSG_HEADER, sizeof(LED_MSG_HEADER));
}

static void led_msg_which(struct led_msg *msg, enum led_which which)
{
	msg->msg[2] &= ~0b111;
	msg->msg[2] |= (u8) which;
}

static enum led_which led_msg_which_get(const struct led_msg *msg)
{
	const enum led_which which = msg->msg[2] & 0b111;
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

static int led_preset_from_str(enum led_preset *preset, const char *str)
{
	if (strcasecmp(str, "fixed") == 0)
		*preset = LED_PRESET_FIXED;
	else if (strcasecmp(str, "fading") == 0)
		*preset = LED_PRESET_FADING;
	else if (strcasecmp(str, "spectrum_wave") == 0)
		*preset = LED_PRESET_SPECTRUM_WAVE;
	else if (strcasecmp(str, "marquee") == 0)
		*preset = LED_PRESET_MARQUEE;
	else if (strcasecmp(str, "covering_marquee") == 0)
		*preset = LED_PRESET_COVERING_MARQUEE;
	else if (strcasecmp(str, "alternating") == 0)
		*preset = LED_PRESET_ALTERNATING;
	else if (strcasecmp(str, "breathing") == 0)
		*preset = LED_PRESET_BREATHING;
	else if (strcasecmp(str, "pulse") == 0)
		*preset = LED_PRESET_PULSE;
	else if (strcasecmp(str, "tai_chi") == 0)
		*preset = LED_PRESET_TAI_CHI;
	else if (strcasecmp(str, "water_cooler") == 0)
		*preset = LED_PRESET_WATER_COOLER;
	else if (strcasecmp(str, "load") == 0)
		*preset = LED_PRESET_LOAD;
	else
		return 1;
	return 0;
}

static void led_msg_preset(struct led_msg *msg, enum led_preset preset)
{
	msg->msg[3] = (u8) preset;
}

static enum led_preset led_msg_preset_get(const struct led_msg *msg)
{
	const enum led_preset preset = msg->msg[3];
	return preset;
}

static bool led_msg_preset_is_legal(const struct led_msg *msg,
                                    enum led_preset preset)
{
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

static int led_moving_from_str(bool *moving, const char *str)
{
	int ret;
	if (strcasecmp(str, "*") == 0) {
		*moving = LED_MOVING_DEFAULT;
		return 0;
	}
	ret = kstrtobool(str, moving);
	return ret;
}

static void led_msg_moving(struct led_msg *msg, bool moving)
{
	msg->msg[2] &= ~(0b1 << 3);
	msg->msg[2] |= ((u8) moving) << 3;
}

static bool led_msg_moving_is_legal(const struct led_msg *msg, bool moving) {
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

static int led_direction_from_str(enum led_direction *direction,
                                  const char *str)
{
	if (strcasecmp(str, "*") == 0) {
		*direction = LED_DIRECTION_DEFAULT;
		return 0;
	}
	if (strcasecmp(str, "forward") == 0)
		*direction = LED_DIRECTION_CLOCKWISE;
	else if (strcasecmp(str, "backward") == 0)
		*direction = LED_DIRECTION_COUNTERCLOCKWISE;
	else
		return 1;
	return 0;
}

static void led_msg_direction(struct led_msg *msg, enum led_direction direction)
{
	msg->msg[2] &= ~(0b1111 << 4);
	msg->msg[2] |= ((u8) direction) << 4;
}

static bool led_msg_direction_is_legal(const struct led_msg *msg,
                                       enum led_direction direction)
{
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

#define LED_INTERVAL_DEFAULT   LED_INTERVAL_NORMAL

static int led_interval_from_str(enum led_interval *interval, const char *str)
{
	if (strcasecmp(str, "*") == 0) {
		*interval = LED_INTERVAL_DEFAULT;
		return 0;
	}
	if (strcasecmp(str, "slowest") == 0)
		*interval = LED_INTERVAL_SLOWEST;
	else if (strcasecmp(str, "slower") == 0)
		*interval = LED_INTERVAL_SLOWER;
	else if (strcasecmp(str, "normal") == 0)
		*interval = LED_INTERVAL_NORMAL;
	else if (strcasecmp(str, "faster") == 0)
		*interval = LED_INTERVAL_FASTER;
	else if (strcasecmp(str, "fastest") == 0)
		*interval = LED_INTERVAL_FASTEST;
	else
		return 1;
	return 0;
}

static void led_msg_interval(struct led_msg *msg, enum led_interval interval)
{
	msg->msg[4] &= ~0b111;
	msg->msg[4] |= (u8) interval;
}

static bool led_msg_interval_is_legal(const struct led_msg *msg,
                                      enum led_interval interval)
{
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

static int led_group_size_from_str(u8 *group_size, const char *str)
{
	int ret;
	unsigned int n;
	if (strcasecmp(str, "*") == 0) {
		*group_size = LED_GROUP_SIZE_DEFAULT;
		return 0;
	}
	ret = kstrtouint(str, 0, &n);
	if (ret || n < LED_GROUP_SIZE_MIN || n > LED_GROUP_SIZE_MAX)
		return ret ? ret : 1;
	*group_size = n;
	return 0;
}

static void led_msg_group_size(struct led_msg *msg, u8 group_size)
{
	group_size = (group_size - LED_GROUP_SIZE_MIN) & 0b11;
	msg->msg[4] &= ~(0b11 << 3);
	msg->msg[4] |= group_size << 3;
}

static bool led_msg_group_size_is_legal(const struct led_msg *msg,
                                        u8 group_size)
{
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

struct led_color {
	u8 red;
	u8 green;
	u8 blue;
};

static int led_color_from_str(struct led_color *color, const char *str)
{
	char hex[7];
	unsigned long rgb;
	int ret;
	switch (strlen(str)) {
	case 6:
		// RRGGBB
		memcpy(hex, str, 6);
		break;
	default:
		return 1;
	}
	hex[6] = '\0';

	ret = kstrtoul(hex, 16, &rgb);
	if (ret)
		return ret;
	color->red   = (rgb >> 16) & 0xff;
	color->green = (rgb >>  8) & 0xff;
	color->blue  = (rgb >>  0) & 0xff;
	return 0;
}

static void led_msg_color_logo(struct led_msg *msg,
                               const struct led_color *color)
{
	// NOTE: the logo color is in GRB format
	msg->msg[5] = color->green;
	msg->msg[6] = color->red;
	msg->msg[7] = color->blue;
}

#define LED_MSG_COLORS_RING ((size_t) 8)

static void led_msg_colors_ring(struct led_msg *msg,
                                const struct led_color *colors)
{
	size_t i;
	for (i = 0; i < LED_MSG_COLORS_RING; i++) {
		u8 *start = msg->msg + 8 + i * 3;
		start[0] = colors[i].red;
		start[1] = colors[i].green;
		start[2] = colors[i].blue;
	}
}


static void led_batch_init(struct led_batch *batch, enum led_which which)
{
	u8 i;
	for (i = 0; i < ARRAY_SIZE(batch->cycles); i++) {
		struct led_msg *msg = &batch->cycles[i];
		led_msg_init(msg);
		led_msg_which(msg, which);
		led_msg_cycle(msg, i);
	}
}

static int led_batch_update(struct led_batch *batch, struct usb_kraken *kraken)
{
	int ret, sent;
	u8 i;
	for (i = 0; i < batch->len; i++) {
		ret = usb_interrupt_msg(
			kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
			batch->cycles[i].msg, sizeof(batch->cycles[i].msg),
			&sent, 1000);
		if (ret || sent != sizeof(batch->cycles[i].msg)) {
			dev_err(&kraken->udev->dev,
			        "failed to set LED cycle %u\n", i);
			return ret ? ret : 1;
		}
	}
	return 0;
}

void led_data_init(struct led_data *data, enum led_which which)
{
	led_batch_init(&data->batch, which);
	// this will never be confused for a real batch
	data->prev.len = 0;
	data->update = false;

	mutex_init(&data->mutex);
}

static int parse_preset_check_len(
	enum led_preset preset, const struct led_batch *batch,
	struct device *dev, const char *attr)
{
	bool ok = false;
	switch (preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_WATER_COOLER:
	case LED_PRESET_LOAD:
		ok = batch->len == 1;
		break;
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_TAI_CHI:
		ok = batch->len == 2;
		break;
	case LED_PRESET_FADING:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		ok = batch->len >= 1 && batch->len <= LED_BATCH_CYCLES_SIZE;
		break;
	}
	if (!ok)
		dev_warn(dev, "%s: invalid nr of cycles %u for given preset\n",
		         attr, batch->len);
	return !ok;
}

static int parse_preset(struct led_batch *batch, struct device *dev,
                        const char *attr, const char **buf)
{
	char preset_str[WORD_LEN_MAX + 1];
	enum led_preset preset;
	u8 i;
	int ret = str_scan_word(buf, preset_str);
	if (ret) {
		dev_warn(dev, "%s: missing preset\n", attr);
		return ret;
	}
	ret = led_preset_from_str(&preset, preset_str);
	if (ret) {
		dev_warn(dev, "%s: invalid preset %s\n", attr, preset_str);
		return ret;
	}
	if (!led_msg_preset_is_legal(&batch->cycles[0], preset)) {
		dev_warn(dev, "%s: illegal preset %s for LED(s)\n", attr,
		         preset_str);
		return 1;
	}
	ret = parse_preset_check_len(preset, batch, dev, attr);
	if (ret)
		return ret;
	for (i = 0; i < batch->len; i++)
		led_msg_preset(&batch->cycles[i], preset);
	return 0;
}

static int parse_moving(struct led_batch *batch, struct device *dev,
                        const char *attr, const char **buf)
{
	char moving_str[WORD_LEN_MAX + 1];
	bool moving;
	u8 i;
	int ret = str_scan_word(buf, moving_str);
	if (ret) {
		dev_warn(dev, "%s: missing moving\n", attr);
		return ret;
	}
	ret = led_moving_from_str(&moving, moving_str);
	if (ret) {
		dev_warn(dev, "%s: invalid moving %s\n", attr, moving_str);
		return ret;
	}
	if (!led_msg_moving_is_legal(&batch->cycles[0], moving)) {
		dev_warn(dev, "%s: illegal moving %d for the given preset\n",
		         attr, moving);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_moving(&batch->cycles[i], moving);
	return 0;
}

static int parse_direction(struct led_batch *batch, struct device *dev,
                           const char *attr, const char **buf)
{
	char direction_str[WORD_LEN_MAX + 1];
	enum led_direction direction;
	u8 i;
	int ret = str_scan_word(buf, direction_str);
	if (ret) {
		dev_warn(dev, "%s: missing direction\n", attr);
		return ret;
	}
	ret = led_direction_from_str(&direction, direction_str);
	if (ret) {
		dev_warn(dev, "%s: invalid direction %s\n", attr,
		         direction_str);
		return ret;
	}
	if (!led_msg_direction_is_legal(&batch->cycles[0], direction)) {
		dev_warn(dev, "%s: illegal direction %s for the given preset\n",
		         attr, direction_str);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_direction(&batch->cycles[i], direction);
	return 0;
}

static int parse_interval(struct led_batch *batch, struct device *dev,
                          const char *attr, const char **buf)
{
	char interval_str[WORD_LEN_MAX + 1];
	enum led_interval interval;
	u8 i;
	int ret = str_scan_word(buf, interval_str);
	if (ret) {
		dev_warn(dev, "%s: missing interval\n", attr);
		return ret;
	}
	ret = led_interval_from_str(&interval, interval_str);
	if (ret) {
		dev_warn(dev, "%s: invalid interval %s\n", attr, interval_str);
		return ret;
	}
	if (!led_msg_interval_is_legal(&batch->cycles[0], interval)) {
		dev_warn(dev, "%s: illegal interval %s for the given preset\n",
		         attr, interval_str);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_interval(&batch->cycles[i], interval);
	return 0;
}

static int parse_group_size(struct led_batch *batch, struct device *dev,
                            const char *attr, const char **buf)
{
	char group_size_str[WORD_LEN_MAX + 1];
	u8 group_size;
	u8 i;
	int ret = str_scan_word(buf, group_size_str);
	if (ret) {
		dev_warn(dev, "%s: missing group size\n", attr);
		return ret;
	}
	ret = led_group_size_from_str(&group_size, group_size_str);
	if (ret) {
		dev_warn(dev, "%s: invalid group size %s\n", attr,
		         group_size_str);
		return ret;
	}
	if (!led_msg_group_size_is_legal(&batch->cycles[0], group_size)) {
		dev_warn(dev,
		         "%s: illegal group size %u for the given preset\n",
		         attr, group_size);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_group_size(&batch->cycles[i], group_size);
	return 0;
}

static int parse_color_logo(struct led_msg *msg, struct device *dev,
                            const char *attr, const char **buf)
{
	char color_str[WORD_LEN_MAX + 1];
	struct led_color color;
	int ret = str_scan_word(buf, color_str);
	if (ret) {
		dev_warn(dev, "%s: missing color\n", attr);
		return ret;
	}
	ret = led_color_from_str(&color, color_str);
	if (ret) {
		dev_warn(dev, "%s: invalid color %s\n", attr, color_str);
		return ret;
	}
	led_msg_color_logo(msg, &color);
	return 0;
}

static int parse_colors_ring(struct led_msg *msg, struct device *dev,
                             const char *attr, const char **buf)
{
	char color_str[WORD_LEN_MAX + 1];
	struct led_color colors[LED_MSG_COLORS_RING];
	size_t i;
	int ret;
	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		ret = str_scan_word(buf, color_str);
		if (ret) {
			dev_warn(dev, (i == 0) ? "%s: missing colors\n" :
			         "%s: invalid colors\n", attr);
			return ret;
		}
		ret = led_color_from_str(&colors[i], color_str);
		if (ret) {
			dev_warn(dev, "%s: invalid colors ... %s\n", attr,
			         color_str);
			return ret;
		}
	}
	led_msg_colors_ring(msg, colors);
	return 0;
}

static int parse_colors(struct led_msg *msg, struct device *dev,
                        const char *attr, const char **buf)
{
	int ret = 0;
	switch (led_msg_which_get(msg)) {
	case LED_WHICH_LOGO:
		ret = parse_color_logo(msg, dev, attr, buf);
		break;
	case LED_WHICH_RING:
		ret = parse_colors_ring(msg, dev, attr, buf);
		break;
	case LED_WHICH_SYNC:
		ret = parse_color_logo(msg, dev, attr, buf);
		if (ret)
			return ret;
		ret = parse_colors_ring(msg, dev, attr, buf);
		break;
	}
	return ret;
}

static int parse_batch_off(struct led_batch *batch)
{
	struct led_color colors[LED_MSG_COLORS_RING];

	struct led_msg *msg = &batch->cycles[0];
	led_msg_preset(msg, LED_PRESET_FIXED);
	led_msg_moving(msg, LED_MOVING_DEFAULT);
	led_msg_direction(msg, LED_DIRECTION_DEFAULT);
	led_msg_interval(msg, LED_INTERVAL_DEFAULT);
	led_msg_group_size(msg, LED_GROUP_SIZE_DEFAULT);

	// off = all-black
	memset(colors, 0x00, sizeof(colors));
	led_msg_color_logo(msg, &colors[0]);
	led_msg_colors_ring(msg, colors);

	batch->len = 1;
	return 0;
}

static int parse_batch(struct led_batch *batch, struct device *dev,
                       const char *attr, const char **buf)
{
	char len_str[WORD_LEN_MAX + 1];
	unsigned int len;
	size_t i;

	int ret = str_scan_word(buf, len_str);
	if (ret) {
		dev_warn(dev, "%s: missing nr of cycles\n", attr);
		return ret;
	}
	if (strcasecmp(len_str, "off") == 0) {
		ret = parse_batch_off(batch);
		return ret;
	}
	ret = kstrtouint(len_str, 0, &len);
	if (ret || len < 1 || len > LED_BATCH_CYCLES_SIZE) {
		dev_warn(dev, "%s: invalid nr of cycles %s\n", attr, len_str);
		return ret ? ret : 1;
	}
	batch->len = len;

	if ((ret = parse_preset(batch, dev, attr, buf)) ||
	    (ret = parse_moving(batch, dev, attr, buf)) ||
	    (ret = parse_direction(batch, dev, attr, buf)) ||
	    (ret = parse_interval(batch, dev, attr, buf)) ||
	    (ret = parse_group_size(batch, dev, attr, buf)))
		return ret;

	for (i = 0; i < batch->len; i++) {
		ret = parse_colors(&batch->cycles[i], dev, attr, buf);
		if (ret)
			return ret;
	}
	return 0;
}

int led_data_parse(struct led_data *data, struct device *dev, const char *attr,
                   const char *buf)
{
	char rest[WORD_LEN_MAX + 1];
	int ret;

	mutex_lock(&data->mutex);

	ret = parse_batch(&data->batch, dev, attr, &buf);
	if (ret)
		goto error;
	ret = str_scan_word(&buf, rest);
	if (!ret) {
		dev_warn(dev, "%s: unrecognized data left in buffer: %s...\n",
		         attr, rest);
		ret = 1;
		goto error;
	}

	data->update = true;
	mutex_unlock(&data->mutex);
	return 0;

error:
	data->update = false;
	mutex_unlock(&data->mutex);
	return ret;
}

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data)
{
	int ret = 0;

	mutex_lock(&data->mutex);

	if (!data->update)
		goto error;
	// if same message as previously, no update necessary
	if (memcmp(&data->batch, &data->prev, sizeof(data->batch)) == 0)
		goto error;
	ret = led_batch_update(&data->batch, kraken);
	if (ret)
		goto error;
	memcpy(&data->prev, &data->batch, sizeof(data->prev));
	data->update = false;

error:
	mutex_unlock(&data->mutex);
	return ret;
}
