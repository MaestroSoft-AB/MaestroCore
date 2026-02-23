#ifndef __MAESTROUTILS_CONFIG_HANDLER_H__
#define __MAESTROUTILS_CONFIG_HANDLER_H__

#include <stddef.h>

/*
 * Reads multiple key=value pairs from a config file.
 *
 * Returns:
 *  0  on success (all requested keys found)
 * -1  file open/read error
 * -2  one or more keys missing
 */
int config_get_value(const char* config_path,
                     const char* keys[],
                     char* values[],
                     size_t max_value_len,
                     size_t key_count);

#endif
