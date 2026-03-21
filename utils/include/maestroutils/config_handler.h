#ifndef __MAESTROUTILS_CONFIG_HANDLER_H__
#define __MAESTROUTILS_CONFIG_HANDLER_H__

#include <stddef.h>

typedef struct
{
  const char* keys;
  char*       vals;

} Conf_Key_Value;

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

/* Edit: 
 * - Returns amount of values parsed
 * - Returns null pointer on not found values
 * - Heap allocates, no fixed buffer sizes */
int config_values_get(const char* _config_path,
                     const char** _keys,
                     char** _values,
                     size_t _key_count);

void config_values_dispose(char** _values, size_t _values_count);

#endif
