#include <maestromodules/http_client.h>
#include <maestromodules/scheduler.h>
#include <maestromodules/tls_ca_bundle.h>
#include <maestromodules/tls_global_ca.h>
#include <maestromodules/tls_client.h>
#include <maestromodules/transport.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t SystemMonotonicMS(void);

static int transport_bio_send(void* ctx, const unsigned char* buf, size_t len)
{
  TCP_Client* tcp = (TCP_Client*)ctx;
  int         ret = tcp_client_write_simple(tcp, buf, (int)len);
  if (ret < 0)
    return -1; // tls_client kommer mappa errno om det behövs
  return ret;
}

static int transport_bio_recv(void* ctx, unsigned char* buf, size_t len)
{
  TCP_Client* tcp = (TCP_Client*)ctx;
  int         ret = tcp_client_read_simple(tcp, buf, (int)len);
  return ret; // errno sätts av recv()
}


// If scheduler_get_task_count() exists in your scheduler.c (you pasted it), but is not in the
// header, this forward declaration makes the test compile without touching your library headers.
int scheduler_get_task_count(void);

static const char* default_url(void) { return "http://httpbin.org/get"; }

/* ------------------------------------------------------------ */
/* File helpers                                                 */
/* ------------------------------------------------------------ */
static int write_blob_to_file(const char* path, const uint8_t* data, size_t len)
{
  FILE* f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "fopen(%s) failed: %s\n", path, strerror(errno));
    return -1;
  }

  if (len > 0 && data) {
    size_t wr = fwrite(data, 1, len, f);
    if (wr != len) {
      fprintf(stderr, "fwrite(%s) short write: %zu/%zu\n", path, wr, len);
      fclose(f);
      return -1;
    }
  }

  fclose(f);
  return 0;
}

static FILE* open_log(const char* path)
{
  FILE* f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "fopen(%s) failed: %s\n", path, strerror(errno));
    return NULL;
  }
  return f;
}

/* ------------------------------------------------------------ */
/* Blocking test                                                */
/* ------------------------------------------------------------ */
static int run_blocking(const char* url, int timeout_ms, const char* out_path, FILE* logf)
{
  fprintf(logf, "\n=== BLOCKING ===\n");
  fflush(logf);

  http_data out;
  memset(&out, 0, sizeof(out));

  int rc = http_blocking_get(url, &out, timeout_ms);

  fprintf(logf, "blocking rc = %d\n", rc);
  if (rc == 0) {
    fprintf(logf, "blocking bytes = %zd\n", out.size);
    fflush(logf);

    if (out.addr && out.size > 0) {
      (void)write_blob_to_file(out_path, out.addr, (size_t)out.size);
      fprintf(logf, "blocking wrote: %s\n", out_path);
    } else {
      fprintf(logf, "blocking: no body captured\n");
    }
  }
  fflush(logf);

  free(out.addr);
  out.addr = NULL;
  out.size = 0;

  return rc;
}

/* ------------------------------------------------------------ */
/* Nonblocking test                                             */
/* ------------------------------------------------------------ */
typedef struct
{
  volatile int done;
  char*        response; // owned by test (malloc'd by client)
  FILE*        logf;
} NBContext;

static void on_success(void* ctx, char** resp_ptr)
{
  NBContext* nb = (NBContext*)ctx;

  if (resp_ptr && *resp_ptr) {
    nb->response = *resp_ptr; // take ownership
    *resp_ptr    = NULL;
  }

  nb->done = 1;

  fprintf(nb->logf, "nonblocking: SUCCESS callback fired\n");
  fflush(nb->logf);
}

