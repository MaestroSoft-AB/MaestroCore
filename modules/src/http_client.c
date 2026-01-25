#include "error.h"
#include <maestromodules/http_client.h>
#include <maestromodules/tcp_client.h>
#include <stddef.h>

void http_client_taskwork(void* _context, uint64_t _montime);
HTTPClientState http_client_worktask_connecting(HTTP_Client* _Client);
HTTPClientState http_client_worktask_build_request(HTTP_Client* _Client);
HTTPClientState http_client_worktask_send_request(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_firstline(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_headers(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_body(HTTP_Client* _Client);
HTTPClientState http_client_worktask_returning(HTTP_Client* _Client);
HTTPClientState http_client_worktask_decipher_chonkiness(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_body_chunked(HTTP_Client* _Client);

int http_client_initiate(HTTP_Client* _Client, const char* _URL, HTTPMethod _method,
                         http_client_on_success _on_success, void* _context, char** _response_out)
{
  (void)_method; // To be used with mbedtls
  if (!_Client) {
    return ERR_INVALID_ARG;
  }
  HTTP_Request* req = calloc(1, sizeof(HTTP_Request));
  if (!req) {
    return ERR_NO_MEMORY;
  }

  HTTP_Response* resp = calloc(1, sizeof(HTTP_Response));
  if (!resp) {
    return ERR_NO_MEMORY;
  }

  _Client->task = scheduler_create_task(_Client, http_client_taskwork);
  if (!_Client->task) {
    free(req);
    return ERR_BUSY;
  }
  // printf("URL in init: %s\n", _URL);

  char* url_copy = strdup(_URL);
  if (!url_copy) {
    scheduler_destroy_task(_Client->task);
    _Client->task = NULL;
    free(req);
    free(resp);
    return ERR_NO_MEMORY;
  }

  _Client->on_success = _on_success;
  _Client->resp = resp;
  _Client->req = req;
  _Client->URL = url_copy;
  _Client->state = HTTP_CLIENT_CONNECTING;
  _Client->request_length = 0;
  _Client->bytes_sent = 0;
  _Client->retries = 0;
  _Client->next_retry_at = SystemMonotonicMS();
  URL_Parts url_parts = {0};
  _Client->url_parts = url_parts;
  _Client->response_out = _response_out;
  _Client->context = _context;

  // Check url for http/https
  _Client->tls = false;
  _Client->chunked = -1;
  _Client->decoded_body = NULL;
  _Client->decoded_body_len = 0;

  return 0;
}

HTTPClientState http_client_worktask_connecting(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  http_parser_url(_Client->URL, (void*)&_Client->url_parts);

  // printf("Scheme: %s\n", _Client->url_parts.scheme);
  // printf("Host: %s\n", _Client->url_parts.host);
  // printf("path: %s\n", _Client->url_parts.path);
  // printf("PorT: %s\n", _Client->url_parts.port);

  if (_Client->tls == false) {
    const char* PORT = "80";
    // printf("URL: %s\n", _Client->url_parts.host);
    // printf("Pprt: %s\n", PORT);
    int result = tcp_client_init(&_Client->tcp_client, _Client->url_parts.host, PORT);

    if (result != SUCCESS) {
      printf("TCP_Client init failed\n");
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_BUILDING_REQUEST;
  } else {
    // init tls client etc
    return HTTP_CLIENT_BUILDING_REQUEST;
  }

  return HTTP_CLIENT_BUILDING_REQUEST;
}

HTTPClientState http_client_worktask_build_request(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  const char* method_str = "GET";

  size_t base_len = 92;
  size_t max_len = strlen(_Client->URL) + base_len;
  _Client->request_buffer = malloc(max_len);
  if (!_Client->request_buffer) {
    return HTTP_CLIENT_ERROR;
  }

  _Client->request_length = snprintf((char*)_Client->request_buffer, max_len,
                                     "%s %s HTTP/1.1\r\n"
                                     "Host: %s\r\n"
                                     "User-Agent: httpclient\r\n"
                                     "Connection: close\r\n\r\n",
                                     method_str, _Client->url_parts.path, _Client->url_parts.host);

  if (_Client->request_length < 0 || (size_t)_Client->request_length >= max_len) {
    free(_Client->request_buffer);
    _Client->request_buffer = NULL;
    return HTTP_CLIENT_ERROR;
  }

  return HTTP_CLIENT_SENDING_REQUEST;
}

HTTPClientState http_client_worktask_send_request(HTTP_Client* _Client)
{

  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }
  if (_Client->tls == false) {
    size_t remaining = 0;
    remaining = _Client->request_length - _Client->bytes_sent;

    int written = tcp_client_write_simple(&_Client->tcp_client,
                                          _Client->request_buffer + _Client->bytes_sent, remaining);

    if (written > 0) {
      _Client->bytes_sent += written;
      if (_Client->bytes_sent >= (size_t)_Client->request_length) {

        _Client->retries = 0;
        return HTTP_CLIENT_READING_FIRSTLINE;
      }

      _Client->next_retry_at = SystemMonotonicMS() + 100;
      return HTTP_CLIENT_SENDING_REQUEST;

    } else if (written == 0) {
      _Client->retries++;
      _Client->next_retry_at = SystemMonotonicMS() + 1000;

      return HTTP_CLIENT_SENDING_REQUEST;

    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        _Client->next_retry_at = SystemMonotonicMS() + 100;
        return HTTP_CLIENT_SENDING_REQUEST;
      }
      if (_Client->retries < 3) {
        _Client->retries++;
        _Client->next_retry_at = SystemMonotonicMS() + 1000;
        return HTTP_CLIENT_SENDING_REQUEST;
      }
      return HTTP_CLIENT_ERROR;
    }
  } else {
    // implement tls read here
    return HTTP_CLIENT_READING_FIRSTLINE;
  }

  _Client->retries = 0;
  return HTTP_CLIENT_READING_FIRSTLINE;
}

HTTPClientState http_client_worktask_read_firstline(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  TCP_Client* TCP_C = &_Client->tcp_client;

  uint8_t tcp_buf[8096];
  int bytes_read = tcp_client_read_simple(TCP_C, tcp_buf, 8095);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_FIRSTLINE;
    }
    perror("recv firstline");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0) {
    printf("Connection closed by peer\n");
    return HTTP_CLIENT_ERROR;
  }
  ssize_t bytes_stored = tcp_client_realloc_data(&TCP_C->data, tcp_buf, (size_t)bytes_read);

  if (bytes_stored < 0) {
    return HTTP_CLIENT_ERROR;
  }

  if (TCP_C->data.size == 0) {
    return HTTP_CLIENT_READING_FIRSTLINE;
  }

  int line_end = http_parser_find_line_end(TCP_C->data.addr, TCP_C->data.size);
  if (line_end < 0) {
    /*No \r\n found yet*/
    if (TCP_C->data.size >= 1024) {
      /*Invalid request*/
      printf("Response too large..\n");
      return HTTP_CLIENT_ERROR;
    }
    /*Keep looking for line end on next work call*/
    return HTTP_CLIENT_READING_FIRSTLINE;
  }

  /*line_end is the index of the first \r*/
  size_t line_len = (size_t)line_end;

  if (line_len == 0 || line_len >= 1024) {
    printf("Response too large..\n");
    return HTTP_CLIENT_ERROR;
  }

  if (http_parser_response_firstline((const char*)TCP_C->data.addr, line_len, _Client->resp) !=
      SUCCESS) {
    /*Add internal error*/
    return HTTP_CLIENT_ERROR;
  }

  // printf("Version: %s\n", _Client->resp->version);
  // printf("status_code_string: %s\n", _Client->resp->status_code_string);
  // printf("reason_phrase: %s\n", _Client->resp->reason_phrase);
  //
  /*We have handled first line + 2 for \r\n*/
  size_t parsed = line_len + 2;

  /*If there is data remaining after first line shift it to beggining of
   * buffer*/
  if (TCP_C->data.size > parsed) {

    memmove(TCP_C->data.addr, TCP_C->data.addr + parsed, TCP_C->data.size - parsed);
  }

  /*Remove first line by shrinking the buffer*/
  TCP_C->data.size -= parsed;

  return HTTP_CLIENT_READING_HEADERS;
}

HTTPClientState http_client_worktask_read_headers(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  TCP_Client* TCP_C = &_Client->tcp_client;

  uint8_t tcp_buf[1024];
  int bytes_read = tcp_client_read_simple(TCP_C, tcp_buf, 1024);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_HEADERS;
    }
    perror("recv headers");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0 && TCP_C->data.size == 0) {
    printf("Connection closed while reading headers\r\n");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read > 0) {

    ssize_t bytes_stored = tcp_client_realloc_data(&TCP_C->data, tcp_buf, (size_t)bytes_read);

    if (bytes_stored < 0) {
      return HTTP_CLIENT_ERROR;
    }
  }

  if (TCP_C->data.size == 0) {
    /*No data, try again on next work call*/
    return HTTP_CLIENT_READING_HEADERS;
  }

  /*Edgecase no headers*/
  if (TCP_C->data.size >= 2 && TCP_C->data.addr[0] == '\r' && TCP_C->data.addr[1] == '\n') {
    if (TCP_C->data.size > 2) {
      memmove(TCP_C->data.addr, TCP_C->data.addr + 2, TCP_C->data.size - 2);
    }
    TCP_C->data.size -= 2;

    /*Create empty header so we can dispose without crash*/
    _Client->req->headers = linked_list_create();
    if (!_Client->req->headers) {
      /*add internal error*/
      return HTTP_CLIENT_ERROR;
    }

    _Client->retries = 0;
    return HTTP_CLIENT_ERROR;
  }

  int headers_end = http_parser_find_headers_end(TCP_C->data.addr, TCP_C->data.size);

  if (headers_end < 0) {
    /*Continue reading on next work call*/
    return HTTP_CLIENT_READING_HEADERS;
  }

  /*headers_end is the index of the first \r parser will ignore last line*/
  size_t header_len = (size_t)headers_end + 4;

  /*We have still parsed the full line including \r\n\r\n*/
  size_t parsed = (size_t)headers_end + 4;

  if (http_parser_headers((const char*)TCP_C->data.addr, header_len, &_Client->req->headers) !=
      SUCCESS) {
    return HTTP_CLIENT_ERROR;
  }

  /*If there is data remaining its the body move it to start of buffer*/
  if (TCP_C->data.size > parsed) {
    memmove(TCP_C->data.addr, TCP_C->data.addr + parsed, TCP_C->data.size - parsed);
  }

  TCP_C->data.size -= parsed;
  _Client->retries = 0;

  /*Check if there is a content-length (body) to read*/
  const char* content_length_string = NULL;
  int result =
      http_parser_get_header_value(_Client->req->headers, "Content-Length", &content_length_string);
  const char* transfer_encoding_string = NULL;
  int res = http_parser_get_header_value(_Client->req->headers, "Transfer-Encoding",
                                         &transfer_encoding_string);

  if (res == SUCCESS) {
    return HTTP_CLIENT_DECIPHER_CHONKINESS;
  }

  if (result < 0) {
    printf("Content-Length header not found\n");
    return HTTP_CLIENT_RETURNING;
  }

  int content_length = 0;
  parse_string_to_int(content_length_string, &content_length);
  _Client->content_length = content_length;

  if (content_length > 0) {
    /*There is a body to read*/
    return HTTP_CLIENT_READING_BODY;
  }

  return HTTP_CLIENT_RETURNING;
}

