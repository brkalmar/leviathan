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

static enum percent_msg_which percent_msg_which_get(struct percent_msg *msg)
{
	const enum percent_msg_which which = msg->msg[2];
	return which;
}

static void percent_msg_set(struct percent_msg *msg, u8 percent)
{
	msg->msg[4] = percent;
}

static int percent_msg_update(struct percent_msg *msg,
                              struct usb_kraken *kraken)
{
	int sent;
	int ret = usb_interrupt_msg(
		kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
		msg->msg, sizeof(msg->msg), &sent, 1000);
	if (ret || sent != sizeof(msg->msg)) {
		dev_err(&kraken->udev->dev,
		        "failed to set speed percent: I/O error\n");
		return ret ? ret : 1;
	}
	return 0;
}

static const u8 PERCENTS_SILENT_FAN[] = {
	35,   35,  35,  35,  35,  35,  35,  35,  35,  35,
	35,   35,  35,  35,  35,  35,  35,  35,  35,  35,
	35,   35,  35,  35,  35,  35,  35,  35,  35,  35,
	35,   35,  35,  35,  35,  35,  35,  35,  35,  35,
	35,   37,  39,  41,  43,  45,  47,  49,  51,  53,
	55,   59,  63,  67,  71,  75,  80,  85,  90,  95,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100,
};

static const u8 PERCENTS_SILENT_PUMP[] = {
	60,   60,  60,  60,  60,  60,  60,  60,  60,  60,
	60,   60,  60,  60,  60,  60,  60,  60,  60,  60,
	60,   60,  60,  60,  60,  60,  60,  60,  60,  60,
	60,   60,  60,  60,  60,  60,  62,  64,  66,  68,
	70,   72,  74,  76,  78,  80,  82,  84,  86,  88,
	90,   92,  94,  96,  98, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100,
};

static const u8 PERCENTS_PERFORMANCE_FAN[] = {
	50,   50,  50,  50,  50,  50,  50,  50,  50,  50,
	50,   50,  50,  50,  50,  50,  50,  50,  50,  50,
	50,   50,  50,  50,  50,  50,  50,  50,  50,  50,
	50,   50,  50,  50,  50,  50,  52,  54,  56,  58,
	60,   62,  64,  66,  68,  70,  72,  74,  76,  78,
	80,   82,  84,  86,  88,  90,  92,  94,  96,  98,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100,
};

static const u8 PERCENTS_PERFORMANCE_PUMP[] = {
	70,   70,  70,  70,  70,  70,  70,  70,  70,  70,
	70,   70,  70,  70,  70,  70,  70,  70,  70,  70,
	70,   70,  70,  70,  70,  70,  70,  70,  70,  70,
	70,   70,  70,  70,  70,  70,  72,  74,  76,  78,
	80,   81,  82,  83,  84,  85,  86,  87,  88,  89,
	90,   91,  92,  93,  94,  95,  96,  97,  98,  99,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100,
};

static void percent_data_set(struct percent_data *data, const u8 *percents)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(data->msgs); i++)
		percent_msg_set(&data->msgs[i], percents[i]);
}

void percent_data_init(struct percent_data *data, enum percent_msg_which which)
{
	size_t i;
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

	data->update = true;
	data->value.get = dynamic_val_temp_liquid;
	for (i = 0; i < ARRAY_SIZE(data->msgs); i++)
		percent_msg_init(&data->msgs[i], which);

	switch (which) {
	case PERCENT_MSG_WHICH_FAN:
		percent_data_set(data, PERCENTS_SILENT_FAN);
		break;
	case PERCENT_MSG_WHICH_PUMP:
		percent_data_set(data, PERCENTS_SILENT_PUMP);
		break;
	}
	data->value_prev = -1;
	data->msg_prev = NULL;

	mutex_init(&data->mutex);
}

int kraken_x62_update_percent(struct usb_kraken *kraken,
                              struct percent_data *data)
{
	struct percent_msg *msg;
	s8 value;
	int ret = 0;
	mutex_lock(&data->mutex);
	if (!data->update)
		goto error;

	value = data->value.get(data->value.state, kraken->data);
	if (value < 0) {
		dev_err(&kraken->udev->dev,
		        "error getting value for dynamic percent update: %d\n",
		        value);
		ret = value;
		goto error;
	}
	if (value == data->value_prev)
		goto error;
	msg = &data->msgs[value];
	if (data->msg_prev != NULL &&
	    memcmp(msg, data->msg_prev, sizeof(*msg)) == 0)
		goto error;

	ret = percent_msg_update(msg, kraken);
	if (ret)
		goto error;
	data->value_prev = value;
	data->msg_prev = msg;

error:
	mutex_unlock(&data->mutex);
	return ret;
}