static int run_nonblocking(const char* url, int timeout_ms, const char* out_path, FILE* logf)
{
  fprintf(logf, "\n=== NONBLOCKING ===\n");
  fflush(logf);

  scheduler_init();

  // IMPORTANT:
  // Your http_client_dispose() frees(_Client).
  // So we must allocate the client on heap.
  HTTP_Client* client = (HTTP_Client*)calloc(1, sizeof(*client));
  if (!client) {
    fprintf(logf, "nonblocking: calloc client failed\n");
    fflush(logf);
    scheduler_dispose();
    return -1;
  }

  NBContext nb;
  memset(&nb, 0, sizeof(nb));
  nb.logf = logf;

  // We pass a dummy pointer to match signature; callback takes ownership of response buffer.
  char* response_out_dummy = NULL;

  int rc = http_client_initiate(client, url, HTTP_GET, on_success, &nb, &response_out_dummy);
  if (rc != 0) {
    fprintf(logf, "nonblocking: init failed rc=%d\n", rc);
    fflush(logf);
    free(client);
    scheduler_dispose();
    return rc;
  }

  uint64_t start = SystemMonotonicMS();

  while (!nb.done) {
    uint64_t now = SystemMonotonicMS();

    // If the client disposes itself (ERROR->DISPOSING->dispose), the scheduler task disappears.
    // Without an on_error callback, nb.done would never flip, so we must break on task_count == 0.
    int tasks = scheduler_get_task_count();
    if (tasks == 0) {
      fprintf(logf, "nonblocking: scheduler has no tasks (client likely disposed/error)\n");
      fflush(logf);
      break;
    }

    if (timeout_ms > 0 && (now - start) > (uint64_t)timeout_ms) {
      fprintf(logf, "nonblocking: TIMEOUT after %d ms\n", timeout_ms);
      fflush(logf);
      break;
    }

    scheduler_work(now);
  }

  // If callback didn't steal, free dummy (usually stays NULL in your implementation).
  free(response_out_dummy);
  response_out_dummy = NULL;

  if (nb.response) {
    size_t len = strlen(nb.response);
    fprintf(logf, "nonblocking bytes = %zu\n", len);
    fflush(logf);

    (void)write_blob_to_file(out_path, (const uint8_t*)nb.response, len);
    fprintf(logf, "nonblocking wrote: %s\n", out_path);
    fflush(logf);
  } else {
    fprintf(logf, "nonblocking: no response captured\n");
    fflush(logf);
  }

  free(nb.response);
  nb.response = NULL;

  // Dispose scheduler. Do NOT free(client) here; if the client reached DISPOSING it probably freed
  // itself. If it didn't, you currently have no safe way to know without changing library code.
  scheduler_dispose();

  return 0;
}

/* ------------------------------------------------------------ */

static void make_paths(const char* prefix, char* out_blocking, size_t obsz, char* out_nonblocking,
                       size_t onbsz, char* out_log, size_t olsz)
{
  snprintf(out_blocking, obsz, "%s_blocking.bin", prefix);
  snprintf(out_nonblocking, onbsz, "%s_nonblocking.bin", prefix);
  snprintf(out_log, olsz, "%s.log", prefix);
}

int main(int argc, char** argv)
{
  int globalres = global_tls_ca_init();
  printf("value of globalres %d\n", globalres);


  TLS_Client tls;
  Transport  Transport;

  TLS_BIO bio = {.io_ctx = &Transport.tcp, .send = transport_bio_send, .recv = transport_bio_recv};

  int ret = tls_client_init(&tls, "example.com", &bio);

  printf("TLS_CLIENT_INIT RESULT: %d\n", ret);


  const char* url        = default_url();
  int         timeout_ms = 10000;
  const char* prefix     = "http_out";

  if (argc >= 2)
    url = argv[1];
  if (argc >= 3)
    timeout_ms = atoi(argv[2]);
  if (argc >= 4)
    prefix = argv[3];

  char path_blocking[512];
  char path_nonblocking[512];
  char path_log[512];
  make_paths(prefix, path_blocking, sizeof(path_blocking), path_nonblocking,
             sizeof(path_nonblocking), path_log, sizeof(path_log));

  FILE* logf = open_log(path_log);
  if (!logf)
    return 2;

  fprintf(logf, "Manual HTTP test (minimal)\n");
  fprintf(logf, "URL      = %s\n", url);
  fprintf(logf, "timeout  = %d ms\n", timeout_ms);
  fprintf(logf, "outputs  = %s, %s, %s\n", path_blocking, path_nonblocking, path_log);
  fflush(logf);

  // Small stdout header so you immediately see where files are written
  printf("Manual HTTP test (minimal)\n");
  printf("URL      = %s\n", url);
  printf("timeout  = %d ms\n", timeout_ms);
  printf("outputs  = %s, %s, %s\n", path_blocking, path_nonblocking, path_log);

  (void)run_blocking(url, timeout_ms, path_blocking, logf);
  (void)run_nonblocking(url, 0, path_nonblocking, logf);

  fclose(logf);
  return 0;
}
