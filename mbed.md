
## 0) Översikt: vad ligger var?

### Globalt (en gång)

- **CA bundle bytes** (PEM) i flash/rodata
    
- **global_tls_ca_init()** som parser CA en gång och håller `mbedtls_x509_crt` globalt
    

### Per anslutning (i `Transport`, per request)

- `TCP_Client tcp` (som idag)
    
- `TLS_Client tls` (ny, per anslutning)
    
    - `mbedtls_ssl_context`
        
    - `mbedtls_ssl_config`
        
    - **RNG per anslutning**: `mbedtls_entropy_context`, `mbedtls_ctr_drbg_context`
        
    - host (SNI/verify)
        
    - state (HANDSHAKING/ESTABLISHED)
        

HTTP-klienten fortsätter bara ropa `transport_*`.

---

## 1) Lägg till filer

### 1.1 CA bundle

**Filer:**

- `tls_ca_bundle.h`
    
- `tls_ca_bundle.c`
    

**Innehåll:**

- `g_ca_bundle_pem[]`, `g_ca_bundle_pem_len`
    

> Du kan ha en enda root, eller en bundle.

---

### 1.2 Global CA-store

**Filer:**

- `tls_global_ca.h`
    
- `tls_global_ca.c`
    

**Ansvar:**

- `global_tls_ca_init(ca_pem, len)` parsar en gång
    
- `global_tls_ca_get()` returnerar pekare till den parsade kedjan
    
- `global_tls_ca_deinit()` (valfritt)
    

---

### 1.3 TLS per-connection modul

**Filer:**

- `tls_client.h`
    
- `tls_client.c`
    

**Ansvar:**

- init mbedTLS per anslutning (ssl/conf + per-conn RNG)
    
- koppla BIO till din `TCP_Client`
    
- handshake stegvis (non-block) eller i loop (block)
    
- read/write wrappers
    
- dispose

### 3.1 `Transport` struct

Lägg till:

- `bool use_tls;`
    
- `TLS_Client tls;`
## 4) Implementera TLS-klienten (per anslutning)

### 4.1 BIO-callbacks använder din TCP_Client

**Var:** `tls_client.c`

- `tls_send_cb()` → `tcp_client_write_simple()`
    
- `tls_recv_cb()` → `tcp_client_read_simple()`
    
- mappar `EAGAIN/EWOULDBLOCK/EINTR` till `MBEDTLS_ERR_SSL_WANT_*`
    

### 4.2 `tls_client_init()`: per-connection RNG + global CA

**Var:** `tls_client.c`

Steg:

1. `TLS_GlobalCA* g = global_tls_ca_get();` (måste finnas)
    
2. init `ssl/conf/entropy/ctr_drbg`
    
3. seed per anslutning: `mbedtls_ctr_drbg_seed(...)` (pers gärna inkluderar host)
    
4. `mbedtls_ssl_config_defaults(...)`
    
5. `mbedtls_ssl_conf_rng(... &ctr_drbg ...)` (per-connection RNG)
    
6. `mbedtls_ssl_conf_ca_chain(... &g->ca ...)` (global CA)
    
7. `mbedtls_ssl_conf_authmode(VERIFY_REQUIRED)`
    
8. `mbedtls_ssl_setup(&ssl, &conf)`
    
9. `mbedtls_ssl_set_hostname(&ssl, host)` (SNI+hostname verify)
    
10. `mbedtls_ssl_set_bio(&ssl, &t->tcp, tls_send_cb, tls_recv_cb, NULL)`
    

**Resultat:** TLS-objekt redo att handshake:a.

### 4.3 `tls_client_handshake_step()`: kör handshake stegvis

**Var:** `tls_client.c`

- anropa `mbedtls_ssl_handshake()`
    
- om 0 → verifiera `mbedtls_ssl_get_verify_result()`, sätt state ESTABLISHED, returnera SUCCESS
    
- om WANT_READ/WANT_WRITE → sätt `errno=EAGAIN`, returnera ERR_IN_PROGRESS
    
- annars → ERR_IO
    

### 4.4 `tls_client_read/write()`

**Var:** `tls_client.c`

- `mbedtls_ssl_read/write`
    
- WANT_* → `errno=EAGAIN`, returnera -1 (eller ERR_IN_PROGRESS beroende på din stil)
    
- PEER_CLOSE/EOF → returnera 0
    
- annars → returnera -1 och `errno=EIO`
    

### 4.5 `tls_client_dispose()`

**Var:** `tls_client.c`

- om ESTABLISHED: `mbedtls_ssl_close_notify()` (ignorera fel)
    
- free ssl/conf
    
- free per-conn rng (ctr_drbg/entropy)
    

---

