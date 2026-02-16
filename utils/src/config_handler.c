#include <maestroutils/config_handler.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void trim(char* s)
{
  char* p = s;
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  memmove(s, p, strlen(p) + 1);

  size_t len = strlen(s);
  while (len && isspace((unsigned char)s[len - 1])) {
    s[--len] = 0;
  }
}

static void strip_inline_comment(char* s)
{
  if (!s) {
    return;
  }

  /* Treat a '#' as a comment delimiter only when it's at the start
     or preceded by whitespace, so values like "abc#def" remain intact. */
  for (size_t i = 0; s[i] != 0; i++) {
    if (s[i] == '#' && (i == 0 || isspace((unsigned char)s[i - 1]))) {
      s[i] = 0;
      break;
    }
  }
}

int config_get_value(const char* config_path,
                     const char* keys[],
                     char* values[],
                     size_t max_value_len,
                     size_t key_count)
{
  if (!config_path || !keys || !values || max_value_len == 0 || key_count == 0) {
    return -2;
  }

  FILE* f = fopen(config_path, "r");
  if (!f) {
    return -1;
  }

  int found[key_count];
  memset(found, 0, sizeof(found));

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    trim(line);

    if (line[0] == '#' || line[0] == 0) {
      continue;
    }

    char* eq = strchr(line, '=');
    if (!eq) {
      continue;
    }

    *eq = 0;
    char* key = line;
    char* value = eq + 1;

    trim(key);
    trim(value);
    strip_inline_comment(value);
    trim(value);

    for (size_t i = 0; i < key_count; i++) {
      if (!found[i] && keys[i] && strcmp(keys[i], key) == 0) {
        strncpy(values[i], value, max_value_len - 1);
        values[i][max_value_len - 1] = 0;
        found[i] = 1;
      }
    }
  }

  fclose(f);

  for (size_t i = 0; i < key_count; i++) {
    if (!found[i]) {
      return -2;
    }
  }

  return 0;
}
