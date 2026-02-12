#include <maestromodules/transport.h>


typedef struct
{

  const char* host;
  const char* port;
  const char* scheme;


  void* client;
  int   timeout_ms;
  bool  use_tls;

} Transport;


int transport_init(Transport* t, const char* host, const char* port, const char* scheme,
                   int timeout_ms);

int transport_connect(Transport* _Transport);

int transport_read(Transport* _Transport, uint8_t* buf, size_t len);

int transport_write(Transport* _Transport, const uint8_t* buf, size_t len);

void transport_dispose(Transport* _Transport);
