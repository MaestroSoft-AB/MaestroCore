#include <maestromodules/tls_client.h>
#include <maestromodules/tls_global_ca.h>
#include <maestroutils/error.h>

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>


/**********************************BIO********************************************************/

static int tls_bio_send(void* ctx, const unsigned char* buf, size_t len)
{
  TLS_Client* c = (TLS_Client*)ctx;

  int res = c->bio.send(c->bio.io_ctx, buf, len);

  if (res < 0) {
    // Underlying layer should already set errno.
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  return res;
}

static int tls_bio_recv(void* ctx, unsigned char* buf, size_t len)
{
  TLS_Client* c = (TLS_Client*)ctx;

  int res = c->bio.recv(c->bio.io_ctx, buf, len);

  if (res < 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  if (res == 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  return res;
}


/********************************************************************************************/


int tls_client_init(TLS_Client* _tls, const char* _host, const TLS_BIO* _bio)
{
  if (!_tls || !_bio || !_bio->send || !_bio->recv) {
    return ERR_INVALID_ARG;
  }

  memset(_tls, 0, sizeof(TLS_Client));
  _tls->bio = *_bio;

  mbedtls_ssl_init(&_tls->ssl);
  mbedtls_ssl_config_init(&_tls->conf);
  mbedtls_entropy_init(&_tls->entropy);
  mbedtls_ctr_drbg_init(&_tls->ctr_drbg);

  const char* pers = "maestro_tls_client";

  int res = mbedtls_ctr_drbg_seed(&_tls->ctr_drbg, mbedtls_entropy_func, &_tls->entropy,
                                  (const unsigned char*)pers, strlen(pers));

  if (res != 0) {
    printf("mbedtls_ctr_drbg_seed failed, error: %d\n", res);
    return ERR_IO;
  }

  res = mbedtls_ssl_config_defaults(&_tls->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);

  if (res != 0) {
    printf("mbedtls_ssl_config_defaults failed, error: %d\n", res);
    return ERR_IO;
  }

  mbedtls_ssl_conf_authmode(&_tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_rng(&_tls->conf, mbedtls_ctr_drbg_random, &_tls->ctr_drbg);

  /*SET GLOBAL CA*/
  mbedtls_x509_crt* ca = global_tls_ca_get();
  if (!ca) {
    printf("Failed to set global CA\n");
    return ERR_IO;
  }

  mbedtls_ssl_conf_ca_chain(&_tls->conf, ca, NULL);

  res = mbedtls_ssl_setup(&_tls->ssl, &_tls->conf);
  if (res != 0) {
    printf("mbedtls_ssl_setup failed, error: %d\n", res);
    return ERR_IO;
  }

  res = mbedtls_ssl_set_hostname(&_tls->ssl, _host);
  if (res != 0) {
    printf("mbedtls_ssl_set_hostname failed, error: %d\n", res);
    return ERR_IO;
  }

  /*SET BIO*/

  mbedtls_ssl_set_bio(&_tls->ssl, _tls, tls_bio_send, tls_bio_recv, NULL);

  _tls->handshake_done = 0;

  return SUCCESS;
}
int  tls_client_handshake_step(TLS_Client* c); // 0=done, ERR_IN_PROGRESS=needs more, <0=fatal
int  tls_client_read(TLS_Client* c, uint8_t* buf, size_t len); // <0 sets errno like TCP
int  tls_client_write(TLS_Client* c, const uint8_t* buf, size_t len);
void tls_client_dispose(TLS_Client* c);
