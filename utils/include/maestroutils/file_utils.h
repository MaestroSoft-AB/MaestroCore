#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

#include <stdbool.h> 
#include <stdio.h>

bool directory_exists(const char* _path);

bool file_exists(const char* _path);

int create_directory_if_not_exists(const char* _path);

int write_string_to_file(const char* _str, const char* _filename);

/* Writes file to heap location */
char* read_file_to_string(const char* _filename);

int count_files_with_extension(const char* _dir, const char* _ext);

/* Helper for checking if file has a specific extension */
int has_extension(const char* _name, const char* _ext);

/*
 * Returns a NULL-terminated array of strdup'ed filenames (no path),
 * filtered by extension `ext` (e.g. "txt").
 *
 * On success:
 *   - *count_out = number of entries
 *   - return value = char** array, terminated by NULL
 *
 * On failure:
 *   - returns NULL, *count_out is set to 0
 *
 * Caller must free each element and the array itself.
 */
char** list_filenames_with_ext(const char* _dirpath, const char* _ext, size_t* _count_out);

#endif
