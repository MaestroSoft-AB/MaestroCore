#include <maestromodules/transport.h>
#include <string.h>
#include <maestroutils/error.h>
#include <maestromodules/tcp_client.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>


int transport_init(Transport* t, const char* host, const char* port, const char* scheme,
                   int timeout_ms, bool use_blocking)
{
  printf("Before nullchecks in transport_init\n");

  if (t == NULL || host == NULL || port == NULL || scheme == NULL || host[0] == '\0' ||
      port[0] == '\0' || scheme[0] == '\0') {
    return ERR_INVALID_ARG;
  }

  printf("After nullchecks\n");

  int  res;
  char scheme_lower[6] = {0};
  t->host              = host;
  t->port              = port;
  t->scheme            = scheme;
  t->timeout_ms        = timeout_ms;
  t->use_blocking      = use_blocking;


  // Convert scheme to lower for comparison
  for (int i = 0; scheme[i] != '\0' && i < 5; i++) {
    scheme_lower[i] = (char)tolower((unsigned char)scheme[i]);
  }

  if (strcmp(scheme_lower, "https") == 0) {
    t->use_tls = true;
  } else if (strcmp(scheme_lower, "http") == 0) {
    t->use_tls = false;
  } else {
    return ERR_BAD_FORMAT;
  }

  printf("do we get here?\n");
  if (t->use_tls == true) {
    // cast tls_client
    // init tls_client
    // return result
  }

  if (t->use_tls == false) {

    if (t->use_blocking) {
      res = tcp_client_blocking_init(&t->tcp, t->host, t->port, t->timeout_ms);
    } else {

      printf("Attempting to init tcp from transport\n");
      res = tcp_client_init(&t->tcp, t->host, t->port);
      printf("Result after tcp init: %d\n", res);
    }
    return res;
  }

  // If we get here something failed
  return ERR_IO;
}


int transport_read(Transport* _Transport, uint8_t* buf, size_t len)
{
  if (_Transport == NULL) {
    return ERR_INVALID_ARG;
  }

  if (_Transport->use_tls == true) {
    // return mbedtls read
  }

  if (_Transport->use_tls == false) {
    return tcp_client_read_simple(&_Transport->tcp, buf, len);
  }

  // Something went wrong
  return ERR_IO;
}

int transport_write(Transport* _Transport, const uint8_t* buf, size_t len)
{
  if (_Transport == NULL) {
    return ERR_INVALID_ARG;
  }

  if (_Transport->use_tls == true) {
    // return tls read
  }

  if (_Transport->use_tls == false) {
    return tcp_client_write_simple(&_Transport->tcp, buf, len);
  }

  // If we are here something went wrong
  return ERR_IO;
}

int transport_finish_connect(Transport* _Transport)
{
  if (_Transport == NULL) {
    return ERR_INVALID_ARG;
  }

  if (_Transport->use_tls == true) {
    // return tls finish connect
  }

  if (!_Transport->use_tls) {
    if (_Transport->use_blocking) {
      return SUCCESS;
    }
    return tcp_client_finish_connect(_Transport->tcp.fd);
  }
  // If we are here something went wrong
  return ERR_IO;
}

void transport_dispose(Transport* _Transport)
{
  if (!_Transport) {
    return;
  }

  if (_Transport->use_tls == true) {
    // tlsclient + call dispose
  } else {
    tcp_client_dispose(&_Transport->tcp);
  }
}
