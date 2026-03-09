#ifndef __MAESTROUTILS_CONFIG_HANDLER_H__
#define __MAESTROUTILS_CONFIG_HANDLER_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/*
 * Modular API (recommended): load the file once, query keys many times.
 *
 * Return codes:
 *  0  success
 * -1  file open/read error
 * -2  invalid arguments
 * -3  out of memory
 */
typedef struct config_handler config_handler_t;

int  config_handler_load(config_handler_t** out_cfg, const char* config_path);
void config_handler_free(config_handler_t* cfg);

/* Returns NULL if key is missing (or args invalid). */
const char* config_handler_get(const config_handler_t* cfg, const char* key);

/* Returns default_value when key is missing OR value is empty. */
const char* config_handler_get_default(const config_handler_t* cfg,
                                      const char* key,
                                      const char* default_value);

/*
 * Typed getters with defaults.
 *
 * Return values:
 *  0  found and parsed
 *  1  default used (missing or empty)
 * -2  invalid arguments
 * -3  invalid value format
 */
int config_handler_get_int_default(const config_handler_t* cfg,
                                  const char* key,
                                  int default_value,
                                  int* out_value);

int config_handler_get_double_default(const config_handler_t* cfg,
                                     const char* key,
                                     double default_value,
                                     double* out_value);

int config_handler_get_bool_default(const config_handler_t* cfg,
                                   const char* key,
                                   int default_value,
                                   int* out_value);

#ifdef __cplusplus
}
#endif

#endif
