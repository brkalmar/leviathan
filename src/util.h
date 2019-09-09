#ifndef LEVIATHAN_UTIL_H_INCLUDED
#define LEVIATHAN_UTIL_H_INCLUDED

#include <linux/kernel.h>

#define WORD_LEN_MAX 64

int kraken_scan_word(const char **buf, char *word);

int kraken_parse_percent(const char **buf, u32 *value);

#endif  /* LEVIATHAN_UTIL_H_INCLUDED */
