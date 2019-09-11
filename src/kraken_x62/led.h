#ifndef LEVIATHAN_X62_LED_H_INCLUDED
#define LEVIATHAN_X62_LED_H_INCLUDED

#include "../common.h"

#include <linux/device.h>
#include <linux/mutex.h>

#define LED_MSG_SIZE        ((size_t) 32)

struct led_msg {
	u8 msg[LED_MSG_SIZE];
};

#define LED_BATCH_CYCLES_SIZE ((size_t) 8)

/**
 * A batch of 1 or more update messages -- one message per cycle.
 */
struct led_batch {
	struct led_msg cycles[LED_BATCH_CYCLES_SIZE];
	// first len messages in `cycles` are to be sent when updating
	u8 len;
};

struct led_data {
	struct led_batch batch;
	struct led_batch prev;
	bool update;
	// number of logo color cycles set
	u8 colors_logo;
	// number of ring color cycles set
	u8 colors_ring;

	struct mutex mutex;
};

int led_data_parse_cycles(const char **buf, struct led_data *data);
int led_data_parse_preset(const char **buf, struct led_data *data);
int led_data_parse_moving(const char **buf, struct led_data *data);
int led_data_parse_direction(const char **buf, struct led_data *data);
int led_data_parse_interval(const char **buf, struct led_data *data);
int led_data_parse_group_size(const char **buf, struct led_data *data);
int led_data_parse_colors_logo(const char **buf, struct led_data *data);
int led_data_parse_colors_ring(const char **buf, struct led_data *data);
int led_data_parse_which(const char **buf, struct led_data *data,
                         struct device *dev);

void led_data_init(struct led_data *data);

int led_data_update(struct kraken_data *kdata, struct led_data *data);

#endif  /* LEVIATHAN_X62_LED_H_INCLUDED */
