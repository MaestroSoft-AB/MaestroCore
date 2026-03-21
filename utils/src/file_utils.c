#include <maestroutils/file_utils.h>
#include <maestroutils/error.h>
#include <maestroutils/string_utils.h>

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/* Returns true or false whether directory exists */
bool directory_exists(const char* _path)
{
  struct stat buffer;
  if (stat(_path, &buffer) == 0) {
    return S_ISDIR(buffer.st_mode);
  }
  return false;
}

/* Returns true or false whether file exists */
bool file_exists(const char* _path)
{
  struct stat buffer;
  if (stat(_path, &buffer) == 0) {
    return S_ISREG(buffer.st_mode);
  }
  return false;
}

/* Tries to create directory if it doesn't already exist */
int create_directory_if_not_exists(const char* _path)
{
  if (directory_exists(_path)) {
    return 0;
  }
#if defined _WIN32
  bool success = CreateDirectory(_Path, NULL);
  if (success == false) {
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS)
      return 1;
    else
      return -1;
  }
#else
  if (mkdir(_path, 0755) == -1) {
    if (errno != EEXIST) {
      printf("Failed to create directory '%s': %s\n", _path, strerror(errno));
      return -1;
    }
  }
#endif
  return 0;
}

/* Writes string to given file */
int write_string_to_file(const char* _str, const char* _filename)
{
  if (_str == NULL || _filename == NULL) {
    return ERR_INVALID_ARG;
  }

  FILE* f = fopen(_filename, "w");
  if (f == NULL) {
    printf("Error: Unable to open the file.\n");
    return -1;
  }

  fputs(_str, f);
  fclose(f);

  return 0;
}

/** Heap allocated */
char* read_file_to_string(const char* _filename)
{
  FILE* file = fopen(_filename, "r");
  if (!file) {
    fprintf(stderr, "File load error: %s\n", _filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);       /* Seek end of file */
  size_t file_size = ftell(file); /* Define filesize based on fseek position */
  rewind(file);                   /* Rewind to beginning of file */

  char* string = malloc(file_size + 1); /* Plus one for \0 */

  if (string == NULL) {
    perror("malloc");
    fclose(file);
    return NULL;
  }

  /* Reads file into buffer and returns the amount of bytes read */
  size_t read_size = fread(string, 1, file_size, file);
  if (read_size != file_size) {
    perror("fread");
    free(string);
    fclose(file);
    return NULL;
  }

  string[file_size] = '\0';

  fclose(file); /* Done with file, close it */

  return string;
}

/* Counts the amount of files with a given file extension */
int count_files_with_extension(const char* _dir, const char* _ext)
{
  DIR*           dir;
  struct dirent* entry;
  int            count   = 0;
  size_t         ext_len = strlen(_ext);

  if (!_dir || !_ext) {
    return -1;
  }

  dir = opendir(_dir);
  if (!dir) {
    return -1; // failed to open directory
  }

  while ((entry = readdir(dir)) != NULL) {
    const char* name     = entry->d_name;
    size_t      name_len = strlen(name);

    // skip "." and ".."
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    // check if this is a regular file when d_type is available
#ifdef DT_REG
    if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
      continue;
#endif

    if (name_len >= ext_len) {
      if (strcmp(name + (name_len - ext_len), _ext) == 0) {
        count++;
      }
    }
  }

  closedir(dir);
  return count;
}

/* Helper for checking if file has a specific extension*/
int has_extension(const char* _name, const char* _ext)
{
  size_t len_name = strlen(_name);
  size_t len_ext  = strlen(_ext);

  if (len_name < len_ext + 1)
    return 0; // need at least ".x"
  if (_name[len_name - len_ext - 1] != '.')
    return 0;

  return strcmp(_name + (len_name - len_ext), _ext) == 0;
}

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
char** list_filenames_with_ext(const char* _dirpath, const char* _ext, size_t* _count_out)
{
  DIR*           dir = NULL;
  struct dirent* entry;
  char**         result   = NULL;
  size_t         capacity = 0;
  size_t         count    = 0;

  if (!_dirpath || !_ext || !_count_out)
    return NULL;
  *_count_out = 0;

  dir = opendir(_dirpath);
  if (!dir) {
    perror("opendir");
    return NULL;
  }

  while ((entry = readdir(dir)) != NULL) {
    const char* name = entry->d_name;

    /* Skip "." and ".." */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    if (!has_extension(name, _ext))
      continue;

    /* Grow array if needed */
    if (count == capacity) {
      size_t new_cap = capacity == 0 ? 8 : capacity * 2;
      char** tmp     = realloc(result, new_cap * sizeof(*tmp));
      if (!tmp) {
        perror("realloc");
        /* Clean up and return failure */
        for (size_t i = 0; i < count; i++)
          free(result[i]);
        free(result);
        closedir(dir);
        return NULL;
      }
      result   = tmp;
      capacity = new_cap;
    }

    result[count] = strdup(name);
    if (!result[count]) {
      perror("strdup");
      for (size_t i = 0; i < count; i++)
        free(result[i]);
      free(result);
      closedir(dir);
      return NULL;
    }
    count++;
  }

  closedir(dir);

  /* Optionally NULL-terminate the array */
  char** tmp = realloc(result, (count + 1) * sizeof(*tmp));
  if (!tmp) {
    /* If this fails, still return without NULL terminator */
    *_count_out = count;
    return result;
  }
  result        = tmp;
  result[count] = NULL;

  *_count_out = count;
  return result;
}
