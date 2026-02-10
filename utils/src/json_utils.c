#include <maestroutils/json_utils.h>

double json_get_double(cJSON* _Root, const char* _Name)
{
  cJSON* field     = cJSON_GetObjectItemCaseSensitive(_Root, _Name);
  double _Fallback = 0.0f;
  if (!cJSON_IsNumber(field)) {
    return _Fallback;
  }
  return field->valuedouble;
}

const char* json_get_string(cJSON* _Root, const char* _Name)
{
  cJSON*      field     = cJSON_GetObjectItemCaseSensitive(_Root, _Name);
  const char* _Fallback = "Unknown";
  if (!cJSON_IsString(field)) {
    return _Fallback;
  }
  return field->valuestring;
}

int json_get_int(cJSON* _Root, const char* _Name)
{
  cJSON* field     = cJSON_GetObjectItemCaseSensitive(_Root, _Name);
  int    _Fallback = 0;
  if (!cJSON_IsNumber(field)) {
    return _Fallback;
  }
  return field->valueint;
}

void json_set_int(cJSON* _Json, const char* _key, int _val)
{
  if (_key == NULL)
    return;
  else
    cJSON_AddNumberToObject(_Json, _key, _val);
}
void json_set_double(cJSON* _Json, const char* _key, double _val)
{
  if (_key == NULL)
    return;
  else
    cJSON_AddNumberToObject(_Json, _key, _val);
}
void json_set_string(cJSON* _Json, const char* _key, const char* _val)
{
  if (_key == NULL || _val == NULL)
    return;
  else if (_val == NULL)
    cJSON_AddStringToObject(_Json, _key, "Unknown");
  else
    cJSON_AddStringToObject(_Json, _key, _val);
}
