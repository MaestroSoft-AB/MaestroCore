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

static const char* default_url(void) { return "http://www.google.com"; }
static const char* default_tls_url(void)
{
  return "https://api.open-meteo.com/v1/"
         "forecast?latitude=59.33&longitude=18.06&current_weather=true";
}


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
// static int run_blocking_tls(int timeout_ms, const char* out_path, FILE* logf)
// {
//   return run_blocking(default_tls_url(), timeout_ms, out_path, logf);
// }
//
// static int run_nonblocking_tls(int timeout_ms, const char* out_path, FILE* logf)
// {
//   return run_nonblocking(default_tls_url(), timeout_ms, out_path, logf);
// }


/* ------------------------------------------------------------ */

// static void make_paths(const char* prefix, char* out_blocking, size_t obsz, char*
// out_nonblocking,
//                        size_t onbsz, char* out_log, size_t olsz)
// {
//   snprintf(out_blocking, obsz, "%s_blocking.bin", prefix);
//   snprintf(out_nonblocking, onbsz, "%s_nonblocking.bin", prefix);
//   snprintf(out_log, olsz, "%s.log", prefix);
// }

int main(int argc, char** argv)
{
  /* -------------------------------------------------------- */
  /* 1. Init global CA (required once for TLS)               */
  /* -------------------------------------------------------- */
  int globalres = global_tls_ca_init();
  printf("global_tls_ca_init() = %d\n", globalres);

  if (globalres != 0) {
    printf("Global TLS CA init failed — HTTPS tests will fail.\n");
  }

  /* -------------------------------------------------------- */
  /* 2. Parse arguments                                       */
  /* -------------------------------------------------------- */

  const char* http_url  = default_url();     // http://
  const char* https_url = default_tls_url(); // https://

  int         timeout_ms = 10000;
  const char* prefix     = "http_out";

  if (argc >= 2)
    timeout_ms = atoi(argv[1]);
  if (argc >= 3)
    prefix = argv[2];

  /* -------------------------------------------------------- */
  /* 3. Prepare output paths                                  */
  /* -------------------------------------------------------- */

  char path_http_blocking[512];
  char path_http_nonblocking[512];
  char path_https_blocking[512];
  char path_https_nonblocking[512];
  char path_log[512];

  snprintf(path_http_blocking, sizeof(path_http_blocking), "%s_http_blocking.bin", prefix);

  snprintf(path_http_nonblocking, sizeof(path_http_nonblocking), "%s_http_nonblocking.bin", prefix);

  snprintf(path_https_blocking, sizeof(path_https_blocking), "%s_https_blocking.bin", prefix);

  snprintf(path_https_nonblocking, sizeof(path_https_nonblocking), "%s_https_nonblocking.bin",
           prefix);

  snprintf(path_log, sizeof(path_log), "%s.log", prefix);

  FILE* logf = open_log(path_log);
  if (!logf)
    return 2;

  /* -------------------------------------------------------- */
  /* 4. Print header                                          */
  /* -------------------------------------------------------- */

  printf("\n===== MaestroCore HTTP/TLS Manual Test =====\n");
  printf("Timeout   = %d ms\n", timeout_ms);
  printf("Prefix    = %s\n", prefix);
  printf("Log file  = %s\n\n", path_log);

  fprintf(logf, "Manual HTTP/TLS test\n");
  fprintf(logf, "Timeout = %d ms\n", timeout_ms);
  fprintf(logf, "Prefix  = %s\n", prefix);
  fflush(logf);

  /* -------------------------------------------------------- */
  /* 5. HTTP TESTS (plain TCP)                                */
  /* -------------------------------------------------------- */

  printf("===== TESTING HTTP (TCP) =====\n");

  fprintf(logf, "\n===== HTTP (TCP) =====\n");
  fflush(logf);

  run_blocking(http_url, timeout_ms, path_http_blocking, logf);

  run_nonblocking(http_url, timeout_ms, path_http_nonblocking, logf);

  /* -------------------------------------------------------- */
  /* 6. HTTPS TESTS (TLS)                                     */
  /* -------------------------------------------------------- */

  printf("\n===== TESTING HTTPS (TLS) =====\n");

  fprintf(logf, "\n===== HTTPS (TLS) =====\n");
  fflush(logf);

  run_blocking(https_url, timeout_ms, path_https_blocking, logf);

  run_nonblocking(https_url, timeout_ms, path_https_nonblocking, logf);

  /* -------------------------------------------------------- */
  /* 7. Done                                                  */
  /* -------------------------------------------------------- */

  printf("\nTests complete.\n");
  printf("Check output files:\n");
  printf("  %s\n", path_http_blocking);
  printf("  %s\n", path_http_nonblocking);
  printf("  %s\n", path_https_blocking);
  printf("  %s\n", path_https_nonblocking);
  printf("  %s\n\n", path_log);

  fclose(logf);
  return 0;
}
