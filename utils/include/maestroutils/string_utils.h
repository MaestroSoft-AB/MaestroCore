#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

char* stringcat(const char* _a, const char* _b);
#ifndef strdup
char* strdup(const char* _str);
#endif

int parse_string_to_double(const char* _str, double* _double);
int parse_string_to_int(const char* _str, int* _int);

bool char_is_number(const char _c);

#endif
