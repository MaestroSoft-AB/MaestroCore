#include <maestromodules/tls_ca_bundle.h>
#include <maestromodules/tls_global_ca.h>
#include <stdio.h>
#include <string.h>

static mbedtls_x509_crt g_ca;
static int              g_initialized = 0;

int global_tls_ca_init(void)
{
  if (g_initialized)
    return 0;

  mbedtls_x509_crt_init(&g_ca);

  int ret = mbedtls_x509_crt_parse(&g_ca, (const unsigned char*)g_ca_bundle_pem,
                                   strlen(g_ca_bundle_pem) + 1);

  printf("ret in global_tls_ca_init: %d\n", ret);


  if (ret != 0) {
    mbedtls_x509_crt_free(&g_ca);
    return ret;
  }

  g_initialized = 1;
  return 0;
}

mbedtls_x509_crt* global_tls_ca_get(void)
{
  if (!g_initialized)
    return NULL;

  return &g_ca;
}

void global_tls_ca_dispose(void)
{
  if (!g_initialized)
    return;

  mbedtls_x509_crt_free(&g_ca);
  g_initialized = 0;
}
