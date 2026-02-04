#include "unity.h"
#include "cmock.h"
#include "Mocktcp_client.h"
#include "maestromodules/http_client.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* --- CMOCK GLOBAL VARIABLES --- */
int   GlobalExpectCount;
int   GlobalVerifyOrder;
char* GlobalOrderError;

/* --- STUBS --- */

/**
 * Standard simple write stub.
 */
int tcp_write_simple_stub(TCP_Client* client, const uint8_t* buf, int len, int num_calls)
{
  (void)client;
  (void)buf;
  (void)num_calls;
  return len;
}

/**
 * Dynamic memory allocation stub for TCP data.
 */
ssize_t tcp_realloc_stub(TCP_Data* Data, void* input, size_t size, int num_calls)
{
  (void)num_calls;
  void* new_ptr = realloc(Data->addr, Data->size + size + 1);
  if (!new_ptr)
    return -1;
  Data->addr = (uint8_t*)new_ptr;
  memcpy(Data->addr + Data->size, input, size);
  Data->size += (ssize_t)size;
  Data->addr[Data->size] = 0;
  return (ssize_t)size;
}

/**
 * Valid HTTP Chunked Encoding stub.
 */
int tcp_read_valid_chunked_stub(TCP_Client* client, uint8_t* buf, int buf_len, int num_calls)
{
  (void)client;
  (void)buf_len;
  const char* chunks[] = {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", "5\r\nHELLO\r\n",
                          "6\r\nWORLD!\r\n", "0\r\n\r\n"};
  if (num_calls < 4) {
    memcpy(buf, chunks[num_calls], strlen(chunks[num_calls]));
    return (int)strlen(chunks[num_calls]);
  }
  return 0;
}

/**
 * Standard Body fragmentation stub (Content-Length: 10).
 */
