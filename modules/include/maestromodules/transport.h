#include <stdbool.h>

#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct
{

  const char* host;
  const char* port;
  const char* scheme;


  void* client; // TLS or TCP
  int   timeout_ms;
  bool  use_tls;
  bool  use_blocking;

} Transport;

/*
 *Initialize transport layer
 * Returns:
 *   SUCCESS
 *   ERR_IO
 *   ERR_NOMEM
 *   ERR_INVALID_ARG
 *   error codes
 */

int transport_init(Transport* t, const char* host, const char* port, const char* scheme,
                   int timeout_ms, bool use_blocking);
/*
 *Create connection.
 *Returns:
 *  SUCCESS
 *  ERR_IN_PROGRESS
 *  error codes
 */
int transport_finish_connect(Transport* _Transport);
/*
 * Non-blocking read.
 * Returns:
 *   >0  bytes read
 *   0   connection closed
 *   -1  error (errno may be EAGAIN)
 */
int transport_read(Transport* _Transport, uint8_t* buf, size_t len);

/*
 * Non-blocking write.
 * Returns:
 *   >0  bytes written
 *   0   nothing written
 *   -1  error (errno may be EAGAIN)
 */
int transport_write(Transport* _Transport, const uint8_t* buf, size_t len);

/*
 * Close and cleanup.
 */
void transport_dispose(Transport* _Transport); // Allocated by http_client, needs to be disposed
                                               // when http_client disposes

#endif
