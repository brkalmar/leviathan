/* Utility functions.
 */

#include "util.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

int kraken_parse_enum(const char **buf, const struct kraken_parse_enum *words,
                      void *value)
{
	u64 *value_u64;
	int read;
	int ret = sscanf(*buf, "%*s%n", &read);
	if (ret != 0)
		return -EINVAL;
	while (words->word != NULL) {
		if (strncasecmp(*buf, words->word, strlen(words->word)) == 0)
			break;
		words++;
	}
	if (words->word == NULL)
		return -EINVAL;
	*buf += read;
	value_u64 = value;
	*value_u64 = words->value;
	return 0;
}

int kraken_parse_color(const char **buf, struct kraken_color *value)
{
	int read;
	int ret = sscanf(*buf, "%02hhx%02hhx%02hhx%n",
	                 &value->red, &value->green, &value->blue, &read);
	if (ret != 3)
		return -EINVAL;
	*buf += read;
	return 0;
}

int kraken_parse_bool(const char **buf, bool *value)
{
	int read;
	int ret = sscanf(*buf, "%*s%n", &read);
	if (ret != 0)
		return -EINVAL;
	ret = kstrtobool(*buf, value);
	if (ret)
		return ret;
	*buf += read;
	return 0;
}

int kraken_parse_percent(const char **buf, u32 *value)
{
	int read;
	int ret = sscanf(*buf, "%u%n", value, &read);
	if (ret != 1)
		return -EINVAL;
	*buf += read;
	return 0;
}
