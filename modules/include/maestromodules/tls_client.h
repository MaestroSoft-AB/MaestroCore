#ifndef __TLS_CLIENT_H__
#define __TLS_CLIENT_H__

/* ******************************************************************* */
/* *************************** TLS CLIENT **************************** */
/* ******************************************************************* */

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <maestromodules/tcp_client.h>

// Jag har använt samma states som i tcp_client.h, men vi kan självklart ändra om det inte makes
// sense

/* typedef enum
{
  TLS_STATE_NONE = 0,
  TLS_STATE_HANDSHAKING,
  TLS_STATE_ESTABLISHED,
  TLS_STATE_CLOSED,
  TLS_STATE_ERROR

} TLSClientState; */


typedef enum
{
  TLS_CLIENT_STATE_INIT,
  TLS_CLIENT_STATE_CONNECTING,
  TLS_CLIENT_STATE_READING,
  TLS_CLIENT_STATE_WRITING,
  TLS_CLIENT_STATE_DISPOSING,
  TLS_CLIENT_STATE_ERROR

} TLSClientState;

typedef struct
{
  // Active TLS session (holds handshake state, negotiated keys, record layer)
  mbedtls_ssl_context ssl;

  // Per-connection TLS config (mode, verification rules, RNG, CA chain)
  mbedtls_ssl_config conf;

  //  OS/hardware entropy provider used to seed this specific connection's DRBG
  mbedtls_entropy_context entropy;

  //  Cryptographically secure deterministic RNG used by TLS (key material, nonces)
  mbedtls_ctr_drbg_context ctr_drbg;

  TCP_Client*    tcp;  //  TCP socket used by TLS
  const char*    host; //  Target hostname (used for SNI and certificate hostname verification)
  TLSClientState state;

} TLS_Client;

#endif