static int percent_parser_percent(struct percent_parser *parser, u8 *percent)
{
	char percent_str[WORD_LEN_MAX + 1];
	unsigned int percent_ui;
	int ret = str_scan_word(&parser->buf, percent_str);
	if (ret) {
		dev_warn(parser->dev, "%s: missing percent\n", parser->attr);
		return ret;
	}
	ret = kstrtouint(percent_str, 0, &percent_ui);
	if (ret) {
		dev_warn(parser->dev, "%s: invalid percent %s\n", parser->attr,
		         percent_str);
		return ret;
	}
	if (percent_ui < parser->data->percent_min) {
		*percent = parser->data->percent_min;
	} else if (percent_ui > parser->data->percent_max) {
		*percent = parser->data->percent_max;
	} else {
		*percent = percent_ui;
	}
	return 0;
}

static int percent_parser_silent(struct percent_parser *parser)
{
	switch (percent_msg_which_get(&parser->data->msgs[0])) {
	case PERCENT_MSG_WHICH_FAN:
		percent_data_set(parser->data, PERCENTS_SILENT_FAN);
		break;
	case PERCENT_MSG_WHICH_PUMP:
		percent_data_set(parser->data, PERCENTS_SILENT_PUMP);
		break;
	}
	return 0;
}

#define PERCENTS_FIXED_MAX_FAN  ((u8) 50)
#define PERCENTS_FIXED_MAX_PUMP ((u8) 50)

static int percent_parser_fixed(struct percent_parser *parser)
{
	size_t i;
	u8 max, percent;
	int ret = percent_parser_percent(parser, &percent);
	if (ret)
		return ret;

	switch (percent_msg_which_get(&parser->data->msgs[0])) {
	case PERCENT_MSG_WHICH_FAN:
		max = PERCENTS_FIXED_MAX_FAN;
		break;
	case PERCENT_MSG_WHICH_PUMP:
		max = PERCENTS_FIXED_MAX_PUMP;
		break;
	}

	for (i = 0; i < max; i++)
		percent_msg_set(&parser->data->msgs[i], percent);
	for (i = max; i < ARRAY_SIZE(parser->data->msgs); i++)
		percent_msg_set(&parser->data->msgs[i],
		                parser->data->percent_min);
	return 0;
}

static int percent_parser_performance(struct percent_parser *parser)
{
	switch (percent_msg_which_get(&parser->data->msgs[0])) {
	case PERCENT_MSG_WHICH_FAN:
		percent_data_set(parser->data, PERCENTS_PERFORMANCE_FAN);
		break;
	case PERCENT_MSG_WHICH_PUMP:
		percent_data_set(parser->data, PERCENTS_PERFORMANCE_PUMP);
		break;
	}
	return 0;
}

static int percent_parser_custom(struct percent_parser *parser)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(parser->data->msgs); i++) {
		u8 percent;
		int ret = percent_parser_percent(parser, &percent);
		if (ret)
			return ret;
		percent_msg_set(&parser->data->msgs[i], percent);
	}
	return 0;
}

int percent_parser_parse(struct percent_parser *parser)
{
	char type[WORD_LEN_MAX + 1];
	char *source = type;
	int ret;

	mutex_lock(&parser->data->mutex);

	ret = str_scan_word(&parser->buf, source);
	if (ret || strcasecmp(source, "temp_liquid") != 0) {
		// NOTE: currently only temp_liquid is implemented as a source
		dev_warn(parser->dev, "%s: missing dynamic value source\n",
		         parser->attr);
		if (!ret)
			ret = 1;
		goto error;
	}
	parser->data->value.get = dynamic_val_temp_liquid;

	ret = str_scan_word(&parser->buf, type);
	if (ret) {
		dev_warn(parser->dev, "%s: missing percent type\n",
		         parser->attr);
		goto error;
	}
	if (strcasecmp(type, "silent") == 0) {
		ret = percent_parser_silent(parser);
	} else if (strcasecmp(type, "fixed") == 0) {
		ret = percent_parser_fixed(parser);
	} else if (strcasecmp(type, "performance") == 0) {
		ret = percent_parser_performance(parser);
	} else if (strcasecmp(type, "custom") == 0) {
		ret = percent_parser_custom(parser);
	} else {
		dev_warn(parser->dev, "%s: invalid percent type %s\n",
		         parser->attr, type);
		ret = 1;
		goto error;
	}
	if (ret)
		goto error;
	ret = str_scan_word(&parser->buf, type);
	if (!ret) {
		dev_warn(parser->dev,
		         "%s: unrecognized data left in buffer: %s...\n",
		         parser->attr, type);
		ret = 1;
		goto error;
	}

	parser->data->value_prev = -1;
	parser->data->msg_prev = NULL;
	parser->data->update = true;
	mutex_unlock(&parser->data->mutex);
	return 0;

error:
	parser->data->update = false;
	mutex_unlock(&parser->data->mutex);
	return ret;
}
