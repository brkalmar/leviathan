#ifndef LEVIATHAN_X62_STATUS_H_INCLUDED
#define LEVIATHAN_X62_STATUS_H_INCLUDED

#include "../common.h"

#include <linux/mutex.h>

#define STATUS_DATA_MSG_SIZE ((size_t) 17)

struct status_data {
	u8 msg[STATUS_DATA_MSG_SIZE];
	struct mutex mutex;
};

void status_data_init(struct status_data *data);

u8 status_data_temp_liquid(struct status_data *data);
u16 status_data_fan_rpm(struct status_data *data);
u16 status_data_pump_rpm(struct status_data *data);
u8 status_data_unknown_1(struct status_data *data);
u32 status_data_unknown_2(struct status_data *data);
u16 status_data_unknown_3(struct status_data *data);

int kraken_x62_update_status(struct usb_kraken *kraken,
                             struct status_data *data);

#endif  /* LEVIATHAN_X62_STATUS_H_INCLUDED */
