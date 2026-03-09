#include <maestroutils/config_handler.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct config_entry {
  char* key;
  char* value;
  struct config_entry* next;
} config_entry;

struct config_handler {
  config_entry* head;
};

static char* config_strdup(const char* s)
{
  if (!s) {
    return NULL;
  }

  size_t len = strlen(s);
  char* out = (char*)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, s, len);
  out[len] = 0;
  return out;
}

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

static config_entry* config_find_entry(const config_handler_t* cfg, const char* key)
{
  if (!cfg || !key) {
    return NULL;
  }

  config_entry* it = cfg->head;
  while (it) {
    if (it->key && strcmp(it->key, key) == 0) {
      return it;
    }
    it = it->next;
  }

  return NULL;
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

int config_handler_load(config_handler_t** out_cfg, const char* config_path)
{
  if (!out_cfg || !config_path) {
    return -2;
  }

  *out_cfg = NULL;

  FILE* f = fopen(config_path, "r");
  if (!f) {
    return -1;
  }

  config_handler_t* cfg = (config_handler_t*)calloc(1, sizeof(config_handler_t));
  if (!cfg) {
    fclose(f);
    return -3;
  }

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

    if (key[0] == 0) {
      continue;
    }

    /* First occurrence wins */
    if (config_find_entry(cfg, key)) {
      continue;
    }

    config_entry* e = (config_entry*)calloc(1, sizeof(config_entry));
    if (!e) {
      fclose(f);
      config_handler_free(cfg);
      return -3;
    }

    e->key = config_strdup(key);
    e->value = config_strdup(value);
    if (!e->key || !e->value) {
      free(e->key);
      free(e->value);
      free(e);
      fclose(f);
      config_handler_free(cfg);
      return -3;
    }

    e->next = cfg->head;
    cfg->head = e;
  }

  fclose(f);
  *out_cfg = cfg;
  return 0;
}

void config_handler_free(config_handler_t* cfg)
{
  if (!cfg) {
    return;
  }

  config_entry* it = cfg->head;
  while (it) {
    config_entry* next = it->next;
    free(it->key);
    free(it->value);
    free(it);
    it = next;
  }

  free(cfg);
}

const char* config_handler_get(const config_handler_t* cfg, const char* key)
{
  config_entry* e = config_find_entry(cfg, key);
  return e ? e->value : NULL;
}

const char* config_handler_get_default(const config_handler_t* cfg,
                                      const char* key,
                                      const char* default_value)
{
  const char* v = config_handler_get(cfg, key);
  if (!v || v[0] == 0) {
    return default_value;
  }
  return v;
}

static int parse_int_strict(const char* s, int* out_value)
{
  if (!s || !out_value) {
    return -1;
  }

  errno = 0;
  char* end = NULL;
  long val = strtol(s, &end, 10);
  if (errno != 0 || end == s || *end != 0) {
    return -1;
  }
  if (val < INT_MIN || val > INT_MAX) {
    return -1;
  }

  *out_value = (int)val;
  return 0;
}

static int parse_double_strict(const char* s, double* out_value)
{
  if (!s || !out_value) {
    return -1;
  }

  errno = 0;
  char* end = NULL;
  double val = strtod(s, &end);
  if (errno != 0 || end == s || *end != 0) {
    return -1;
  }

  *out_value = val;
  return 0;
}

int config_handler_get_int_default(const config_handler_t* cfg,
                                  const char* key,
                                  int default_value,
                                  int* out_value)
{
  if (!cfg || !key || !out_value) {
    return -2;
  }

  const char* v = config_handler_get(cfg, key);
  if (!v || v[0] == 0) {
    *out_value = default_value;
    return 1;
  }

  if (parse_int_strict(v, out_value) != 0) {
    return -3;
  }

  return 0;
}

int config_handler_get_double_default(const config_handler_t* cfg,
                                     const char* key,
                                     double default_value,
                                     double* out_value)
{
  if (!cfg || !key || !out_value) {
    return -2;
  }

  const char* v = config_handler_get(cfg, key);
  if (!v || v[0] == 0) {
    *out_value = default_value;
    return 1;
  }

  if (parse_double_strict(v, out_value) != 0) {
    return -3;
  }

  return 0;
}

static int streq(const char* a, const char* b)
{
  return a && b && strcmp(a, b) == 0;
}

int config_handler_get_bool_default(const config_handler_t* cfg,
                                   const char* key,
                                   int default_value,
                                   int* out_value)
{
  if (!cfg || !key || !out_value) {
    return -2;
  }

  const char* v = config_handler_get(cfg, key);
  if (!v || v[0] == 0) {
    *out_value = default_value ? 1 : 0;
    return 1;
  }

  if (streq(v, "true") || streq(v, "1")) {
    *out_value = 1;
    return 0;
  }
  if (streq(v, "false") || streq(v, "0")) {
    *out_value = 0;
    return 0;
  }

  return -3;
}
