#ifndef LEVIATHAN_X62_PERCENT_H_INCLUDED
#define LEVIATHAN_X62_PERCENT_H_INCLUDED

#include "dynamic.h"
#include "../common.h"

#include <linux/mutex.h>

#define PERCENT_MSG_SIZE ((size_t) 5)

struct percent_msg {
	u8 msg[PERCENT_MSG_SIZE];
};

enum percent_msg_which {
	PERCENT_MSG_WHICH_FAN  = 0x00,
	PERCENT_MSG_WHICH_PUMP = 0x40,
};

struct percent_data {
	u8 percent_min;
	u8 percent_max;

	struct percent_msg msg;
	u8 prev;
	bool update;

	struct mutex mutex;
};

void percent_data_init(struct percent_data *data, enum percent_msg_which which);
int percent_data_parse(struct percent_data *data, struct device *dev,
                       const char *attr, const char *buf);

int kraken_x62_update_percent(struct usb_kraken *kraken,
                              struct percent_data *data);

#endif  /* LEVIATHAN_X62_PERCENT_H_INCLUDED */
