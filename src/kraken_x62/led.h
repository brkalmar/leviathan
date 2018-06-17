#ifndef LEVIATHAN_X62_LED_H_INCLUDED
#define LEVIATHAN_X62_LED_H_INCLUDED

#include "../common.h"

#include <linux/device.h>
#include <linux/mutex.h>

#define LED_MSG_SIZE        ((size_t) 32)

struct led_msg {
	u8 msg[LED_MSG_SIZE];
};

enum led_which {
	LED_WHICH_SYNC = 0b000,
	LED_WHICH_LOGO = 0b001,
	LED_WHICH_RING = 0b010,
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

	struct mutex mutex;
};

void led_data_init(struct led_data *data, enum led_which which);
int led_data_parse(struct led_data *data, struct device *dev, const char *attr,
                   const char *buf);

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data);

#endif  /* LEVIATHAN_X62_LED_H_INCLUDED */
