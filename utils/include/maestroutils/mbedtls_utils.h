#ifndef __MBEDTLS_UTILS_H__
#define __MBEDTLS_UTILS_H__
#include <mbedtls/net_sockets.h>
#include "mbedtls/ssl.h"


typedef struct
{
  mbedtls_net_context* ctx;
  mbedtls_ssl_context* ssl;
  mbedtls_ssl_config*  cfg;

} mbedtls_global;

int mbedtls_global_init();

#endif
