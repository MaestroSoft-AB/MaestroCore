/* ******************************************************************* */
/* *************************** HTTP CLIENT *************************** */
/* ******************************************************************* */
#ifndef HTTPClient_h
#define HTTPClient_h

#include <maestromodules/scheduler.h>
#include <maestromodules/tcp_client.h>
#include <maestroutils/error.h>
#include <maestroutils/http_parser.h>

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  HTTP_CLIENT_INITIALIZING,
  HTTP_CLIENT_CONNECTING,
  HTTP_CLIENT_BUILDING_REQUEST,
  HTTP_CLIENT_SENDING_REQUEST,
  HTTP_CLIENT_READING_FIRSTLINE,
  HTTP_CLIENT_READING_HEADERS,
  HTTP_CLIENT_DECIPHER_CHONKINESS,
  HTTP_CLIENT_READING_BODY_CHUNKED,
  HTTP_CLIENT_READING_BODY,
  HTTP_CLIENT_RETURNING,
  HTTP_CLIENT_DISPOSING,
  HTTP_CLIENT_ERROR,

} HTTPClientState;

typedef void (*http_client_on_success)(void* _context, char** _response);

typedef struct
{
  char scheme[6]; // http/https
  char host[128];
  char port[6];
  char path[512];
} URL_Parts;

typedef struct
{
  URL_Parts url_parts;

  uint64_t next_retry_at;

  size_t bytes_sent;
  size_t decoded_body_len;

  Scheduler_Task* task;
  TCP_Client tcp_client;
  const char* URL;
  HTTP_Request* req;
  HTTP_Response* resp;
  http_client_on_success on_success;
  void* context;
  char** response_out;
  uint8_t* request_buffer;
  uint8_t* response_buffer;
  uint8_t* decoded_body; // Might be too small

  int request_length;
  int bytes_received;
  int retries;
  int content_length;
  int chunked;
  int chunk_remaining;

  HTTPClientState state;

  bool tls;

} HTTP_Client;

int http_client_initiate(HTTP_Client* _Client, const char* _URL, HTTPMethod _method,
                         http_client_on_success _on_success, void* _context, char** _response_out);
void http_client_dispose(HTTP_Client* _Client);

#endif // HTTPClient_h