## 5) Transport: exakt steg-för-steg logik

### 5.1 `transport_init()`

**Var:** `transport.c`

Steg:

1. validera args
    
2. sätt `t->host/port/scheme/timeout/use_blocking`
    
3. sätt `t->use_tls` baserat på scheme
    
4. init TCP:
    

- blocking: `tcp_client_blocking_init(...)`
    
- non-block: `tcp_client_init(...)` (SUCCESS eller ERR_IN_PROGRESS)
    

5. Om `use_tls == false`:
    

- returnera resultatet från TCP init
    

6. Om `use_tls == true`:
    

**BLOCKING-läge**

- när TCP init är SUCCESS:
    
    - `tls_client_init(&t->tls, &t->tcp, t->host, true)`
        
    - loopa handshake tills klar eller error:
        
        - vid ERR_IN_PROGRESS fortsätt
            
        - vid ERR_IO → fail
            
    - returnera SUCCESS
        

**NON-BLOCK-läge**

- om TCP init == ERR_IN_PROGRESS:
    
    - returnera ERR_IN_PROGRESS (TLS görs i finish_connect)
        
- om TCP init == SUCCESS:
    
    - initiera TLS (`tls_client_init(...)`)
        
    - returnera ERR_IN_PROGRESS (handshake görs i finish_connect stegvis)
        

> Viktigt: i non-block bör du inte blocka i transport_init.

---

### 5.2 `transport_finish_connect()`

**Var:** `transport.c`

Steg:

1. om blocking: return SUCCESS (du är redan klar efter init)
    
2. om !use_tls:
    
    - return `tcp_client_finish_connect(fd)` (SUCCESS eller ERR_IO)
        
3. om use_tls:
    
    - först `tcp_client_finish_connect(fd)`
        
        - om inte klar än: return ERR_IN_PROGRESS och sätt `errno=EINPROGRESS` eller EAGAIN
            
        - om fel: return ERR_IO
            
    - när TCP är klar:
        
        - om TLS inte initad: `tls_client_init(&t->tls, &t->tcp, t->host, false)`
            
        - kör `tls_client_handshake_step(&t->tls)`
            
            - SUCCESS → return SUCCESS
                
            - ERR_IN_PROGRESS → return ERR_IN_PROGRESS (och errno=EAGAIN)
                
            - ERR_IO → return ERR_IO
                

Detta gör att din HTTP state `WAITING_CONNECT` väntar på både TCP och TLS.

---

### 5.3 `transport_read()` / `transport_write()`

**Var:** `transport.c`

- om `use_tls`: `tls_client_read/write`
    
- annars: `tcp_client_read_simple/write_simple`
    

HTTP-koden är oförändrad.

---

### 5.4 `transport_dispose()`

**Var:** `transport.c`

- om `use_tls`: `tls_client_dispose(&t->tls)`
    
- alltid: `tcp_client_dispose(&t->tcp)`
    

---

## 6) HTTP-klienten: vad ändras (minimalt)

### 6.1 I `http_client_worktask_connecting()`

- Ta bort `if (_Client->tls == false) ... else ...`
    
- Anropa alltid `transport_init(...)` med scheme från URL
    
- Om `ERR_IN_PROGRESS` → `HTTP_CLIENT_WAITING_CONNECT`
    
- Om `SUCCESS` → `HTTP_CLIENT_BUILDING_REQUEST`
    

### 6.2 I send/read states

Du använder redan `transport_write/read` — behåll det.  
Ta bort TLS-brancherna (de behövs inte).

---

## 7) Exakt “ordning” när en HTTPS request körs (non-block)

1. HTTP CONNECTING:
    
    - `transport_init()` → startar TCP connect → return ERR_IN_PROGRESS
        
2. HTTP WAITING_CONNECT:
    
    - `transport_finish_connect()`
        
        - när TCP klar: init TLS (om ej redan)
            
        - kör handshake step
            
        - vill läsa/skriva mer → ERR_IN_PROGRESS (errno=EAGAIN)
            
        - när handshake klar → SUCCESS
            
3. HTTP BUILDING_REQUEST
    
4. HTTP SENDING_REQUEST: `transport_write()` (TLS)
    
5. HTTP READING_*: `transport_read()` (TLS)
    
6. DISPOSING: `transport_dispose()` → close_notify + close(fd)
    

---

## 8) Checklist: du är klar när detta stämmer

-  `global_tls_ca_init()` körs en gång vid start
    
-  `Transport` har `TLS_Client tls;`
    
-  `transport_finish_connect()` driver TLS handshake stegvis
    
-  HTTP-koden har inga TLS-brancher kvar (bara transport)
    
-  HTTPS fungerar i både blocking och non-block
    

---
