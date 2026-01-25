#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h> 
#include <sys/stat.h> 
#include <errno.h> 
#include <string.h> 

bool directory_exists(const char* _path);

bool file_exists(const char* _path);

int create_directory_if_not_exists(const char* _path);

int write_string_to_file(const char* _str, const char* _filename);

/* Writes file to heap location */
char* read_file_to_string(const char* _filename);


#endif
