#ifndef LEVIATHAN_UTIL_H_INCLUDED
#define LEVIATHAN_UTIL_H_INCLUDED

#include <linux/kernel.h>

struct kraken_parse_enum {
	const char *word;
	const u64 value;
};

int kraken_parse_enum(const char **buf, const struct kraken_parse_enum *words,
                      void *value);

struct kraken_color {
	u8 red;
	u8 green;
	u8 blue;
};

int kraken_parse_color(const char **buf, struct kraken_color *value);

int kraken_parse_bool(const char **buf, bool *value);
int kraken_parse_percent(const char **buf, u32 *value);

#endif  /* LEVIATHAN_UTIL_H_INCLUDED */