HTTPClientState http_client_worktask_decipher_chonkiness(HTTP_Client* _Client)
{
  if (_Client == NULL) {
    return HTTP_CLIENT_ERROR;
  }

  TCP_Client* TCP_C = &_Client->tcp_client;

  int line_end = http_parser_find_line_end(TCP_C->data.addr, TCP_C->data.size);

  if (line_end < 0) {

    // Read until we find end of the line
    uint8_t buf[1024];
    int additional_bytes_read = tcp_client_read_simple(TCP_C, buf, sizeof(buf));

    if (additional_bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        // Read again
        return HTTP_CLIENT_DECIPHER_CHONKINESS;
      }
      perror("recv chunk");
      return HTTP_CLIENT_ERROR;
    }

    if (additional_bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    if (tcp_client_realloc_data(&TCP_C->data, buf, (size_t)additional_bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_DECIPHER_CHONKINESS;
  }

  size_t consume_len = (size_t)line_end + 2;
  size_t parse_len = (size_t)line_end;

  int chunk_size;

  for (size_t i = 0; i < (size_t)line_end; i++) {
    uint8_t ch = TCP_C->data.addr[i];
    if (ch == ';' || ch == ' ' || ch == '\t') {
      parse_len = i;
      break;
    }
  }

  if (parse_len <= 0) {
    return HTTP_CLIENT_ERROR;
  }

  char line[48];
  memcpy(line, TCP_C->data.addr, parse_len);
  line[parse_len] = '\0';

  char* endptr = NULL;
  unsigned long long val = strtoull(line, &endptr, 16);
  if (endptr == line) {
    return HTTP_CLIENT_ERROR;
  }

  chunk_size = (size_t)val;

  if (TCP_C->data.size > consume_len) {
    memmove(TCP_C->data.addr, TCP_C->data.addr + consume_len, TCP_C->data.size - consume_len);
  }

  TCP_C->data.size -= consume_len;

  // printf("%.*s\n", (int)TCP_C->data.size, (char *)TCP_C->data.addr);

  // Find trailing \r\n\r\n
  if (chunk_size == 0) {
    // No trailers just singe \r\n
    if (TCP_C->data.size >= 2 && TCP_C->data.addr[0] == '\r' && TCP_C->data.addr[1] == '\n') {

      if (TCP_C->data.size > 2) {

        memmove(TCP_C->data.addr, TCP_C->data.addr + 2, TCP_C->data.size - 2);
      }

      TCP_C->data.size -= 2;
      return HTTP_CLIENT_RETURNING;
    }

    // There are trailers
    int traling_end = http_parser_find_headers_end(TCP_C->data.addr, TCP_C->data.size);
    if (traling_end >= 0) {
      size_t trailers_consume = (size_t)traling_end + 4;
      if (TCP_C->data.size > trailers_consume) {
        memmove(TCP_C->data.addr, TCP_C->data.addr + trailers_consume,
                TCP_C->data.size - trailers_consume);
      }
      TCP_C->data.size -= trailers_consume;
      return HTTP_CLIENT_RETURNING;
    }

    // Incomplete trailers
    // Read again
    uint8_t buf[1024];

    int bytes_read = tcp_client_read_simple(TCP_C, buf, sizeof(buf));

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return HTTP_CLIENT_DECIPHER_CHONKINESS;
      }
      perror("recv trailers");
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    if (tcp_client_realloc_data(&TCP_C->data, buf, (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_DECIPHER_CHONKINESS;
  }

  _Client->chunk_remaining = (int)chunk_size;
  return HTTP_CLIENT_READING_BODY_CHUNKED;
}

HTTPClientState http_client_worktask_read_body_chunked(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  TCP_Client* TCP_C = &_Client->tcp_client;

  // Do we need more data?
  if (TCP_C->data.size < (size_t)_Client->chunk_remaining) {
    uint8_t buf[1024];
    int bytes_read = tcp_client_read_simple(TCP_C, buf, sizeof(buf));

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        // Keep reading
        return HTTP_CLIENT_READING_BODY_CHUNKED;
      }
      perror("recv chunk-data");
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    // If we have read more data
    if (tcp_client_realloc_data(&TCP_C->data, buf, (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_READING_BODY_CHUNKED;
  }

  size_t old_len = _Client->decoded_body_len;
  size_t add_len = (size_t)_Client->chunk_remaining;
  size_t new_len = old_len + add_len;

  uint8_t* newbuf = realloc(_Client->decoded_body, new_len + 1);
  if (newbuf == NULL) {
    return HTTP_CLIENT_ERROR;
  }
  _Client->decoded_body = newbuf;

  memcpy(_Client->decoded_body + old_len, TCP_C->data.addr, add_len);
  _Client->decoded_body_len = new_len;
  _Client->decoded_body[new_len] = '\0';

  // Move read data out of tcpbuffer
  if (TCP_C->data.size > (size_t)_Client->chunk_remaining) {
    memmove(TCP_C->data.addr, TCP_C->data.addr + _Client->chunk_remaining,
            TCP_C->data.size - _Client->chunk_remaining);
  }

  TCP_C->data.size -= _Client->chunk_remaining;
  _Client->chunk_remaining = 0;

  if (TCP_C->data.size < 2) {
    // NO CRLF (End of line) found
    uint8_t buf[1024];
    int bytes_read = tcp_client_read_simple(TCP_C, buf, sizeof(buf));

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return HTTP_CLIENT_READING_BODY_CHUNKED;
      }
      perror("chunck recv");
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    if (tcp_client_realloc_data(&TCP_C->data, buf, (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_READING_BODY_CHUNKED;
  }

  if (TCP_C->data.addr[0] != '\r' || TCP_C->data.addr[1] != '\n') {
    return HTTP_CLIENT_ERROR;
  }

  // Consume the CRLF (end of line)
  memmove(TCP_C->data.addr, TCP_C->data.addr + 2, TCP_C->data.size - 2);
  TCP_C->data.size -= 2;

  return HTTP_CLIENT_DECIPHER_CHONKINESS;
}

HTTPClientState http_client_worktask_read_body(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  unsigned int TCP_BUF_SIZE = 1024;
  TCP_Client* TCP_C = &_Client->tcp_client;

  uint8_t tcp_buf[TCP_BUF_SIZE];

  int bytes_read = tcp_client_read_simple(TCP_C, tcp_buf, TCP_BUF_SIZE);

  // printf("Buf: %s\n", tcp_buf);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_BODY;
    }
    perror("recv body");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0 && TCP_C->data.size < (size_t)_Client->content_length) {
    printf("Connection closed before full body was recieved\r\n");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read > 0) {
    _Client->chunked -= bytes_read;
    printf("Chunked read: %d\n\n", _Client->chunked);
    ssize_t bytes_stored = tcp_client_realloc_data(&TCP_C->data, tcp_buf, (size_t)bytes_read);

    if (bytes_stored < 0) {
      return HTTP_CLIENT_ERROR;
    }
  }

  if (TCP_C->data.size < (size_t)_Client->content_length) {
    /*Keep reading body on next work call*/
    return HTTP_CLIENT_READING_BODY;
  }

  _Client->retries = 0;
  return HTTP_CLIENT_RETURNING;
}

HTTPClientState http_client_worktask_returning(HTTP_Client* _Client)
{

  uint8_t* src = NULL;
  size_t src_len = 0;

  if (_Client->decoded_body != NULL && _Client->decoded_body_len > 0) {
    src = _Client->decoded_body;
    src_len = _Client->decoded_body_len;
  } else {
    src = _Client->tcp_client.data.addr;
    src_len = _Client->tcp_client.data.size;
  }

  char* response_out = malloc(src_len + 1);
  if (response_out == NULL) {
    perror("malloc");
    return HTTP_CLIENT_ERROR;
  }

  memcpy(response_out, src, src_len);
  response_out[src_len] = '\0';
  _Client->on_success(_Client->context, &response_out);

  return HTTP_CLIENT_DISPOSING;
}

void http_client_taskwork(void* _context, uint64_t _montime)
{
  if (!_context) {
    return;
  }
  (void)_montime;
  HTTP_Client* client = (HTTP_Client*)_context;

  uint64_t now = SystemMonotonicMS();

  switch (client->state) {

  case HTTP_CLIENT_INITIALIZING: {
    printf("HTTP_CLIENT_INITIALIZING\n");
    break;
  }
  case HTTP_CLIENT_CONNECTING: {
    printf("HTTP_CLIENT_CONNECTING\n");
    client->state = http_client_worktask_connecting(client);
    break;
  }
  case HTTP_CLIENT_BUILDING_REQUEST: {
    printf("HTTP_CLIENT_BUILDING_REQUEST\n");
    client->state = http_client_worktask_build_request(client);
    break;
  }
  case HTTP_CLIENT_SENDING_REQUEST: {
    printf("HTTP_CLIENT_SENDING_REQUEST\n");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_send_request(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_READING_FIRSTLINE: {
    printf("HTTP_CLIENT_READING_FIRSTLINE\n");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_firstline(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_READING_HEADERS: {
    printf("HTTP_CLIENT_READING_HEADERS\n");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_headers(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_DECIPHER_CHONKINESS: {
    printf("HTTP_CLIENT_DECIPHER_CHONKINESS\n");
    client->state = http_client_worktask_decipher_chonkiness(client);
    break;
  }
  case HTTP_CLIENT_READING_BODY_CHUNKED: {
    printf("HTTP_CLIENT_READING_BODY_CHUNKED\n");
    client->state = http_client_worktask_read_body_chunked(client);
    break;
  }

  case HTTP_CLIENT_READING_BODY: {
    printf("HTTP_CLIENT_READING_BODY\n");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_body(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_RETURNING: {
    printf("HTTP_CLIENT_RETURNING\n");
    client->state = http_client_worktask_returning(client);
    break;
  }
  case HTTP_CLIENT_ERROR: {
    printf("HTTP_CLIENT_ERROR\n");
    client->state = HTTP_CLIENT_DISPOSING;
    break;
  }
  case HTTP_CLIENT_DISPOSING: {
    printf("HTTP_CLIENT_DISPOSING\n");
    http_client_dispose(client);
    break;
  }
  default: {
    printf("HTTP_CLIENT default\n");
    break;
  }
  }
}
void http_client_dispose(HTTP_Client* _Client)
{
  if (!_Client)
    return;

  tcp_client_dispose(&_Client->tcp_client);

  if (_Client->URL != NULL) {
    free((void*)_Client->URL);
    _Client->URL = NULL;
  }

  http_parser_dispose(_Client->req, _Client->resp);
  free(_Client->req);
  _Client->req = NULL;
  free(_Client->resp);
  _Client->resp = NULL;

  if (_Client->decoded_body) {
    free(_Client->decoded_body);
    _Client->decoded_body = NULL;
  }

  if (_Client->task) {
    scheduler_destroy_task(_Client->task);
    _Client->task = NULL;
  }

  if (_Client->request_buffer != NULL) {
    free(_Client->request_buffer);
  }

  free(_Client);
}