int tcp_read_fragmented_body_stub(TCP_Client* client, uint8_t* buf, int buf_len, int num_calls)
{
  (void)client;
  (void)buf_len;
  const char* parts[] = {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n", "ABCDE", "FGHIJ"};
  if (num_calls < 3) {
    memcpy(buf, parts[num_calls], strlen(parts[num_calls]));
    return (int)strlen(parts[num_calls]);
  }
  return 0;
}

/**
 * Refined non-blocking stub.
 * Delivers headers and body in separate calls and uses EAGAIN to test retry logic.
 */
int tcp_read_non_blocking_retry_stub(TCP_Client* client, uint8_t* buf, int buf_len, int num_calls)
{
  (void)client;
  (void)buf_len;
  switch (num_calls) {
  case 0:
  case 1:
    errno = EAGAIN;
    return -1;
  case 2: // Headers only
  {
    const char* h = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n";
    memcpy(buf, h, strlen(h));
    return (int)strlen(h);
  }
  case 3: // Body only
    memcpy(buf, "BYE!", 4);
    return 4;
  default:
    return 0;
  }
}

/**
 * Stub that simulates network lag (EAGAIN) right in the middle of the body.
 */
int tcp_read_unstable_chunks_stub(TCP_Client* client, uint8_t* buf, int buf_len, int num_calls)
{
  (void)client;
  (void)buf_len;
  switch (num_calls) {
  case 0: // Headers
  {
    const char* h = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n";
    memcpy(buf, h, strlen(h));
    return (int)strlen(h);
  }
  case 1: // Partial body
    memcpy(buf, "PI", 2);
    return 2;
  case 2: // Network delay mid-body
    errno = EAGAIN;
    return -1;
  case 3: // Rest of body
    memcpy(buf, "NG", 2);
    return 2;
  default:
    return 0;
  }
}

/* --- SETUP & TEARDOWN --- */

void setUp(void)
{
  GlobalExpectCount = 0;
  GlobalVerifyOrder = 0;
  GlobalOrderError  = NULL;
  errno             = 0;
  Mocktcp_client_Init();
}

void tearDown(void)
{
  Mocktcp_client_Verify();
  Mocktcp_client_Destroy();
}

/* --- TEST CASES --- */

void test_http_blocking_get_should_fail_if_tcp_connect_fails(void)
{
  http_data out = {0};
  tcp_client_blocking_init_ExpectAndReturn(NULL, "example.com", "80", 1000, -20);
  tcp_client_blocking_init_IgnoreArg__Client();
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_get("http://example.com", &out, 1000);
  TEST_ASSERT_EQUAL_INT(-20, result);
}

void test_http_get_chunked_success(void)
{
  http_data out = {0};
  tcp_client_write_simple_StubWithCallback(tcp_write_simple_stub);
  tcp_client_read_simple_StubWithCallback(tcp_read_valid_chunked_stub);
  tcp_client_realloc_data_StubWithCallback(tcp_realloc_stub);

  tcp_client_blocking_init_IgnoreAndReturn(0);
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_get("http://chunked.com", &out, 1000);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(11, out.size);
  TEST_ASSERT_EQUAL_MEMORY("HELLOWORLD!", out.data, 11);

  if (out.data)
    free(out.data);
}

void test_http_get_fragmented_content_length(void)
{
  http_data out = {0};
  tcp_client_write_simple_StubWithCallback(tcp_write_simple_stub);
  tcp_client_read_simple_StubWithCallback(tcp_read_fragmented_body_stub);
  tcp_client_realloc_data_StubWithCallback(tcp_realloc_stub);

  tcp_client_blocking_init_IgnoreAndReturn(0);
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_get("http://fragmented.com", &out, 1000);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(10, out.size);
  TEST_ASSERT_EQUAL_MEMORY("ABCDEFGHIJ", out.data, 10);

  if (out.data)
    free(out.data);
}

void test_http_get_non_blocking_retries(void)
{
  http_data out = {0};
  tcp_client_write_simple_StubWithCallback(tcp_write_simple_stub);
  tcp_client_read_simple_StubWithCallback(tcp_read_non_blocking_retry_stub);
  tcp_client_realloc_data_StubWithCallback(tcp_realloc_stub);

  tcp_client_blocking_init_IgnoreAndReturn(0);
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_get("http://retry.com", &out, 1000);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(4, out.size);
  TEST_ASSERT_EQUAL_MEMORY("BYE!", out.data, 4);

  if (out.data)
    free(out.data);
}

void test_http_get_resilience_mid_payload(void)
{
  http_data out = {0};
  tcp_client_write_simple_StubWithCallback(tcp_write_simple_stub);
  tcp_client_read_simple_StubWithCallback(tcp_read_unstable_chunks_stub);
  tcp_client_realloc_data_StubWithCallback(tcp_realloc_stub);

  tcp_client_blocking_init_IgnoreAndReturn(0);
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_get("http://unstable.com", &out, 1000);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(4, out.size);
  TEST_ASSERT_EQUAL_MEMORY("PING", out.data, 4);

  if (out.data)
    free(out.data);
}

void test_http_post_large_data(void)
{
  http_data out           = {0};
  size_t    large_size    = 4096;
  uint8_t*  large_payload = malloc(large_size);
  memset(large_payload, 'Z', large_size);
  http_data in = {.data = large_payload, .size = (ssize_t)large_size};

  tcp_client_write_simple_StubWithCallback(tcp_write_simple_stub);
  tcp_client_read_simple_StubWithCallback(tcp_read_valid_chunked_stub);
  tcp_client_realloc_data_StubWithCallback(tcp_realloc_stub);

  tcp_client_blocking_init_IgnoreAndReturn(0);
  tcp_client_dispose_Expect(NULL);
  tcp_client_dispose_IgnoreArg__Client();

  int result = http_blocking_post("http://post-large.com", &in, &out, 2000);

  TEST_ASSERT_EQUAL_INT(0, result);

  free(large_payload);
  if (out.data)
    free(out.data);
}

/* --- MAIN --- */

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_http_blocking_get_should_fail_if_tcp_connect_fails);
  RUN_TEST(test_http_get_chunked_success);
  RUN_TEST(test_http_get_fragmented_content_length);
  RUN_TEST(test_http_get_non_blocking_retries);
  RUN_TEST(test_http_get_resilience_mid_payload);
  RUN_TEST(test_http_post_large_data);
  return UNITY_END();
}
