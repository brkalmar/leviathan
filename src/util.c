/* Utility functions.
 */

#include "util.h"

#include <linux/kernel.h>
#include <linux/stringify.h>

int str_scan_word(const char **buf, char *word)
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
