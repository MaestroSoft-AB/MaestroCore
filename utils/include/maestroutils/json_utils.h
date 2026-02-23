#pragma once

#include "../../external/cjson/cJSON.h"

int         json_get_int(cJSON* _Root, const char* _Name);
double      json_get_double(cJSON* _Root, const char* _Name);
const char* json_get_string(cJSON* _Root, const char* _Name);

void json_set_int(cJSON* _Json, const char* _key, int _val);
void json_set_double(cJSON* _Json, const char* _key, double _val);
void json_set_string(cJSON* _Json, const char* _key, const char* _val);
