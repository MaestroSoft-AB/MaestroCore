#include <maestroutils/error.h>
#include <maestroutils/file_utils.h>

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
