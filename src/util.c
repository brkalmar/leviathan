/* Utility functions.
 */

#include "util.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/stringify.h>

int kraken_scan_word(const char **buf, char *word)
{
	size_t i;
	for (i = 0; i < WORD_LEN_MAX - 1; i++) {
		if (**buf == '\0')
			break;
		if (**buf == ' ' || **buf == '\n') {
			++(*buf);
			break;
		}
		word[i] = **buf;
		++(*buf);
	}
	word[i] = '\0';
	return i == 0;
}

int kraken_parse_percent(const char **buf, u32 *value)
{
	size_t read;
	int ret = sscanf(*buf, "%u%zn", value, &read);
	if (ret != 1)
		return -EINVAL;
	*buf += read;
	return 0;
}
